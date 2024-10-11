#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#define DHCP_OFFER 2
#define DHCP_ACK 5
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
    uint32_t yiaddr;   // 4 bytes (IP ofrecida o asignada)
    uint32_t siaddr;   // 4 bytes
    uint32_t giaddr;   // 4 bytes
    uint8_t chaddr[16]; // 16 bytes
    char sname[64];    // 64 bytes
    char file[128];    // 128 bytes
    uint32_t magic_cookie; // 4 bytes
    uint8_t options[312];  // Opciones DHCP
};

void construct_dhcp_offer(struct dhcp_packet *packet, uint32_t offered_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;  // Servidor -> Cliente
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // ID de transacción
    packet->secs = 0;
    packet->flags = 0;
    packet->ciaddr = 0;
    packet->yiaddr = offered_ip;  // IP ofrecida
    packet->siaddr = inet_addr("192.168.0.1");  // IP del servidor DHCP
    packet->giaddr = 0;
    // Dirección MAC del cliente
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
    packet->options[2] = DHCP_OFFER;  // DHCP Offer
    packet->options[3] = 255;  // Fin de opciones
}

void construct_dhcp_ack(struct dhcp_packet *packet, uint32_t assigned_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;  // Servidor -> Cliente
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // ID de transacción
    packet->secs = 0;
    packet->flags = 0;
    packet->ciaddr = 0;
    packet->yiaddr = assigned_ip;  // IP asignada
    packet->siaddr = inet_addr("192.168.0.1");  // IP del servidor DHCP
    packet->giaddr = 0;
    memcpy(packet->chaddr, (uint8_t[]){0x00, 0x0c, 0x29, 0x3e, 0x53, 0xf7}, 6);  // MAC del cliente

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_ACK;  // DHCP ACK
    packet->options[3] = 255;  // Fin de opciones
}

int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    struct dhcp_packet dhcp_request, dhcp_offer, dhcp_ack;
    socklen_t client_addr_len = sizeof(client_addr);
    uint32_t offered_ip = inet_addr("192.168.0.100");  // IP a ofrecer
    uint32_t assigned_ip = inet_addr("192.168.0.100"); // IP asignada

    // Crear socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        return 1;
    }

    // Configurar dirección del servidor DHCP
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP del servidor
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Escuchar en cualquier IP

    // Enlazar el socket a la dirección del servidor
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar socket");
        close(sock);
        return 1;
    }

    printf("Servidor DHCP en ejecución, esperando solicitudes...\n");

    while (1) {
        // Esperar DHCP Request del cliente
        if (recvfrom(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
            perror("Error al recibir DHCP Request");
            continue;
        }

        printf("DHCP Request recibido del cliente.\n");

        // Enviar DHCP ACK al cliente
        construct_dhcp_ack(&dhcp_ack, assigned_ip);
        if (sendto(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
            perror("Error al enviar DHCP ACK");
            continue;
        }

        printf("DHCP ACK enviado: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&assigned_ip));
    }

    close(sock);
    return 0;
}
