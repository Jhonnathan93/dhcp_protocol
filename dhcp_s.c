#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/select.h>

// Definición de códigos de mensaje DHCP
#define DHCP_DISCOVER 1  // Mensaje DHCP Discover
#define DHCP_REQUEST 3   // Mensaje DHCP Request
#define DHCP_OFFER 2     // Mensaje DHCP Offer
#define DHCP_ACK 5       // Mensaje DHCP Acknowledgement (ACK)
#define DHCP_NAK 6       // Mensaje DHCP Negative Acknowledgement (NAK)
#define DHCP_MAGIC_COOKIE 0x63825363  // Valor fijo para identificar mensajes DHCP

// Definiciones relacionadas con el servidor
#define MAX_CLIENTS 8    // Número máximo de clientes
#define LEASE_TIME 60    // Tiempo de arrendamiento en segundos

// Estructura para el formato de un paquete DHCP
struct dhcp_packet {
    uint8_t op;            // Tipo de mensaje (1 para solicitud, 2 para respuesta)
    uint8_t htype;         // Tipo de hardware (1 para Ethernet)
    uint8_t hlen;          // Longitud de la dirección de hardware
    uint8_t hops;          // Número de saltos (no usado en DHCP)
    uint32_t xid;          // Identificador de transacción
    uint16_t secs;         // Segundos transcurridos desde que se inició la solicitud DHCP
    uint16_t flags;        // Flags (bit de broadcast)
    uint32_t ciaddr;       // Dirección IP del cliente (si ya tiene una)
    uint32_t yiaddr;       // Dirección IP "tuya" (IP ofrecida al cliente)
    uint32_t siaddr;       // Dirección IP del servidor
    uint32_t giaddr;       // Dirección IP del gateway (relay)
    uint8_t chaddr[16];    // Dirección de hardware (MAC)
    char sname[64];        // Nombre del servidor (opcional)
    char file[128];        // Nombre del archivo de arranque (opcional)
    uint32_t magic_cookie; // Valor especial que identifica los paquetes DHCP
    uint8_t options[312];  // Opciones DHCP
};

// Estructura para almacenar asignaciones de IP
struct ip_assignment {
    uint32_t ip;               // Dirección IP asignada
    uint8_t mac[6];            // Dirección MAC del cliente
    time_t lease_start;        // Inicio del arrendamiento
    int lease_duration;        // Duración del arrendamiento
    uint32_t xid;              // Identificador de transacción para evitar duplicados
};

// Pool de IPs para asignar
struct ip_assignment ip_pool[MAX_CLIENTS];
uint32_t ip_range_start = 0xc0a80003;  // Rango de inicio de direcciones IP (192.168.0.3)
uint32_t ip_range_end = 0xC0A8006E;    // Rango de fin de direcciones IP (192.168.0.110)

// Inicializa el pool de IPs con valores vacíos
void init_ip_pool() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ip_pool[i].ip = 0;                  // Sin IP asignada
        memset(ip_pool[i].mac, 0, 6);       // MAC vacía
        ip_pool[i].lease_start = time(NULL); // Hora actual
        ip_pool[i].lease_duration = LEASE_TIME;  // Tiempo de arrendamiento
        ip_pool[i].xid = 0;                 // XID en 0 (sin solicitudes)
    }
}

// Busca una IP libre en el rango definido
uint32_t find_free_ip() {
    for (uint32_t ip = ip_range_start; ip <= ip_range_end; ip++) {
        int is_assigned = 0;
        // Verifica si la IP ya está asignada
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (ip_pool[i].ip == ip) {
                is_assigned = 1;
                break;
            }
        }
        // Si no está asignada, retorna la IP
        if (!is_assigned) {
            return ip;
        }
    }
    return 0;  // No hay IPs libres
}

// Busca si un cliente ya tiene una IP asignada según su MAC
uint32_t find_ip_by_mac(uint8_t *mac) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Compara las direcciones MAC
        if (memcmp(ip_pool[i].mac, mac, 6) == 0) {
            return ip_pool[i].ip;
        }
    }
    return 0;  // El cliente no tiene IP asignada
}

// Asigna una IP a un cliente específico basado en su MAC y XID
void assign_ip_to_client(uint32_t ip, uint8_t *mac, uint32_t xid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip == 0) {  // Busca una entrada vacía
            ip_pool[i].ip = ip;    // Asigna la IP
            memcpy(ip_pool[i].mac, mac, 6);  // Asigna la MAC
            ip_pool[i].xid = xid;  // Guarda el XID
            break;
        }
    }
}

// Libera las IPs cuyos arrendamientos han expirado
void release_expired_ips() {
    time_t current_time = time(NULL);  // Hora actual
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ip_pool[i].ip != 0) {  // Si la IP está asignada
            // Si el tiempo del lease ha expirado
            if (difftime(current_time, ip_pool[i].lease_start) > ip_pool[i].lease_duration) {
                printf("IP Lease duration: %.f seconds\n", difftime(current_time, ip_pool[i].lease_start));
                printf("IP %s liberada (lease expirado).\n", inet_ntoa(*(struct in_addr *)&ip_pool[i].ip));
                ip_pool[i].ip = 0;  // Libera la IP
                memset(ip_pool[i].mac, 0, 6);  // Limpia la MAC
                ip_pool[i].lease_start = time(NULL);  // Reinicia el lease
                ip_pool[i].xid = 0;  // Limpia el XID
            }
        }
    }
}

// Verifica si un XID ya fue procesado (para evitar solicitudes duplicadas)
int is_duplicate_xid(uint32_t xid, uint8_t *mac) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0 && ip_pool[i].xid == xid) {
            return 1;  // Solicitud duplicada
        }
    }
    return 0;  // No es duplicada
}

// Construye un paquete DHCP Offer para enviar al cliente
void construct_dhcp_offer(struct dhcp_packet *packet, uint32_t offered_ip, uint8_t *mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));  // Limpia el paquete

    packet->op = 2;  // Respuesta del servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->hops = 0;
    packet->xid = htonl(0x12345678);  // Identificador de transacción (dummy)
    packet->yiaddr = htonl(offered_ip);  // IP ofrecida al cliente
    packet->siaddr = inet_addr("192.168.0.1");  // Dirección del servidor
    memcpy(packet->chaddr, mac, 6);  // Copia la MAC del cliente
    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);  // Agrega la cookie mágica

    // Opciones DHCP (tipo de mensaje, máscara de red, gateway, DNS)
    packet->options[0] = 53;  // Tipo de mensaje DHCP
    packet->options[1] = 1;
    packet->options[2] = DHCP_OFFER;  // Es un DHCP Offer

    // Máscara de red (opción 1)
    packet->options[3] = 1;
    packet->options[4] = 4;
    uint32_t subnet_mask = inet_addr("255.255.255.0");
    memcpy(&packet->options[5], &subnet_mask, 4);

    // Gateway (opción 3)
    packet->options[9] = 3;
    packet->options[10] = 4;
    uint32_t gateway = inet_addr("192.168.0.1");
    memcpy(&packet->options[11], &gateway, 4);

    // Servidor DNS (opción 6)
    packet->options[15] = 6;
    packet->options[16] = 4;
    uint32_t dns_server = inet_addr("8.8.8.8");
    memcpy(&packet->options[17], &dns_server, 4);

    packet->options[21] = 255;  // Fin de opciones
}

// Construye un paquete DHCP ACK para confirmar asignación de IP
void construct_dhcp_ack(struct dhcp_packet *packet, uint32_t assigned_ip, uint8_t *mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));  // Limpia el paquete

    packet->op = 2;  // Respuesta del servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    packet->yiaddr = htonl(assigned_ip);  // IP asignada al cliente
    packet->siaddr = inet_addr("192.168.0.1");  // Dirección del servidor
    memcpy(packet->chaddr, mac, 6);  // Copia la MAC del cliente
    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);  // Agrega la cookie mágica

    // Opciones DHCP (tipo de mensaje)
    packet->options[0] = 53;  // Tipo de mensaje DHCP
    packet->options[1] = 1;
    packet->options[2] = DHCP_ACK;  // Es un DHCP ACK
    packet->options[3] = 255;  // Fin de opciones

    // Actualiza el inicio y duración del arrendamiento
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (memcmp(ip_pool[i].mac, mac, 6) == 0 && ip_pool[i].ip == assigned_ip) {
            ip_pool[i].lease_start = time(NULL);  // Reinicia el lease
            ip_pool[i].lease_duration = LEASE_TIME;  // Duración del lease
            break;
        }
    }
}

// Construye un paquete DHCP NAK para denegar la solicitud
void construct_dhcp_nak(struct dhcp_packet *packet, uint8_t *mac) {
    memset(packet, 0, sizeof(struct dhcp_packet));  // Limpia el paquete

    packet->op = 2;  // Respuesta del servidor
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Tamaño de la dirección HW
    memcpy(packet->chaddr, mac, 6);  // Copia la MAC del cliente
    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);  // Agrega la cookie mágica

    // Opciones DHCP (tipo de mensaje)
    packet->options[0] = 53;  // Tipo de mensaje DHCP
    packet->options[1] = 1;
    packet->options[2] = DHCP_NAK;  // Es un DHCP NAK
    packet->options[3] = 255;  // Fin de opciones
}

// Función principal del servidor DHCP
int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;  // Direcciones del servidor y cliente
    struct dhcp_packet dhcp_request, dhcp_ack, dhcp_nak, dhcp_discover, dhcp_offer;  // Paquetes DHCP
    socklen_t client_addr_len = sizeof(client_addr);
    uint8_t client_mac[6];  // Dirección MAC del cliente
    uint32_t offered_ip;     // IP ofrecida
    uint32_t xid;            // Identificador de transacción

    // Crear el socket UDP
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(67);  // Puerto DHCP (67)
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Escuchar en cualquier interfaz

    // Vincular el socket a la dirección del servidor
    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Inicializar el pool de IPs
    init_ip_pool();

    while (1) {
        release_expired_ips();  // Liberar IPs expiradas

        fd_set read_fds;
        struct timeval timeout;
        timeout.tv_sec = 5;  // Timeout de 5 segundos
        timeout.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        int activity = select(sock + 1, &read_fds, NULL, NULL, &timeout);

        // Si ocurre un error en select()
        if (activity < 0) {
            perror("select error");
            continue;
        }

        // Si el timeout expira, volver a empezar el ciclo
        if (activity == 0) {
            continue;
        }

        // Si hay actividad en el socket
        if (FD_ISSET(sock, &read_fds)) {
            // Recibir paquete DHCP
            if (recvfrom(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
                perror("Error al recibir datos");
                continue;
            }
        }

        memcpy(client_mac, dhcp_request.chaddr, 6);  // Copiar la MAC del cliente
        xid = ntohl(dhcp_request.xid);  // Extraer el XID del cliente

        // Manejo de DHCP Discover
        if (dhcp_request.options[0] == 53 && dhcp_request.options[1] == 1 && dhcp_request.options[2] == DHCP_DISCOVER) {
            printf("DHCP Discover recibido del cliente.\n");

            // Verificar si es una solicitud duplicada por XID
            if (is_duplicate_xid(xid, client_mac)) {
                printf("Solicitud duplicada ignorada (xid = %u).\n", xid);
                continue;
            }

            // Verificar si el cliente ya tiene una IP asignada
            offered_ip = find_ip_by_mac(client_mac);
            if (offered_ip == 0) {
                // Buscar una IP libre
                offered_ip = find_free_ip();
                if (offered_ip == 0) {
                    printf("No hay más direcciones IP disponibles.\n");
                    // Enviar un DHCP NAK si no hay IPs
                    construct_dhcp_nak(&dhcp_nak, client_mac);
                    if (sendto(sock, &dhcp_nak, sizeof(dhcp_nak), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                        perror("Error al enviar DHCP NAK");
                    } else {
                        printf("DHCP NAK enviado.\n");
                    }
                    continue;
                }
                // Asignar la IP al cliente
                assign_ip_to_client(offered_ip, client_mac, xid);
            }

            // Construir y enviar el DHCP Offer
            construct_dhcp_offer(&dhcp_offer, offered_ip, client_mac);
            if (sendto(sock, &dhcp_offer, sizeof(dhcp_offer), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                perror("Error al enviar DHCP OFFER");
            } else {
                printf("DHCP Offer enviado al cliente.\n");
            }
        }

        // Manejo de DHCP Request
        if (dhcp_request.options[0] == 53 && dhcp_request.options[1] == 1 && dhcp_request.options[2] == DHCP_REQUEST) {
            printf("DHCP Request recibido del cliente.\n");

            // Enviar el DHCP ACK al cliente
            construct_dhcp_ack(&dhcp_ack, offered_ip, client_mac);
            if (sendto(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
                perror("Error al enviar DHCP ACK");
            } else {
                printf("DHCP ACK enviado: IP asignada = %s\n", inet_ntoa(*(struct in_addr *)&dhcp_ack.yiaddr));
            }
        }

        // Mostrar el XID para depuración
        printf("(xid = %u)\n", xid);
    }

    close(sock);  // Cerrar el socket

    return 0;
}
