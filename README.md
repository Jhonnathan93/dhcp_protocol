# Proyecto de DHCP (Cliente y Servidor)

Este proyecto consiste en la implementación de un **servidor DHCP** y un **cliente DHCP** en lenguaje C. Ambos programas siguen el protocolo DHCP (Dynamic Host Configuration Protocol), utilizado para asignar direcciones IP de manera dinámica a dispositivos en una red.

## Archivos del Proyecto

1. `dhcp_server.c`: Implementación del servidor DHCP que escucha solicitudes de clientes, asigna direcciones IP y envía respuestas con información de configuración de red.
2. `dhcp_client.c`: Implementación del cliente DHCP que envía solicitudes al servidor DHCP y recibe la asignación de una dirección IP, junto con información de red como la máscara de subred, puerta de enlace y servidor DNS.

## `dhcp_server.c`

#### Funcionalidad:

El servidor DHCP escucha solicitudes de **DHCP Discover** de los clientes en el puerto 67 (puerto estándar para DHCP en IPv4). Cuando recibe una solicitud válida, responde con un **DHCP Offer**, ofreciendo una dirección IP y otros parámetros de red como la máscara de subred, puerta de enlace y servidor DNS. Luego, si el cliente responde con un **DHCP Request** para confirmar la oferta, el servidor asigna la dirección IP y envía una respuesta **DHCP Acknowledgement**.

El servidor tiene una tabla de direcciones IP disponibles y registra qué direcciones han sido asignadas y a qué clientes (identificados por su dirección MAC).

#### Proceso:

1. **DHCP Discover**: El servidor recibe una solicitud de un cliente buscando una IP.
2. **DHCP Offer**: El servidor responde ofreciendo una IP.
3. **DHCP Request**: El cliente solicita formalmente la IP ofrecida.
4. **DHCP Acknowledgement**: El servidor confirma la asignación de la IP.

#### Código:

Aquí está un resumen de las funciones clave:

- **`construct_dhcp_offer()`**: Construye el paquete de DHCP Offer que envía al cliente.
- **`send_dhcp_ack()`**: Envía la respuesta de DHCP Acknowledgement al cliente.
- **`assign_ip()`**: Asigna una IP al cliente basándose en su dirección MAC.

#### Opciones DHCP:

El servidor también incluye las siguientes opciones en sus respuestas:
- **Opción 1**: Máscara de red.
- **Opción 3**: Puerta de enlace predeterminada.
- **Opción 6**: Servidor DNS.

## `dhcp_client.c`

#### Funcionalidad:

El cliente DHCP envía un mensaje **DHCP Discover** en broadcast para encontrar un servidor DHCP. Una vez que recibe un **DHCP Offer**, el cliente analiza el paquete, muestra la dirección IP ofrecida y la información de red proporcionada (máscara de subred, puerta de enlace y servidor DNS). Posteriormente, envía un **DHCP Request** al servidor para confirmar la aceptación de la dirección IP, y recibe una respuesta final de **DHCP Acknowledgement** con la asignación definitiva.

#### Proceso:

1. **DHCP Discover**: El cliente envía un broadcast buscando servidores DHCP.
2. **DHCP Offer**: El cliente recibe una oferta con una IP y configuración de red.
3. **DHCP Request**: El cliente solicita formalmente la IP ofrecida.
4. **DHCP Acknowledgement**: El servidor confirma la asignación de la IP.

#### Código:

- **`construct_dhcp_discover()`**: Construye el paquete de DHCP Discover que envía al servidor.
- **`construct_dhcp_request()`**: Construye el paquete de DHCP Request para confirmar la IP.
- **`parse_dhcp_options()`**: Analiza las opciones del paquete DHCP Offer y extrae la máscara de red, puerta de enlace y servidor DNS.

### Compilación

Para compilar ambos archivos, necesitarás tener instalado un compilador C (como `gcc`).

#### Servidor DHCP

```bash
gcc -o dhcp_server dhcp_server.c
```

#### Cliente DHCP

```bash
gcc -o dhcp_client dhcp_client.c
```

### Ejecución

#### Ejecutar el Servidor DHCP

El servidor DHCP se ejecuta escuchando las solicitudes de los clientes. Debe ejecutarse como superusuario para poder abrir el puerto 67.

```bash
sudo ./dhcp_server
```

**Nota**: Si quieres probar el servidor en un entorno real, asegúrate de que no haya otro servidor DHCP activo en la red, ya que podrían interferir.

#### Ejecutar el Cliente DHCP

El cliente DHCP se ejecuta enviando una solicitud al servidor en el puerto 67 (de broadcast). Para ejecutarlo, también necesitas permisos de superusuario, ya que el cliente debe enviar paquetes de broadcast.

```bash
sudo ./dhcp_client
```

### Flujo Completo

1. El cliente envía un **DHCP Discover**.
2. El servidor responde con un **DHCP Offer**.
3. El cliente envía un **DHCP Request** solicitando la IP ofrecida.
4. El servidor confirma con un **DHCP Acknowledgement**.

#### dhcp_server.c

1. **Estructura `dhcp_packet`**: Representa un paquete DHCP con campos como direcciones IP, identificadores y opciones de DHCP.
2. **`construct_dhcp_offer()`**: Inicializa un paquete DHCP Offer, establece el tipo de mensaje a "Offer" y añade las opciones correspondientes (máscara de subred, puerta de enlace y DNS).
3. **`send_dhcp_ack()`**: Después de recibir un DHCP Request, este método envía un Acknowledgement que confirma la asignación de la IP.
4. **Asignación de IP**: La función `assign_ip()` asigna una dirección IP libre de una lista predefinida y la asocia con la dirección MAC del cliente.

#### dhcp_client.c

1. **Estructura `dhcp_packet`**: Similar al servidor, define un paquete DHCP con los campos necesarios.
2. **`construct_dhcp_discover()`**: Crea y envía un mensaje DHCP Discover en broadcast para buscar servidores DHCP en la red.
3. **`parse_dhcp_options()`**: Analiza las opciones recibidas en el DHCP Offer para extraer la máscara de subred, puerta de enlace y DNS.
4. **Renovación del lease**: Una vez que el cliente recibe una IP, si se acerca la expiración del lease, envía una solicitud de renovación.

### Problemas Comunes

- **Permisos**: Ambos programas requieren permisos de superusuario para abrir puertos privilegiados y enviar paquetes de broadcast.
- **Conflictos con otros servidores DHCP**: Si hay otro servidor DHCP en la red, podría interferir con el funcionamiento del servidor implementado.

