# Utilizar una imagen base de Linux con compilador de C
FROM gcc:latest

# Crear un directorio de trabajo
WORKDIR /usr/src/app

# Copiar los archivos de código fuente al contenedor
COPY dhcp_server.c ./
COPY dhcp_client.c ./

# Compilar el servidor y el cliente DHCP
RUN gcc -o dhcp_server dhcp_server.c
RUN gcc -o dhcp_client dhcp_client.c

# Comando por defecto: iniciar el servidor DHCP
CMD ["./dhcp_server"]
