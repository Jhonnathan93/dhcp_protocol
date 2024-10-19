#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/select.h>
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

struct ip_assignment ip_pool[MAX_CLIENTS];
uint32_t ip_range_start = 0xC0A80064;  // 192.168.0.100 en hexadecimal
uint32_t ip_range_end = 0xC0A800C8;    // 192.168.0.200 en hexadecimal

// Mutex para proteger el acceso a ip_pool
pthread_mutex_t pool_mutex;

// Estructura para pasar datos al hilo de cliente
struct client_request {
    int sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    struct dhcp_packet dhcp_request;
};

// Inicializa el pool de IPs
void init_ip_pool() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ip_pool[i].ip = 0;
        memset(ip_pool[i].mac, 0, 6);
        ip_pool[i].lease_start = 0;
        ip_pool[i].lease_duration = LEASE_TIME;
        ip_pool[i].xid = 0;
    }
}

// Encuentra una IP libre
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

// Busca si el cliente ya tiene una IP asignada
uint32_t find_ip_by_mac(uint8_t *mac) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0) {
            return ip_pool[i].ip;
        }
    }
    return 0;  // El cliente no tiene IP asignada
}

// Asigna una IP al cliente
void assign_ip_to_client(uint32_t ip, uint8_t *mac, uint32_t xid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip == 0) {  // Buscar una entrada libre
            ip_pool[i].ip = ip;
            memcpy(ip_pool[i].mac, mac, 6);
            // ip_pool[i].lease_start = time(NULL);    //Revision
            // ip_pool[i].lease_duration = LEASE_TIME; //Revision
            ip_pool[i].xid = xid;  // Guarda el xid para controlar duplicados
            break;
        }
    }
}

// Libera las IPs cuyos arrendamientos han expirado
void release_expired_ips() {
    pthread_mutex_lock(&pool_mutex);  // Bloquear el acceso al pool
    time_t current_time = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip != 0 && ip_pool[i].lease_start != 0) {
            if (difftime(current_time, ip_pool[i].lease_start) > ip_pool[i].lease_duration) {
                printf("IP Lease duration: %.f seconds\n", difftime(current_time, ip_pool[i].lease_start));
                printf("IP %s liberada (lease expirado).\n", inet_ntoa(*(struct in_addr *)&ip_pool[i].ip));
                fflush(stdout);  // Forzar el vaciamiento del buffer
                // Liberar la IP
                ip_pool[i].ip = 0;
                memset(ip_pool[i].mac, 0, 6);
                ip_pool[i].lease_start = 0;
                ip_pool[i].xid = 0;
            }
        }
    }
    pthread_mutex_unlock(&pool_mutex);  // Desbloquear el acceso al pool
}

// Función para verificar si un `xid` ya fue procesado recientemente
int is_duplicate_xid(uint32_t xid, uint8_t *mac) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0) {
            if (ip_pool[i].xid == xid) {
                return 1;  // Solicitud duplicada
            }
        }
    }
    return 0;  // No es un duplicado
}

// Construye un DHCP Offer
void construct_dhcp_offer(struct dhcp_packet *packet, uint32_t offered_ip, uint8_t *mac, uint32_t xid) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;  // Servidor -> Cliente
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(xid);
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast flag
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
void construct_dhcp_ack(struct dhcp_packet *packet, uint32_t assigned_ip, uint8_t *mac, uint32_t xid) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;
    packet->htype = 1;
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(xid);
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast flag
    packet->ciaddr = 0;
    packet->yiaddr = htonl(assigned_ip);  // IP asignada
    packet->siaddr = inet_addr("192.168.0.1");
    packet->giaddr = 0;
    memcpy(packet->chaddr, mac, 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;
    packet->options[1] = 1;
    packet->options[2] = DHCP_ACK;
    packet->options[3] = 255;

    // Actualizar el lease
    pthread_mutex_lock(&pool_mutex);  // Bloquear el acceso al pool
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0 && ip_pool[i].ip == assigned_ip) {
            ip_pool[i].lease_start = time(NULL);  // Iniciar el lease en el momento de ACK
            ip_pool[i].lease_duration = LEASE_TIME;
            break;
        }
    }
    pthread_mutex_unlock(&pool_mutex);  // Desbloquear el acceso al pool
}

// Construye un DHCP NAK
void construct_dhcp_nak(struct dhcp_packet *packet, uint8_t *mac, uint32_t xid) {
    memset(packet, 0, sizeof(struct dhcp_packet));

    packet->op = 2;
    packet->htype = 1;
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(xid);
    packet->secs = 0;
    packet->flags = htons(0x8000);  // Broadcast flag
    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = inet_addr("192.168.0.1");
    packet->giaddr = 0;
    memcpy(packet->chaddr, mac, 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    // Opciones DHCP
    packet->options[0] = 53;
    packet->options[1] = 1;
    packet->options[2] = DHCP_NAK;
    packet->options[3] = 255;
}

// Función que maneja cada solicitud del cliente
void *handle_client_request(void *arg) {
    pthread_t my_id = pthread_self();
    printf("-------------------- \n");
    printf("Hilo creado con ID: %lu\n", (unsigned long)my_id);
    printf("-------------------- \n");
    struct client_request *request = (struct client_request *)arg;
    struct dhcp_packet *dhcp_request = &request->dhcp_request;
    uint8_t client_mac[6];
    memcpy(client_mac, dhcp_request->chaddr, 6);
    uint32_t xid = ntohl(dhcp_request->xid);
    uint32_t offered_ip = 0;

    // Manejo de DHCP Discover
    if (dhcp_request->options[0] == 53 && dhcp_request->options[1] == 1 && dhcp_request->options[2] == DHCP_DISCOVER) {
        printf("DHCP Discover recibido del cliente.\n");

        pthread_mutex_lock(&pool_mutex);  // Bloquear el acceso al pool

        offered_ip = find_ip_by_mac(client_mac); // Verificar si ya tiene IP

        if (offered_ip != 0) {
            char mac_address[18];
            sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X",
                    client_mac[0], client_mac[1], client_mac[2], 
                    client_mac[3], client_mac[4], client_mac[5]);
            printf("Cliente con MAC %s ya tiene una IP asignada.\n", mac_address);
            // No necesitamos asignar una nueva IP, usamos la existente
        } else {
            offered_ip = find_free_ip();
            if (offered_ip == 0) {
                printf("No hay más direcciones IP disponibles.\n");
                // Enviar DHCP NAK al cliente
                struct dhcp_packet dhcp_nak;
                construct_dhcp_nak(&dhcp_nak, client_mac, xid);
                sendto(request->sock, &dhcp_nak, sizeof(dhcp_nak), 0, (struct sockaddr *)&request->client_addr, request->client_addr_len);
                pthread_mutex_unlock(&pool_mutex);
                free(request);
                pthread_exit(NULL);
            }
            assign_ip_to_client(offered_ip, client_mac, xid);
        }

        pthread_mutex_unlock(&pool_mutex);  // Desbloquear el acceso al pool

        // Construir y enviar el DHCPOFFER con la IP asignada o existente
        struct dhcp_packet dhcp_offer;
        construct_dhcp_offer(&dhcp_offer, offered_ip, client_mac, xid);
        if (sendto(request->sock, &dhcp_offer, sizeof(dhcp_offer), 0, (struct sockaddr *)&request->client_addr, request->client_addr_len) < 0) {
            perror("Error al enviar DHCP OFFER");
        } else {
            printf("DHCP Offer enviado a cliente. \n");
        }
    }

    // Manejo de DHCP Request
    else if (dhcp_request->options[0] == 53 && dhcp_request->options[1] == 1 && dhcp_request->options[2] == DHCP_REQUEST) {
        printf("DHCP Request recibido del cliente.\n");

        pthread_mutex_lock(&pool_mutex);  // Bloquear el acceso al pool

        // Verificar si el cliente tiene una IP asignada
        uint32_t assigned_ip = find_ip_by_mac(client_mac);
        if (assigned_ip == 0) {
            printf("El cliente no tiene una IP asignada previamente.\n");
            pthread_mutex_unlock(&pool_mutex);  // Desbloquear el acceso al pool
            free(request);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&pool_mutex);  // Desbloquear el acceso al pool

        // Enviar el DHCPACK al cliente
        struct dhcp_packet dhcp_ack;
        construct_dhcp_ack(&dhcp_ack, assigned_ip, client_mac, xid);
        if (sendto(request->sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&request->client_addr, request->client_addr_len) < 0) {
            perror("Error al enviar DHCP ACK");
        } else {
            printf("DHCP ACK enviado: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&dhcp_ack.yiaddr));
        }
    }

    // Liberar la memoria asignada y terminar el hilo
    free(request);
    pthread_exit(NULL);
}

int main() {
    // Desactivar el buffering de stdout
    setbuf(stdout, NULL);

    int sock;
    struct sockaddr_in server_addr;
    struct dhcp_packet dhcp_request;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Inicializar el mutex
    pthread_mutex_init(&pool_mutex, NULL);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    init_ip_pool();

    while (1) {
        release_expired_ips();

        fd_set read_fds;
        struct timeval timeout;
        timeout.tv_sec = 1;  // 1 second timeout
        timeout.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        int activity = select(sock + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select error");
            continue;
        }

        if (activity == 0) {
            // Timeout occurred, check for expired leases again
            continue;  // No data received, loop again to check leases
        }

        if (FD_ISSET(sock, &read_fds)) {
            struct client_request *request = malloc(sizeof(struct client_request));
            if (request == NULL) {
                perror("Error al asignar memoria para request");
                continue;
            }

            request->sock = sock;
            request->client_addr_len = sizeof(struct sockaddr_in);

            if (recvfrom(sock, &request->dhcp_request, sizeof(request->dhcp_request), 0, (struct sockaddr *)&request->client_addr, &request->client_addr_len) < 0) {
                perror("Error al recibir datos");
                free(request);
                continue;
            }

            // Crear un nuevo hilo para manejar la solicitud del cliente
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client_request, (void *)request) != 0) {
                perror("Error al crear el hilo");
                free(request);
                continue;
            }

            // Separar el hilo para que limpie sus recursos al terminar
            pthread_detach(thread_id);
        }
    }

    // Cerrar el socket y destruir el mutex
    close(sock);
    pthread_mutex_destroy(&pool_mutex);

    return 0;
}
