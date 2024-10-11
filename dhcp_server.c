#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

#define DHCP_OFFER 2
#define DHCP_ACK 5
#define DHCP_MAGIC_COOKIE 0x63825363
#define MAX_CLIENTS 10  // Máximo número de clientes que pueden recibir IPs
#define LEASE_TIME 60   // Tiempo de arrendamiento en segundos

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

// Estructura para almacenar asignaciones de IP
struct ip_assignment {
    uint32_t ip;         // IP asignada
    uint8_t mac[6];      // Dirección MAC del cliente
    time_t lease_start;  // Tiempo de inicio del arrendamiento
    int lease_duration;  // Duración del arrendamiento en segundos
};

struct ip_assignment ip_pool[MAX_CLIENTS];  // Pool de asignación de IPs
uint32_t ip_range_start = 0xC0A80064;  // 192.168.0.100 en hexadecimal
uint32_t ip_range_end = 0xC0A8006E;    // 192.168.0.110 en hexadecimal

// Inicializa el pool de IPs
void init_ip_pool() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ip_pool[i].ip = 0;
        memset(ip_pool[i].mac, 0, 6);
        ip_pool[i].lease_start = 0;
        ip_pool[i].lease_duration = 0;
    }
}

// Encuentra una IP libre en el rango
uint32_t find_free_ip() {
    for (uint32_t ip = ip_range_start; ip <= ip_range_end; ip++) {
        int is_assigned = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (ip_pool[i].ip == ip) {
                is_assigned = 1;
                break;
            }
        }
        if (!is_assigned) {
            return ip;
        }
    }
    return 0;  // No hay IPs libres
}

// Registra una asignación de IP para un cliente
void assign_ip_to_client(uint32_t ip, uint8_t *mac) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip == 0) {  // Encontrar entrada libre
            ip_pool[i].ip = ip;
            memcpy(ip_pool[i].mac, mac, 6);
            ip_pool[i].lease_start = time(NULL);  // Inicio del arrendamiento
            ip_pool[i].lease_duration = LEASE_TIME;
            break;
        }
    }
}

// Libera una IP si el arrendamiento ha expirado
void release_expired_ips() {
    time_t current_time = time(NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip != 0) {  // Si hay una IP asignada
            if (difftime(current_time, ip_pool[i].lease_start) > ip_pool[i].lease_duration) {
                printf("IP %s liberada (lease expirado).\n", inet_ntoa(*(struct in_addr *)&ip_pool[i].ip));
                ip_pool[i].ip = 0;  // Liberar la IP
                memset(ip_pool[i].mac, 0, 6);
                ip_pool[i].lease_start = 0;
                ip_pool[i].lease_duration = 0;
            }
        }
    }
}

// Construye un mensaje DHCP Offer
void construct_dhcp_offer(struct dhcp_packet *packet, uint32_t offered_ip, uint8_t *mac) {
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
    memcpy(packet->chaddr, mac, 6);  // Dirección MAC del cliente

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;  // DHCP Message Type
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_OFFER;  // DHCP Offer
    packet->options[3] = 255;  // Fin de opciones
}

// Construye un mensaje DHCP ACK
void construct_dhcp_ack(struct dhcp_packet *packet, uint32_t assigned_ip, uint8_t *mac) {
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
    memcpy(packet->chaddr, mac, 6);  // Dirección MAC del cliente

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
    uint32_t offered_ip;
    uint8_t client_mac[6];

    // Inicializar pool de IPs
    init_ip_pool();

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
        // Revisar si alguna IP ha expirado
        release_expired_ips();

        // Esperar DHCP Request del cliente
        if (recvfrom(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
            perror("Error al recibir datos");
            continue;
        }

        // Procesar DHCP Request
        printf("DHCP Request recibido del cliente.\n");
        memcpy(client_mac, dhcp_request.chaddr, 6);

        // Asignar IP dinámica
        offered_ip = find_free_ip();
        if (offered_ip == 0) {
            printf("No hay más direcciones IP disponibles.\n");
            continue;
        }

        // Registrar la IP asignada
        assign_ip_to_client(offered_ip, client_mac);

        // Construir y enviar DHCP ACK
        construct_dhcp_ack(&dhcp_ack, offered_ip, client_mac);
        if (sendto(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
            perror("Error al enviar DHCP ACK");
        } else {
            printf("DHCP ACK enviado: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&offered_ip));
        }
    }

    close(sock);
    return 0;
}
