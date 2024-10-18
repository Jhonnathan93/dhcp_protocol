#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

// Definiciones de puertos DHCP
#define DHCP_SERVER_PORT 67  // Puerto del servidor DHCP
#define DHCP_CLIENT_PORT 68  // Puerto del cliente DHCP
#define BUFFER_SIZE 1024     // Tamaño del buffer para los paquetes

// Estructura que representa un paquete DHCP
struct dhcp_packet {
    uint8_t op;            // Tipo de mensaje (1: solicitud, 2: respuesta)
    uint8_t htype;         // Tipo de hardware (1: Ethernet)
    uint8_t hlen;          // Longitud de la dirección de hardware
    uint8_t hops;          // Número de saltos
    uint32_t xid;          // Identificador de transacción
    uint16_t secs;         // Segundos desde el inicio de la solicitud
    uint16_t flags;        // Flags (bit de broadcast)
    uint32_t ciaddr;       // Dirección IP del cliente (si ya tiene una)
    uint32_t yiaddr;       // Dirección IP "tuya" (IP ofrecida al cliente)
    uint32_t siaddr;       // Dirección IP del servidor DHCP
    uint32_t giaddr;       // Dirección IP del gateway (rellenado por el relay)
    uint8_t chaddr[16];    // Dirección de hardware (MAC del cliente)
    char sname[64];        // Nombre del servidor (opcional)
    char file[128];        // Nombre del archivo de arranque (opcional)
    uint32_t magic_cookie; // Valor especial que identifica los paquetes DHCP
    uint8_t options[312];  // Opciones DHCP
};

// Función para obtener el tipo de mensaje DHCP del paquete
uint8_t get_dhcp_message_type(struct dhcp_packet *packet) {
    // Recorrer las opciones buscando la opción 53 (tipo de mensaje DHCP)
    for (int i = 0; i < sizeof(packet->options); i++) {
        if (packet->options[i] == 53) { // 53 es el tipo de mensaje
            return packet->options[i + 2]; // El tipo está dos posiciones después
        }
    }
    return 0;  // Si no se encuentra, devuelve 0 (no es un tipo válido)
}

int main() {
    int relay_sock;  // Descriptor del socket del relay
    struct sockaddr_in relay_addr, client_addr, server_addr;  // Direcciones del relay, cliente y servidor
    struct dhcp_packet dhcp_request;  // Paquete DHCP para manejar las solicitudes
    socklen_t addr_len = sizeof(client_addr);  // Tamaño de la estructura de la dirección
    ssize_t recv_len;  // Variable para almacenar la longitud del mensaje recibido

    // Crear un socket UDP para el relay
    if ((relay_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket del relay");
        exit(1);
    }

    // Configurar la dirección del relay para escuchar en cualquier interfaz
    memset(&relay_addr, 0, sizeof(relay_addr));  // Limpiar la estructura
    relay_addr.sin_family = AF_INET;  // IPv4
    relay_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Escuchar en cualquier interfaz disponible
    relay_addr.sin_port = htons(1067);  // Usar un puerto arbitrario para el relay

    // Enlazar el socket a la dirección del relay
    if (bind(relay_sock, (struct sockaddr *)&relay_addr, sizeof(relay_addr)) < 0) {
        perror("Error al enlazar socket del relay");
        close(relay_sock);
        exit(1);
    }

    // Ciclo principal del relay: recibir y reenviar paquetes
    while (1) {
        printf("Esperando paquete DHCP del cliente...\n");

        // Recibir paquete DHCP (Discover o Request) desde el cliente
        recv_len = recvfrom(relay_sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("Error al recibir paquete del cliente");
            continue;
        }

        printf("Paquete recibido con tamaño: %zd bytes\n", recv_len);
        printf("Paquete recibido del cliente con xid: %u\n", ntohl(dhcp_request.xid));

        // Identificar el tipo de paquete DHCP recibido (Discover, Request, etc.)
        uint8_t dhcp_message_type = get_dhcp_message_type(&dhcp_request);

        // Configurar la dirección del servidor DHCP para reenviar el paquete
        memset(&server_addr, 0, sizeof(server_addr));  // Limpiar la estructura
        server_addr.sin_family = AF_INET;  // IPv4
        server_addr.sin_port = htons(DHCP_SERVER_PORT);  // Puerto del servidor DHCP (67)
        server_addr.sin_addr.s_addr = inet_addr("192.168.0.1");  // IP del servidor DHCP

        // Modificar el campo giaddr (Gateway IP Address) antes de reenviar el paquete al servidor
        dhcp_request.giaddr = inet_addr("192.168.0.2");  // IP del relay
        printf("Reenviando paquete al servidor DHCP con giaddr: %s\n", inet_ntoa(*(struct in_addr *)&dhcp_request.giaddr));

        // Reenviar el paquete DHCP al servidor
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

        // Reenviar la respuesta del servidor al cliente
        if (sendto(relay_sock, &dhcp_request, resp_len, 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
            perror("Error al reenviar respuesta al cliente");
            continue;
        }

        printf("Respuesta DHCP reenviada al cliente.\n");
    }

    // Cerrar el socket del relay antes de salir
    close(relay_sock);
    return 0;
}
