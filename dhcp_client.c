#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>

// Definiciones de tipos de mensajes DHCP y otros parámetros
#define DHCP_DISCOVER 1       // Tipo de mensaje DHCP Discover
#define DHCP_REQUEST 3        // Tipo de mensaje DHCP Request
#define DHCP_MAGIC_COOKIE 0x63825363  // Valor fijo para identificar mensajes DHCP
#define LEASE_TIME 60         // Duración del lease en segundos (para la simulación)

// Estructura que representa un paquete DHCP
struct dhcp_packet {
    uint8_t op;            // Tipo de mensaje (1 para solicitud, 2 para respuesta)
    uint8_t htype;         // Tipo de hardware (1 para Ethernet)
    uint8_t hlen;          // Longitud de la dirección de hardware
    uint8_t hops;          // Número de saltos (normalmente 0 en DHCP)
    uint32_t xid;          // Identificador de transacción
    uint16_t secs;         // Segundos transcurridos desde que se inició la solicitud DHCP
    uint16_t flags;        // Flags (bit de broadcast)
    uint32_t ciaddr;       // Dirección IP del cliente (si tiene una)
    uint32_t yiaddr;       // Dirección IP ofrecida al cliente (en las respuestas)
    uint32_t siaddr;       // Dirección IP del servidor
    uint32_t giaddr;       // Dirección IP del gateway (relay, si aplica)
    uint8_t chaddr[16];    // Dirección de hardware (MAC del cliente)
    char sname[64];        // Nombre del servidor (opcional)
    char file[128];        // Nombre del archivo de arranque (opcional)
    uint32_t magic_cookie; // Valor especial que identifica los paquetes DHCP
    uint8_t options[312];  // Opciones DHCP
};

// Variable global para almacenar el xid del cliente (identificador de transacción)
uint32_t global_xid = 0;

// Función auxiliar para imprimir los bytes de una dirección IP
void print_ip_bytes(uint32_t ip) {
    unsigned char *bytes = (unsigned char *)&ip;
    printf("IP address bytes: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
}

// Construye un paquete DHCP Discover para que el cliente busque un servidor DHCP
void construct_dhcp_discover(struct dhcp_packet *packet, uint32_t xid) {
    memset(packet, 0, sizeof(struct dhcp_packet));  // Limpia el paquete

    packet->op = 1;  // Cliente -> Servidor (solicitud)
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Longitud de la dirección de hardware (MAC)
    packet->hops = 0;   // No hay saltos
    packet->xid = htonl(xid);  // Identificador de transacción (único para cada cliente)

    packet->secs = htons(0);   // Número de segundos en el formato de red
    packet->flags = htons(0x8000);  // Flag de broadcast activado
    packet->ciaddr = htonl(0);  // El cliente aún no tiene dirección IP
    packet->yiaddr = htonl(0);  // El servidor aún no ha ofrecido una dirección IP
    packet->siaddr = htonl(0);  // No se especifica servidor
    packet->giaddr = htonl(0);  // No hay relay

    // Dirección MAC del cliente
    packet->chaddr[0] = 0x00;
    packet->chaddr[1] = 0x0c;
    packet->chaddr[2] = 0x29;
    packet->chaddr[3] = 0x3e;
    packet->chaddr[4] = 0x53;
    packet->chaddr[5] = 0xf7;

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);  // Agrega la cookie mágica

    // Opciones DHCP (indica que es un DHCP Discover)
    packet->options[0] = 53;  // Tipo de mensaje DHCP
    packet->options[1] = 1;   // Longitud
    packet->options[2] = DHCP_DISCOVER;  // DHCP Discover
    packet->options[3] = 255;  // Fin de las opciones
}

// Construye un paquete DHCP Request para solicitar una IP ofrecida
void construct_dhcp_request(struct dhcp_packet *packet, uint32_t offered_ip, uint32_t xid) {
    memset(packet, 0, sizeof(struct dhcp_packet));  // Limpia el paquete


    packet->op = 1;  // Cliente -> Servidor (solicitud)
    packet->htype = 1;  // Ethernet
    packet->hlen = 6;   // Longitud de la dirección de hardware (MAC)
    packet->hops = 0;   // No hay saltos
    packet->xid = htonl(xid);  // Identificador de transacción

    packet->flags = htons(0x8000);  // Flag de broadcast activado
    packet->ciaddr = 0;  // El cliente aún no tiene dirección IP
    packet->yiaddr = 0;  // El servidor aún no ha asignado una dirección IP
    packet->siaddr = 0;  // No se especifica servidor
    packet->giaddr = 0;  // No hay relay

    // Dirección MAC del cliente
    memcpy(packet->chaddr, (uint8_t[]){0x00, 0x0c, 0x29, 0x3e, 0x53, 0xf7}, 6);

    packet->magic_cookie = htonl(DHCP_MAGIC_COOKIE);  // Agrega la cookie mágica

    // Opciones DHCP (indica que es un DHCP Request)
    packet->options[0] = 53;  // Tipo de mensaje DHCP
    packet->options[1] = 1;
    packet->options[2] = DHCP_REQUEST;  // DHCP Request
    packet->options[3] = 50;  // Opción para solicitar una IP específica
    packet->options[4] = 4;   // Longitud de la IP solicitada
    memcpy(&packet->options[5], &offered_ip, 4);  // Copia la IP ofrecida en las opciones
    packet->options[9] = 255;  // Fin de las opciones
}

// Función para renovar el lease de la IP solicitada
void renew_lease(int sock, struct sockaddr_in *relay_addr, uint32_t offered_ip, uint32_t xid) {
    struct dhcp_packet dhcp_request;
    socklen_t relay_addr_len = sizeof(*relay_addr);

    // Construir y enviar un paquete DHCP Request para renovar el lease
    construct_dhcp_request(&dhcp_request, offered_ip, xid);
    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)relay_addr, relay_addr_len) < 0) {
        perror("Error al enviar DHCP Request para renovación");
        close(sock);
        exit(1);
    }
    printf("DHCP Request enviado para renovación del lease.\n");
}

// Función para analizar las opciones del paquete DHCP recibido (máscara, gateway, DNS)
void parse_dhcp_options(uint8_t *options, uint32_t *subnet_mask, uint32_t *gateway, uint32_t *dns_server) {
    int i = 0;
    // Recorre las opciones hasta encontrar el fin de opciones (255)
    while (i < 312 && options[i] != 255) {
        switch (options[i]) {
            case 1:  // Máscara de red
                memcpy(subnet_mask, &options[i + 2], 4);  // La máscara es de 4 bytes
                break;
            case 3:  // Puerta de enlace (gateway)
                memcpy(gateway, &options[i + 2], 4);  // El gateway es de 4 bytes
                break;
            case 6:  // Servidor DNS
                memcpy(dns_server, &options[i + 2], 4);  // El servidor DNS es de 4 bytes
                break;
            default:
                break;
        }
        i += options[i + 1] + 2;  // Avanzar al siguiente campo (tipo + longitud)
    }
}

// Función principal del cliente DHCP
int main() {
    int sock;
    struct sockaddr_in relay_addr;
    struct dhcp_packet dhcp_request, dhcp_ack, dhcp_offer;
    socklen_t relay_addr_len = sizeof(relay_addr);
    uint32_t offered_ip, ack_ip, subnet_mask = 0, gateway = 0, dns_server = 0;
    time_t lease_start;  // Hora de inicio del lease
    int lease_duration = LEASE_TIME;  // Duración del lease en segundos

    // Inicializar la semilla de números aleatorios para generar el xid
    srand(time(NULL));

    // Crear un socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        return 1;
    }

    // Configurar la dirección del relay o servidor DHCP
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_port = htons(1067);  // Puerto donde escucha el servidor/relay
    relay_addr.sin_addr.s_addr = inet_addr("192.168.0.2");  // Dirección del relay o servidor DHCP

    // Habilitar el uso de broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Error al habilitar broadcast");
        close(sock);
        return 1;
    }

    // Enviar un DHCP Discover al relay o servidor
    printf("Enviando DHCP Discover al relay en IP: %s\n", inet_ntoa(relay_addr.sin_addr));
    global_xid = rand();  // Generar un identificador único (xid) para la transacción
    construct_dhcp_discover(&dhcp_request, global_xid);  // Construir el paquete DHCP Discover

    // Enviar el paquete DHCP Discover
    if (sendto(sock, &dhcp_request, sizeof(dhcp_request), 0, (struct sockaddr *)&relay_addr, relay_addr_len) < 0) {
        perror("Error al enviar DHCP Discover");
        close(sock);
        return 1;
    }

        printf("DHCP Discover enviado con xid = %u.\n", global_xid);

    // Esperar un paquete DHCP Offer
    if (recvfrom(sock, &dhcp_offer, sizeof(dhcp_offer), 0, (struct sockaddr *)&relay_addr, &relay_addr_len) < 0) {
        perror("Error al recibir DHCP Offer");
        close(sock);
        return 1;
    }

    offered_ip = dhcp_offer.yiaddr;  // Dirección IP ofrecida (en formato de red)
    printf("DHCP Offer recibido: IP ofrecida = %s\n", inet_ntoa(*(struct in_addr *)&offered_ip));

    // Imprimir los bytes de la IP ofrecida para verificar el orden
    print_ip_bytes(dhcp_offer.yiaddr);

    // Analizar las opciones del DHCP Offer (máscara de red, gateway, DNS)
    parse_dhcp_options(dhcp_offer.options, &subnet_mask, &gateway, &dns_server);
    printf("Subnet Mask: %s\n", inet_ntoa(*(struct in_addr *)&subnet_mask));
    printf("Gateway: %s\n", inet_ntoa(*(struct in_addr *)&gateway));
    printf("DNS Server: %s\n", inet_ntoa(*(struct in_addr *)&dns_server));

    // Enviar un DHCP Request para solicitar la IP ofrecida
    renew_lease(sock, &relay_addr, offered_ip, global_xid);

    // Esperar un paquete DHCP ACK
    if (recvfrom(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&relay_addr, &relay_addr_len) < 0) {
        perror("Error al recibir DHCP ACK");
        close(sock);
        return 1;
    }

    ack_ip = dhcp_ack.yiaddr;  // IP confirmada (en formato de red)
    printf("DHCP ACK recibido: IP reconocida = %s\n", inet_ntoa(*(struct in_addr *)&ack_ip));


    // Registrar la hora de inicio del lease
    lease_start = time(NULL);

    // Bucle para renovar el lease antes de que expire
    while (1) {  // Repetir indefinidamente
        sleep(lease_duration / 2);  // Dormir hasta la mitad del lease

        // Renovar el lease antes de que expire usando el mismo xid
        renew_lease(sock, &relay_addr, offered_ip, global_xid);

        // Esperar un nuevo DHCP ACK para confirmar la renovación
        if (recvfrom(sock, &dhcp_ack, sizeof(dhcp_ack), 0, (struct sockaddr *)&relay_addr, &relay_addr_len) < 0) {
            perror("Error al recibir DHCP ACK.");
            continue;
        }

        ack_ip = dhcp_ack.yiaddr;  // IP confirmada (en formato de red)
        printf("DHCP ACK recibido: IP reconocida = %s\n", inet_ntoa(*(struct in_addr *)&ack_ip));

        // Reiniciar el tiempo del lease
        lease_start = time(NULL);
    }

    printf("Lease expirado.\n");

    // Cerrar el socket antes de salir
    close(sock);
    return 0;
}
