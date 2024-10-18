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

// Función para obtener el tipo de mensaje DHCP
uint8_t get_dhcp_message_type(struct dhcp_packet *packet) {
    for (int i = 0; i < sizeof(packet->options); i++) {
        if (packet->options[i] == 53) { // Buscar la opción DHCP Message Type
            return packet->options[i + 2]; // El valor del tipo de mensaje está dos posiciones después
        }
    }
    return 0;
}

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

        // Recibir paquete DHCP Discover o Request del cliente
        int recv_len = recvfrom(relay_sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("Error al recibir paquete del cliente");
            continue;
        }

        printf("Paquete recibido con tamaño: %d bytes\n", recv_len);
        printf("Paquete recibido del cliente con xid: %u\n", ntohl(dhcp_request.xid));

        // Identificar el tipo de paquete DHCP
        uint8_t dhcp_message_type = get_dhcp_message_type(&dhcp_request);

        // // Manejar el paquete según su tipo
        // switch (dhcp_message_type) {
        //     case DHCP_DISCOVER:
        //         printf("El paquete es un DHCP Discover\n");
        //         break;

        //     case DHCP_REQUEST:
        //         printf("El paquete es un DHCP Request\n");
        //         break;

        //     case DHCP_OFFER:
        //         printf("El paquete es un DHCP Offer\n");
        //         break;

        //     case DHCP_ACK:
        //         printf("El paquete es un DHCP ACK\n");
        //         break;

        //     default:
        //         printf("Paquete DHCP desconocido o no soportado.\n");
        //         continue;
        // }

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

        // Esperar la respuesta del servidor DHCP (Offer o ACK)
        printf("Esperando respuesta del servidor DHCP...\n");
        int resp_len = recvfrom(relay_sock, &dhcp_request, sizeof(dhcp_request), 0, NULL, NULL);
        if (resp_len < 0) {
            perror("Error al recibir respuesta del servidor DHCP");
            continue;
        }

        printf("Respuesta del servidor DHCP recibida, tamaño: %d bytes\n", resp_len);

        // Identificar el tipo de mensaje recibido del servidor (Offer o ACK)
        dhcp_message_type = get_dhcp_message_type(&dhcp_request);

        // switch (dhcp_message_type) {
        //     case DHCP_OFFER:
        //         printf("Respuesta recibida: DHCP Offer\n");
        //         break;

        //     case DHCP_ACK:
        //         printf("Respuesta recibida: DHCP ACK\n");
        //         break;

        //     default:
        //         printf("Respuesta DHCP desconocida.\n");
        //         continue;
        // }

        // Reenviar la respuesta al cliente
        if (sendto(relay_sock, &dhcp_request, resp_len, 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
            perror("Error al reenviar respuesta al cliente");
            continue;
        }

        printf("Respuesta DHCP reenviada al cliente.\n");
    }

    close(relay_sock);
    return 0;
}
