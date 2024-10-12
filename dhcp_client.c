#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#define DHCP_DISCOVER 1
#define DHCP_REQUEST 3
#define DHCP_MAGIC_COOKIE 0x63825363

struct dhcp_packet {
    uint8_t op;        // 1 byte
    uint8_t htype;     // 1 byte
    uint8_t hlen;      // 1 byte
    uint8_t hops;      // 1 byte
    uint32_t xid;      // 4 bytes
    uint16_t secs;     // 2 bytes
    uint16_t flags;    // 2 bytes
    uint32_t ciaddr;   // 4 bytes
    uint32_t yiaddr;   // 4 bytes
    uint32_t siaddr;   // 4 bytes
    uint32_t giaddr;   // 4 bytes
    uint8_t chaddr[16]; // 16 bytes
    char sname[64];    // 64 bytes
    char file[128];    // 128 bytes
    uint32_t magic_cookie; // 4 bytes
    uint8_t options[312];  // Opciones DHCP
};

void construct_dhcp_discover(struct dhcp_packet *packet) {
    memset(packet, 0, sizeof(struct dhcp_packet));
    
    packet->op = 1;  // Cliente -> Servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // ID de transacción (aleatorio)
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast
    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = 0;
    packet->giaddr = 0;
    // Dirección MAC del cliente (ficticia para este ejemplo)
    packet->chaddr[0] = 0x00;
    packet->chaddr[1] = 0x0c;
    packet->chaddr[2] = 0x29;
    packet->chaddr[3] = 0x3e;
    packet->chaddr[4] = 0x53;
    packet->chaddr[5] = 0xf7;

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_DISCOVER;  // DHCP Discover
    packet->options[3] = 255;  // Fin de opciones
}

void construct_dhcp_request(struct dhcp_packet *packet, uint32_t offered_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 1;  // Cliente -> Servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // Usamos el mismo ID de transacción
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast
    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = 0;
    packet->giaddr = 0;
    memcpy(packet->chaddr, (uint8_t[]){0x00, 0x0c, 0x29, 0x3e, 0x53, 0xf7}, 6);  // MAC

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_REQUEST;  // DHCP Request
    packet->options[3] = 50;  // Requested IP Address
    packet->options[4] = 4;   // Longitud
    memcpy(&packet->options[5], &offered_ip, 4);  // IP ofrecida
    packet->options[9] = 255;  // Fin de opciones
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    struct dhcp_packet dhcp_discover, dhcp_offer, dhcp_request;
    socklen_t server_addr_len = sizeof(server_addr);
    uint32_t offered_ip;

    // Crear socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configurar dirección del servidor DHCP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP del servidor
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);  // Broadcast

    // Permitir el uso de broadcast en el socket
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
    
    // Enviar DHCP Request
    construct_dhcp_request(&dhcp_request, offered_ip);
    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&server_addr, server_addr_len) < 0) {
        perror("Error al enviar DHCP Request");
        close(sock);
        return 1;
    }

    printf("DHCP Request enviado.\n");

    close(sock);
    return 0;
}