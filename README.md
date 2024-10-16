﻿# Proyecto de DHCP (Cliente y Servidor)

Este proyecto consiste en la implementación de un **servidor DHCP** y un **cliente DHCP** en lenguaje C. Ambos programas siguen el protocolo DHCP (Dynamic Host Configuration Protocol), utilizado para asignar direcciones IP de manera dinámica a dispositivos en una red.

## Archivos del Proyecto

1. `dhcp_server.c`: Implementación del servidor DHCP que escucha solicitudes de clientes, asigna direcciones IP y envía respuestas con información de configuración de red.
2. `dhcp_client.c`: Implementación del cliente DHCP que envía solicitudes al servidor DHCP y recibe la asignación de una dirección IP, junto con información de red como la máscara de subred, puerta de enlace y servidor DNS.
3. `dhcp_relay.c`: Implementación del relay DHCP que facilita la comunicación entre clientes y servidores que no están en el mismo segmento de red.

## `dhcp_server.c`

#### Funcionalidad:

El servidor DHCP escucha solicitudes de **DHCP Discover** de los clientes en el puerto 67 (puerto estándar para DHCP en IPv4). Cuando recibe una solicitud válida, responde con un **DHCP Offer**, ofreciendo una dirección IP y otros parámetros de red como la máscara de subred, puerta de enlace y servidor DNS. Luego, si el cliente responde con un **DHCP Request** para confirmar la oferta, el servidor asigna la dirección IP y envía una respuesta **DHCP Acknowledgement**.

El servidor tiene una tabla de direcciones IP disponibles y registra qué direcciones han sido asignadas y a qué clientes (identificados por su dirección MAC).

#### Proceso:

1. **DHCP Discover**: El servidor recibe una solicitud de un cliente buscando una IP.
2. **DHCP Offer**: El servidor responde ofreciendo una IP.
3. **DHCP Request**: El cliente solicita formalmente la IP ofrecida.
4. **DHCP Acknowledgement**: El servidor confirma la asignación de la IP.

## `dhcp_client.c`

#### Funcionalidad:

El cliente DHCP envía un mensaje **DHCP Discover** en broadcast para encontrar un servidor DHCP. Una vez que recibe un **DHCP Offer**, el cliente analiza el paquete, muestra la dirección IP ofrecida y la información de red proporcionada (máscara de subred, puerta de enlace y servidor DNS). Posteriormente, envía un **DHCP Request** al servidor para confirmar la aceptación de la dirección IP, y recibe una respuesta final de **DHCP Acknowledgement** con la asignación definitiva.

#### Proceso:

1. **DHCP Discover**: El cliente envía un broadcast buscando servidores DHCP.
2. **DHCP Offer**: El cliente recibe una oferta con una IP y configuración de red.
3. **DHCP Request**: El cliente solicita formalmente la IP ofrecida.
4. **DHCP Acknowledgement**: El servidor confirma la asignación de la IP.

## Preparativos

### Instalación de Herramientas de Red

Para poder configurar las interfaces de red como se requiere para la prueba de los programas DHCP, necesitas tener instalado `net-tools`. Para instalarlo en Ubuntu, ejecuta:

```bash
sudo apt update
sudo apt install net-tools
```

### Configuración de Interfaces de Red
Antes de ejecutar los programas, necesitarás configurar las interfaces de red en diferentes terminales para el relay DHCP y el servidor DHCP:

## Relay DHCP:
```bash
sudo ifconfig eth0:0 192.168.0.2 netmask 255.255.255.0 up
```

## Servidor DHCP:
```bash
sudo ifconfig eth0:1 192.168.0.1 netmask 255.255.255.0 up
```

## Cliente DHCP:
```bash
sudo ifconfig eth0 0.0.0.0
```

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

#### Relay DHCP

```bash
gcc -o dhcp_relay dhcp_relay.c
```

### Ejecución

#### Ejecutar el Servidor DHCP

El servidor DHCP se ejecuta escuchando las solicitudes de los clientes. Debe ejecutarse como superusuario para poder abrir el puerto 67.

```bash
sudo ./dhcp_server
```

#### Ejecutar el Cliente DHCP

El cliente DHCP se ejecuta enviando una solicitud al servidor en el puerto 67 (de broadcast). Para ejecutarlo, también necesitas permisos de superusuario, ya que el cliente debe enviar paquetes de broadcast.

```bash
sudo ./dhcp_client
```

### Ejecutar el Relay DHCP

El relay DHCP se ejecuta enrutando paquetes entre el cliente y el servidor DHCP. También debe ejecutarse con permisos de superusuario para poder recibir y enviar paquetes en los puertos de red necesarios.

```bash
sudo ./dhcp_relay
```
