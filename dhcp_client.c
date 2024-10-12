#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

#define DHCP_DISCOVER 1
#define DHCP_REQUEST 3
#define DHCP_MAGIC_COOKIE 0x63825363
#define LEASE_TIME 60   // Lease de ejemplo en segundos

struct dhcp_packet {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    char sname[64];
    char file[128];
    uint32_t magic_cookie;
    uint8_t options[312];
};

void construct_dhcp_discover(struct dhcp_packet *packet) {
    memset(packet, 0, sizeof(struct dhcp_packet));
    
    packet->op = 1;  // Cliente -> Servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // ID de transacción
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast
    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = 0;
    packet->giaddr = 0;
    packet->chaddr[0] = 0x00;
    packet->chaddr[1] = 0x0c;
    packet->chaddr[2] = 0x29;
    packet->chaddr[3] = 0x3e;
    packet->chaddr[4] = 0x53;
    packet->chaddr[5] = 0xf7;

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_DISCOVER;
    packet->options[3] = 255;  // Fin de opciones
}

void construct_dhcp_request(struct dhcp_packet *packet, uint32_t offered_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 1;  // Cliente -> Servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // ID de transacción
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast
    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = 0;
    packet->giaddr = 0;
    memcpy(packet->chaddr, (uint8_t[]){0x00, 0x0c, 0x29, 0x3e, 0x53, 0xf7}, 6);  // MAC

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;
    packet->options[2] = DHCP_REQUEST;
    packet->options[3] = 50;  // Requested IP Address
    packet->options[4] = 4;   // Longitud
    memcpy(&packet->options[5], &offered_ip, 4);  // IP ofrecida
    packet->options[9] = 255;  // Fin de opciones
}

void renew_lease(int sock, struct sockaddr_in *server_addr, uint32_t offered_ip) {
    struct dhcp_packet dhcp_request;
    socklen_t server_addr_len = sizeof(*server_addr);

    // Enviar DHCP Request para renovar el lease
    construct_dhcp_request(&dhcp_request, offered_ip);
    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)server_addr, server_addr_len) < 0) {
        perror("Error al enviar DHCP Request para renovación");
        close(sock);
        exit(1);
    }
    printf("DHCP Request enviado para renovación del lease.\n");
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    struct dhcp_packet dhcp_discover, dhcp_offer;
    socklen_t server_addr_len = sizeof(server_addr);
    uint32_t offered_ip;
    time_t lease_start;
    int lease_duration = LEASE_TIME;

    // Crear socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        return 1;
    }

    // Configurar la dirección del servidor DHCP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);  // Broadcast

    // Habilitar el uso de broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Error al habilitar broadcast");
        close(sock);
        return 1;
    }

    // Enviar DHCP Discover
    construct_dhcp_discover(&dhcp_discover);
    if (sendto(sock, &dhcp_discover, sizeof(dhcp_discover), 0, (struct sockaddr *)&server_addr, server_addr_len) < 0) {
        perror("Error al enviar DHCP Discover");
        close(sock);
        return 1;
    }

    printf("DHCP Discover enviado.\n");

    // Esperar DHCP Offer
    if (recvfrom(sock, &dhcp_offer, sizeof(dhcp_offer), 0, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("Error al recibir DHCP Offer");
        close(sock);
        return 1;
    }

    offered_ip = dhcp_offer.yiaddr;
    printf("DHCP Offer recibido: IP ofrecida = %s\n", inet_ntoa(*(struct in_addr *)&offered_ip));
    
    // Registrar el inicio del lease
    lease_start = time(NULL);

    // Mientras no expire el lease, renovamos cuando queden pocos segundos
    while (difftime(time(NULL), lease_start) < lease_duration) {
        sleep(lease_duration / 2);  // Dormir hasta la mitad del lease

        // Renovar el lease antes de que expire
        renew_lease(sock, &server_addr, offered_ip);
    }

    printf("Lease expirado.\n");

    close(sock);
    return 0;
}
