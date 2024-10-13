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

void construct_dhcp_request(struct dhcp_packet *packet, uint32_t offered_ip) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 1;
    packet->htype = 1;
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(0x12345678);
    packet->secs = 0;
    packet->flags = 0;
    packet->ciaddr = 0;
    packet->yiaddr = offered_ip;
    packet->siaddr = 0;
    packet->giaddr = 0;
    memcpy(packet->chaddr, "\x00\x11\x22\x33\x44\x55", 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    packet->options[0] = 53;
    packet->options[1] = 1;
    packet->options[2] = DHCP_REQUEST;
    packet->options[3] = 255;
}

void renew_dhcp_lease(int sock, struct sockaddr_in *server_addr) {
    struct dhcp_packet dhcp_request;
    socklen_t server_addr_len = sizeof(*server_addr);

    printf("Renovando lease...\n");

    construct_dhcp_request(&dhcp_request, 0);  // Enviar solicitud de renovación

    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)server_addr, server_addr_len) < 0) {
        perror("Error al enviar DHCP Request");
    }

    // Esperar respuesta del servidor
    struct dhcp_packet dhcp_ack;
    if (recvfrom(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)server_addr, &server_addr_len) < 0) {
        perror("Error al recibir DHCP ACK");
    } else {
        printf("Lease renovado.\n");
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    time_t lease_start;
    int lease_duration = LEASE_TIME;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);
    server_addr.sin_addr.s_addr = inet_addr("192.168.0.1");

    printf("Cliente DHCP iniciando...\n");

    // Simulación de lease inicial
    lease_start = time(NULL);

    while (1) {
        // Simular lease por cierto tiempo
        if (difftime(time(NULL), lease_start) > lease_duration / 2) {
            renew_dhcp_lease(sock, &server_addr);
            lease_start = time(NULL);  // Reset del lease al renovarlo
        }

        sleep(5);  // Pausa para simular la ejecución continua
    }

    close(sock);
    return 0;
}
