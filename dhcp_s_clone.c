#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>

#define DHCP_DISCOVER 1
#define DHCP_REQUEST 3
#define DHCP_OFFER 2
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_MAGIC_COOKIE 0x63825363
#define MAX_CLIENTS 10
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
    uint32_t ip;
    uint8_t mac[6];
    time_t lease_start;
    int lease_duration;
    uint32_t xid;  // Identificador de transacción para controlar duplicados
};

// Mutex para proteger el acceso a ip_pool
pthread_mutex_t ip_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ip_assignment ip_pool[MAX_CLIENTS];
uint32_t ip_range_start = 0xC0A80064;  // 192.168.0.100 en hexadecimal
uint32_t ip_range_end = 0xC0A8006E;    // 192.168.0.110 en hexadecimal

// Inicializa el pool de IPs
void init_ip_pool() {
    pthread_mutex_lock(&ip_pool_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ip_pool[i].ip = 0;
        memset(ip_pool[i].mac, 0, 6);
        ip_pool[i].lease_start = 0;
        ip_pool[i].lease_duration = 0;
        ip_pool[i].xid = 0;
    }
    pthread_mutex_unlock(&ip_pool_mutex);
}

// Encuentra una IP libre
uint32_t find_free_ip() {
    pthread_mutex_lock(&ip_pool_mutex);
    for (uint32_t ip = ip_range_start+1; ip < ip_range_end; ip++) {
        int is_assigned = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (ip_pool[i].ip == ip) {
                is_assigned = 1;
                break;
            }
        }
        if (!is_assigned) {
            pthread_mutex_unlock(&ip_pool_mutex);
            return ip;
        }
    }
    pthread_mutex_unlock(&ip_pool_mutex);
    return 0;  // No hay IPs libres
}

// Busca si el cliente ya tiene una IP asignada
uint32_t find_ip_by_mac(uint8_t *mac) {
    pthread_mutex_lock(&ip_pool_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0) {
            pthread_mutex_unlock(&ip_pool_mutex);
            return ip_pool[i].ip;
        }
    }
    pthread_mutex_unlock(&ip_pool_mutex);
    return 0;  // El cliente no tiene IP asignada
}

// Asigna una IP al cliente
void assign_ip_to_client(uint32_t ip, uint8_t *mac, uint32_t xid) {
    pthread_mutex_lock(&ip_pool_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip == 0) {  // Buscar una entrada libre
            ip_pool[i].ip = ip;
            memcpy(ip_pool[i].mac, mac, 6);
            ip_pool[i].lease_start = time(NULL);  // Tiempo de inicio del arrendamiento
            ip_pool[i].lease_duration = LEASE_TIME;
            ip_pool[i].xid = xid;  // Guarda el xid para controlar duplicados
            break;
        }
    }
    pthread_mutex_unlock(&ip_pool_mutex);
}

// Libera las IPs cuyos arrendamientos han expirado
void release_expired_ips() {
    pthread_mutex_lock(&ip_pool_mutex);
    time_t current_time = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip != 0) {
            if (difftime(current_time, ip_pool[i].lease_start) > ip_pool[i].lease_duration) {
                printf("IP %s liberada (lease expirado).\n", inet_ntoa(*(struct in_addr *)&ip_pool[i].ip));
                ip_pool[i].ip = 0;
                memset(ip_pool[i].mac, 0, 6);
                ip_pool[i].lease_start = 0;
                ip_pool[i].lease_duration = 0;
                ip_pool[i].xid = 0;  // Limpia el xid
            }
        }
    }
    pthread_mutex_unlock(&ip_pool_mutex);
}

// Función para verificar si un `xid` ya fue procesado recientemente
int is_duplicate_xid(uint32_t xid, uint8_t *mac) {
    pthread_mutex_lock(&ip_pool_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0) {
            if (ip_pool[i].xid == xid) {
                pthread_mutex_unlock(&ip_pool_mutex);
                return 1;  // Solicitud duplicada
            }
        }
    }
    pthread_mutex_unlock(&ip_pool_mutex);
    return 0;  // No es un duplicado
}

// Construye un DHCP Offer
void construct_dhcp_offer(struct dhcp_packet *packet, uint32_t offered_ip, uint8_t *mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;  // Servidor -> Cliente
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);
    packet->secs = 0;
    packet->flags = 0;
    packet->ciaddr = 0;
    packet->yiaddr = htonl(offered_ip);  // IP ofrecida
    packet->siaddr = inet_addr("192.168.0.1");
    packet->giaddr = 0;
    memcpy(packet->chaddr, mac, 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;
    packet->options[1] = 1;
    packet->options[2] = DHCP_OFFER;

    // Máscara de red (opción 1)
    packet->options[3] = 1;
    packet->options[4] = 4;
    uint32_t subnet_mask = inet_addr("255.255.255.0");
    memcpy(&packet->options[5], &subnet_mask, 4);

    // Puerta de enlace (opción 3)
    packet->options[9] = 3;
    packet->options[10] = 4;
    uint32_t gateway = inet_addr("192.168.0.1");
    memcpy(&packet->options[11], &gateway, 4);

    // DNS (opción 6)
    packet->options[15] = 6;
    packet->options[16] = 4;
    uint32_t dns_server = inet_addr("8.8.8.8");
    memcpy(&packet->options[17], &dns_server, 4);

    packet->options[21] = 255;  // Fin de opciones
}

// Construye un DHCP ACK
void construct_dhcp_ack(struct dhcp_packet *packet, uint32_t assigned_ip, uint8_t *mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;
    packet->htype = 1;
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(0x12345678);
    packet->secs = 0;
    packet->flags = 0;
    packet->ciaddr = 0;
    packet->yiaddr = htonl(assigned_ip);  // IP asignada
    packet->siaddr = inet_addr("192.168.0.1");
    packet->giaddr = 0;
    memcpy(packet->chaddr, mac, 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    //Opciones DHCP
    packet->options[0] = 53;
    packet->options[1] = 1;
    packet->options[2] = DHCP_ACK;

    packet->options[3] = 255;  // Fin de opciones
}

void process_dhcp_message(int server_socket) {
    struct dhcp_packet dhcp_request, dhcp_offer, dhcp_ack;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    uint8_t client_mac[6];
    uint32_t offered_ip;
    uint32_t xid;

    while (1) {
        release_expired_ips();

        // Recibir mensajes DHCP
        if (recvfrom(server_socket, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
            perror("Error al recibir datos");
            continue;
        }

        memcpy(client_mac, dhcp_request.chaddr, 6);
        xid = ntohl(dhcp_request.xid);

        printf("The client mac is: %02x:%02x:%02x:%02x:%02x:%02x\n", client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5]);
        // Manejar DHCP Discover
        if (dhcp_request.options[0] == 53 && dhcp_request.options[1] == 1 && dhcp_request.options[2] == DHCP_DISCOVER) {
            printf("DHCP Discover recibido del cliente.\n");

            // Verificar si es una solicitud duplicada por `xid`
            if (is_duplicate_xid(xid, client_mac)) {
                printf("Solicitud duplicada ignorada (xid = %u).\n", xid);
                continue;
            }

            // Verificar si el cliente ya tiene una IP asignada
            offered_ip = find_ip_by_mac(client_mac);
            if (offered_ip == 0) {
                offered_ip = find_free_ip();
                if (offered_ip == 0) {
                    printf("No hay más direcciones IP disponibles.\n");
                    continue;
                }
                assign_ip_to_client(offered_ip, client_mac, xid);
            }

            // Construir y enviar el DHCPOFFER
            construct_dhcp_offer(&dhcp_offer, offered_ip, client_mac);
            if (sendto(server_socket, &dhcp_offer, sizeof(dhcp_offer), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                perror("Error al enviar DHCP OFFER");
            } else {
                printf("DHCPOFFER enviado a cliente.\n");
            }
        }

        // Manejar DHCP Request
        if (dhcp_request.options[0] == 53 && dhcp_request.options[1] == 1 && dhcp_request.options[2] == DHCP_REQUEST) {
            printf("DHCP Request recibido del cliente.\n");

            // Enviar el DHCPACK al cliente
            construct_dhcp_ack(&dhcp_ack, offered_ip, client_mac);
            if (sendto(server_socket, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                perror("Error al enviar DHCP ACK");
            } else {
                printf("DHCP ACK enviado: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&dhcp_ack.yiaddr));
            }
        }

        printf("(xid = %u)\n", xid);
    }
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr;

    // Crear socket UDP
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("No se pudo crear el socket");
        return 1;
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(67);  // Puerto DHCP

    // Enlazar socket a la dirección del servidor
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el bind");
        close(server_socket);
        return 1;
    }

    printf("Servidor DHCP iniciado...\n");

    // Inicializar el pool de IPs
    init_ip_pool();

    // Procesar mensajes DHCP
    process_dhcp_message(server_socket);

    // Cerrar socket del servidor
    close(server_socket);
    return 0;
}