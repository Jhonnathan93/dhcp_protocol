#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define BUFFER_SIZE 1024

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
    uint32_t giaddr; // Este campo es el que indica al servidor que el paquete pasa por el relay
    uint8_t chaddr[16];
    char sname[64];
    char file[128];
    uint32_t magic_cookie;
    uint8_t options[312];
};

int main() {
    int relay_sock;
    struct sockaddr_in relay_addr, client_addr, server_addr;
    struct dhcp_packet dhcp_request;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t recv_len;

    // Crear socket UDP para el relay
    if ((relay_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket del relay");
        exit(1);
    }

    // Configurar la dirección del relay para escuchar en cualquier interfaz
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Escuchar en cualquier interfaz
    relay_addr.sin_port = htons(1067);  // Escuchar en el puerto DHCP

    // Enlazar el socket a la dirección del relay
    if (bind(relay_sock, (struct sockaddr *)&relay_addr, sizeof(relay_addr)) < 0) {
        perror("Error al enlazar socket del relay");
        close(relay_sock);
        exit(1);
    }


    // Ciclo infinito para recibir y reenviar paquetes
    while (1) {
        printf("Esperando paquete DHCP del cliente...\n");
        // Recibir paquete DHCP Discover del cliente
        int recv_len = recvfrom(relay_sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("Error al recibir paquete del cliente");
        } else {
            printf("Paquete recibido con tamaño: %d bytes\n", recv_len);
        }


        printf("Paquete recibido del cliente con xid: %u\n", ntohl(dhcp_request.xid));


        // Configurar la dirección del servidor DHCP
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DHCP_SERVER_PORT);  // Puerto 67 del servidor DHCP
        server_addr.sin_addr.s_addr = inet_addr("192.168.0.1");  // IP del servidor DHCP

        // Modificar el campo giaddr (Gateway IP Address) para reenviar al servidor DHCP
        dhcp_request.giaddr = inet_addr("192.168.0.2");  // IP del relay (gateway)
        printf("Reenviando paquete al servidor DHCP con giaddr: %s\n", inet_ntoa(*(struct in_addr *)&dhcp_request.giaddr));


        // Reenviar el paquete al servidor DHCP
        if (sendto(relay_sock, &dhcp_request, recv_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error al reenviar paquete al servidor DHCP");
            continue;
        }

        printf("Paquete reenviado al servidor DHCP.\n");
    }

    close(relay_sock);
    return 0;
}
