#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define DHCP_DISCOVER 1
#define DHCP_REQUEST  3
#define DHCP_ACK      5
#define DHCP_NAK      6
#define DHCP_MAGIC_COOKIE 0x63825363
#define LEASE_TIME    60  // Tiempo de arrendamiento en segundos

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

// Simulamos un lease, en un entorno real esto sería recibido desde el servidor
time_t lease_start_time;
int lease_duration = LEASE_TIME;
uint32_t assigned_ip = 0;

// Función para enviar DHCP Discover
void send_dhcp_discover(int sock, struct sockaddr_in *server_addr) {
    struct dhcp_packet dhcp_discover;
    memset(&dhcp_discover, 0, sizeof(dhcp_discover));

    dhcp_discover.op = 1;  // Cliente -> Servidor
    dhcp_discover.htype = 1;  // Ethernet
    dhcp_discover.hlen = 6;   // Tamaño de dirección HW
    dhcp_discover.hops = 0;
    dhcp_discover.xid = htonl(0x12345678);  // Un identificador aleatorio para la transacción
    dhcp_discover.magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    dhcp_discover.options[0] = 53;  // Tipo de mensaje DHCP
    dhcp_discover.options[1] = 1;
    dhcp_discover.options[2] = DHCP_DISCOVER;
    dhcp_discover.options[3] = 255;  // Fin de las opciones

    if (sendto(sock, &dhcp_discover, sizeof(dhcp_discover), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Error al enviar DHCP Discover");
        exit(1);
    }

    printf("DHCP Discover enviado.\n");
}

// Función para enviar DHCP Request (solicitud de renovación o inicial)
void send_dhcp_request(int sock, struct sockaddr_in *server_addr) {
    struct dhcp_packet dhcp_request;
    memset(&dhcp_request, 0, sizeof(dhcp_request));

    dhcp_request.op = 1;  // Cliente -> Servidor
    dhcp_request.htype = 1;  // Ethernet
    dhcp_request.hlen = 6;   // Tamaño de dirección HW
    dhcp_request.hops = 0;
    dhcp_request.xid = htonl(0x12345678);  // Debe ser el mismo que en el DHCP Discover
    dhcp_request.magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    dhcp_request.options[0] = 53;  // Tipo de mensaje DHCP
    dhcp_request.options[1] = 1;
    dhcp_request.options[2] = DHCP_REQUEST;
    dhcp_request.options[3] = 255;  // Fin de las opciones

    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Error al enviar DHCP Request");
        exit(1);
    }

    printf("DHCP Request enviado.\n");
}

// Función para recibir respuestas del servidor (Offer, ACK, NAK)
int receive_dhcp_response(int sock, struct dhcp_packet *dhcp_response) {
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    if (recvfrom(sock, dhcp_response, sizeof(*dhcp_response), 0, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("Error al recibir respuesta");
        return -1;
    }
    return 0;
}

// Función para monitorear el lease y renovar si es necesario
void monitor_lease(int sock, struct sockaddr_in *server_addr) {
    while (1) {
        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, lease_start_time);

        if (elapsed_time >= lease_duration / 2) {
            printf("Tiempo de lease restante: %.0f segundos. Renovando lease...\n", lease_duration - elapsed_time);
            send_dhcp_request(sock, server_addr);

            struct dhcp_packet dhcp_ack;
            if (receive_dhcp_response(sock, &dhcp_ack) == 0 && dhcp_ack.options[2] == DHCP_ACK) {
                printf("DHCP ACK recibido. Lease renovado.\n");
                lease_start_time = time(NULL);  // Reiniciar el lease time
            }
        }

        sleep(10);  // Esperar antes de volver a verificar el lease
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    struct dhcp_packet dhcp_response;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP
    server_addr.sin_addr.s_addr = inet_addr("192.168.0.1");  // IP del servidor DHCP

    // Iniciar proceso de DHCP Discover
    send_dhcp_discover(sock, &server_addr);

    // Recibir DHCP Offer
    if (receive_dhcp_response(sock, &dhcp_response) == 0 && dhcp_response.options[2] == DHCP_OFFER) {
        assigned_ip = dhcp_response.yiaddr;  // IP ofrecida por el servidor
        printf("DHCP Offer recibido: IP ofrecida = %s\n", inet_ntoa(*(struct in_addr *)&assigned_ip));

        // Enviar DHCP Request
        send_dhcp_request(sock, &server_addr);

        // Recibir DHCP ACK
        if (receive_dhcp_response(sock, &dhcp_response) == 0 && dhcp_response.options[2] == DHCP_ACK) {
            printf("DHCP ACK recibido: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&assigned_ip));

            lease_start_time = time(NULL);  // Iniciar el lease time
            monitor_lease(sock, &server_addr);  // Monitorear el lease
        }
    }

    close(sock);
    return 0;
}
