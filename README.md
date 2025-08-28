# DHCP Project (Client and Server)

## Introduction

The goal of this project is to implement a **DHCP server** and a **DHCP client** using the C programming language. The DHCP (Dynamic Host Configuration Protocol) is essential in modern networks, as it enables the dynamic assignment of IP addresses and other network configuration parameters to client devices without manual configuration. This project aims to replicate the basic functionalities of the DHCP protocol, including handling key messages, lease management, concurrency to handle multiple requests, and managing duplicate requests based on clients' MAC addresses.

## Project Development

### Project Files

1. `dhcp_server.c`: Implementation of the DHCP server that listens for client requests, assigns IP addresses, and sends responses with network configuration information.
2. `dhcp_client.c`: Implementation of the DHCP client that sends requests to the DHCP server and receives an IP address assignment along with network information such as subnet mask, gateway, and DNS server.
3. `dhcp_relay.c`: Implementation of the DHCP relay that facilitates communication between clients and servers that are not on the same network segment.

### `dhcp_server.c`

The DHCP server listens for **DHCP Discover** requests from clients on port 67 (standard DHCP port for IPv4). When it receives a valid request, it responds with a **DHCP Offer**, offering an IP address and other network parameters such as subnet mask, gateway, and DNS server. If the client responds with a **DHCP Request** to confirm the offer, the server assigns the IP address and sends a **DHCP Acknowledgement** response.

The server maintains a table of available IP addresses and records which addresses have been assigned and to which clients (identified by their MAC address).

#### Implemented Features

- **Sending DHCPDISCOVER**: The client sends a DHCPDISCOVER message to the DHCP server to request an IP address.
- **Receiving DHCPOFFER**: The client receives a DHCPOFFER message from the server with an offered IP address and other network parameters.
- **Sending DHCPREQUEST**: The client sends a DHCPREQUEST message to formally request the offered IP address.
- **Receiving DHCPACK**: The client receives a DHCPACK message confirming the IP address assignment.
- **Lease Management**: The client manages the lease time and requests renewals when necessary.
- **IP Release**: Upon termination, the client releases the assigned IP address after the lease time expires.

#### Key Implementation Aspects

- **DHCP Packet Structure**: A `dhcp_packet` structure was defined to represent the standard format of a DHCP packet, including fields like `op`, `htype`, `hlen`, `xid`, `chaddr`, among others.
- **Unique XID Generation**: A random transaction identifier (`xid`) is used for each session, ensuring unique communications that can be correctly identified by the server.
- **DHCP Options Handling**: The client parses the options received in DHCP messages, such as subnet mask, default gateway, and DNS server.
- **Lease Time Management**: A loop implementation checks the remaining lease time and sends renewal requests before expiration.

### `dhcp_client.c`

!alt text

#### DHCP Client Methods:

- `construct_dhcp_discover()`: Builds a DHCP Discover packet for the client to search for a DHCP server.
- `construct_dhcp_request()`: Builds a DHCP Request packet to request an offered IP.
- `renew_lease()`: Sends a request to renew the lease of the offered IP.
- `parse_dhcp_options()`: Parses the options in the received DHCP packet (subnet mask, gateway, DNS).
- `print_ip_bytes()`: Helper function to print IP address bytes in a readable format.

The DHCP client sends a **DHCP Discover** broadcast message to find a DHCP server. Once it receives a **DHCP Offer**, the client parses the packet, displays the offered IP address and provided network information (subnet mask, gateway, and DNS server). Then, it sends a **DHCP Request** to the server to confirm acceptance of the IP address and receives a final **DHCP Acknowledgement** response with the definitive assignment.

!alt text

#### DHCP Client Methods:

- `construct_dhcp_discover()`: Builds a DHCP Discover packet for the client to search for a DHCP server.
- `construct_dhcp_request()`: Builds a DHCP Request packet to request an offered IP.
- `renew_lease()`: Sends a request to renew the lease of the offered IP.
- `parse_dhcp_options()`: Parses the options in the received DHCP packet (subnet mask, gateway, DNS).
- `print_ip_bytes()`: Helper function to print IP address bytes in a readable format.

#### Implemented Features

- **Listening for DHCP Requests**: The server listens on port 67 to receive DHCP messages from clients.
- **Dynamic IP Assignment**: Manages a user-defined IP address pool and dynamically assigns available addresses to clients.
- **DHCP Message Handling**: Processes the four main DHCP protocol messages: DHCPDISCOVER, DHCPOFFER, DHCPREQUEST, and DHCPACK.
- **Lease Management**: Controls the lease time of assigned IP addresses and releases IPs when the lease expires.
- **Concurrency**: Implements threads to handle multiple client requests simultaneously.
- **Duplicate Request Handling**: Checks if a MAC address already has an assigned IP to avoid duplicate assignments.

#### Key Implementation Aspects

- **IP Pool Structure**: An `ip_assignment` structure is defined to store the assigned IP, client MAC, lease start and duration, and `xid` to identify requests.
- **Mutex for Synchronization**: A mutex (`pthread_mutex_t pool_mutex`) is used to protect access to the IP pool and prevent race conditions in concurrent environments.
- **Threads for Concurrency**: Each incoming request is handled by a new thread created with `pthread_create`, allowing the server to serve multiple clients simultaneously.
- **Lease Management**: A periodic function checks and releases IPs whose leases have expired.
- **DHCP Message Handling**: Functions are implemented to build and send DHCPOFFER, DHCPACK, and DHCPNAK messages, following the protocol format and options.

### `dhcp_relay.c`

!alt text

#### DHCP Relay Methods:

- `get_dhcp_message_type()`: Iterates through DHCP packet options to get the message type (Discover, Request, Offer, etc.).
- `recvfrom()`: Receives a packet from the client or server.
- `sendto()`: Sends a packet to the client or server.
- `bind()`: Binds the relay socket to a specific address.

#### Implemented Features

- **DHCP Message Forwarding**: The relay receives DHCP messages from clients in one subnet and forwards them to the DHCP server in another subnet.
- **`giaddr` Field Modification**: The relay updates the `giaddr` (Gateway IP Address) field in DHCP packets to indicate the relay's address to the server.
- **Response Management**: Receives responses from the DHCP server and forwards them to the original client.

#### Aspectos Clave de la Implementación
-   **Socket UDP**: El relay utiliza sockets UDP para recibir y enviar paquetes DHCP.
-   **Análisis del Tipo de Mensaje DHCP**: Se implementa una función para extraer el tipo de mensaje DHCP de las opciones del paquete.
-   **Direcciones de Enlace**: Configura correctamente las direcciones y puertos para la comunicación entre el cliente, el relay y el servidor.

#### Key Implementation Aspects
- **UDP Socket**: The relay uses UDP sockets to receive and send DHCP packets.
- **DHCP Message Type Analysis**: A function is implemented to extract the DHCP message type from the packet options.
- **Link Addresses**: Properly configures the addresses and ports for communication between the client, the relay, and the server.

### Implemented DHCP Messages

#### Sequence Diagrams:
The DHCP Relay acts as an intermediary between the DHCP client and the DHCP server when they are on different subnets. It allows DHCP requests to traverse subnets, ensuring that a single DHCP server can manage IP addresses for multiple networks.

!alt text

1. **DHCP Discover**: The client sends this message to find a DHCP server.

- On the client side, the `construct_dhcp_discover()` function is used to build this message.
- The client sends the DHCP Discover to the relay. At the relay, this message is received using `recvfrom()`. Then, the relay forwards the DHCP Discover to the DHCP server using `sendto()`.

2. **DHCP Offer**: The server responds with a message offering an IP address to the client.

- On the server side, the `construct_dhcp_offer()` function is used to build this message and assign an IP.
- The DHCP server sends the DHCP Offer to the relay. The relay receives this message and forwards it to the client. At the relay, this process is handled using `recvfrom()` to receive the DHCP Offer from the server, and then `sendto()` to forward it to the client.

3. **DHCP Request**: The client formally requests the offered IP.

- On the client side, the `construct_dhcp_request()` function is used to request the IP offered in the DHCP Offer.
- The client sends the DHCP Request to the relay. The relay receives this message and forwards it to the DHCP server using the same `recvfrom()` and `sendto()` process.

4. **DHCP ACK**: The server confirms the IP assignment with an ACK message.

- On the server side, the `construct_dhcp_ack()` function is used to confirm the IP assignment to the client.
- The DHCP server sends the DHCP ACK to the relay. The relay receives this DHCP ACK and forwards it to the client using the same `recvfrom()` and `sendto()` functions.

### Concurrency

#### Handling Multiple Clients
The DHCP server is designed to handle multiple client requests simultaneously. This is crucial in network environments where several devices may be trying to obtain network configurations at the same time.

#### Thread Implementation
To efficiently manage multiple requests and maintain a smooth and responsive service, the `pthread` library is used. This library allows the creation of threads that operate independently for each client request. Each thread handles the full DHCP communication cycle for a specific client, from receiving the DHCPDISCOVER to sending the DHCPACK.

#### Synchronization
Since multiple threads may access and modify the IP address pool simultaneously, a lock/mutex (`pthread_mutex_t`) is used to synchronize access. The mutex ensures that only one thread can interact with the IP pool at any given time, preventing race conditions and ensuring data integrity.

### Lease Management

#### Lease Time
Each IP address assigned by the server has a defined lease time, which in this case is 60 seconds. This lease determines the period during which the client can use the IP address without needing renewal.

#### Renewal and Expiration
The server keeps track of the lease time for each assigned IP. Before a lease expires, the server expects to receive a DHCPREQUEST from the client requesting renewal. If no such request is received, the server releases the IP so it can be reassigned to another client.

#### Lease Update
When a DHCPREQUEST is received to renew an IP, the server updates the lease information associated with that IP in its pool. This includes resetting the lease time counter, allowing the client to continue using the IP for another full lease period.

### Handling Duplicate Requests (MAC)

#### MAC Verification
Before assigning a new IP to a client, the server checks whether the client's MAC address already has an assigned IP. This check prevents duplicate assignments and allows efficient management of the IP address pool.

#### IP Reuse
If a client with a known MAC requests an IP and already has one whose lease hasn't expired, the server simply reoffers the same IP.

#### xid Control
The server uses the transaction identifier (`xid`) along with the MAC address to detect and manage duplicate requests. This ensures that responses to old or repeated requests are not unnecessarily processed.

### Key Development Aspects

#### Socket Programming

For network communication, the project uses UDP sockets (`SOCK_DGRAM`). UDP sockets are ideal for the DHCP protocol due to their connectionless nature, allowing fast and efficient communication without the overhead of establishing and maintaining a connection. This characteristic is essential for DHCP, which needs to quickly handle large volumes of short, distributed requests.

#### Data Structures

Specific structures are defined for DHCP packets and IP assignments, reflecting the fields required by the DHCP protocol. This includes elements such as `op`, `htype`, `hlen`, `xid`, and more, which are crucial for the correct formatting and processing of DHCP messages.

#### DHCP Options Parsing

Functions are implemented to build and parse options within DHCP packets. This allows the server and client to handle flexible network configurations and provide functionalities such as DNS and gateway assignment.

#### Error Handling

The project includes robust error handling to manage situations such as receiving corrupted packets, lack of available IP addresses, and network errors. This ensures that the server can operate continuously and reliably.

#### Network Configuration

IP address ranges and ports are specifically configured to suit the project's needs, considering a controlled environment. This setup allows for simulating a realistic network environment and validating the behavior of the DHCP server and client.

## Achieved and Unachieved Aspects

### Achieved Aspects

- **Complete Implementation of Main DHCP Messages**: Successfully implemented DHCPDISCOVER, DHCPOFFER, DHCPREQUEST, and DHCPACK messages.
- **Dynamic IP Assignment**: The server dynamically assigns IP addresses to clients, managing an available IP pool.
- **Concurrency and Multi-Client Handling**: Thanks to thread implementation, the server can handle multiple requests simultaneously without blocking.
- **Lease Management**: Proper lease time management was implemented, including releasing expired addresses.
- **Duplicate Request Handling**: The server checks if a client already has an assigned IP and avoids duplicate assignments based on MAC address and `xid`.
- **DHCP Relay Implementation**: A relay was implemented to allow clients on different subnets to communicate with the DHCP server.

### Unachieved Aspects

- **Execution with Relay on AWS**: One of the unachieved aspects of this project was the proper deployment and configuration of the DHCP system components in a cloud environment, specifically on Amazon Web Services (AWS). Although direct communication between the DHCP client and server was successfully established on AWS, the configuration involving the DHCP relay could not be replicated successfully. This may be attributed to the additional complexities of network configuration in a cloud environment, where security policies, routing tables, and security groups significantly influence communication between instances.

## How to Run the Program?

### Prerequisites

#### Operating System

- A Unix-based operating system is required, such as Ubuntu or any other Linux distribution, since the setup instructions and commands are specific to these systems.

#### Required Tools

- **GCC or another C compiler**: Needed to compile the C programs. To install GCC on Ubuntu, run:
```bash
sudo apt update
sudo apt install build-essential
```

#### Network Tools Installation

To configure network interfaces as required for testing the DHCP programs, you need to have `net-tools` installed. To install it on Ubuntu, run:

```bash
sudo apt install net-tools
```

## Execution Order

It is crucial to start the components in the correct order to ensure that all elements of the DHCP system can communicate effectively:

1. **DHCP Relay**: Must be running before the server starts listening to ensure that any packet directed to the server via the relay is properly forwarded.
2. **DHCP Server**: Needs to be active before any client attempts to obtain an IP address.
3. **DHCP Client**: Should be the last to start, once the relay and server are ready to handle requests.


## Network Interface Configuration

Before running the programs, you’ll need to configure the network interfaces in different terminals for the DHCP relay and DHCP server:

### DHCP Relay:
```bash
sudo ifconfig eth0:0 192.168.0.2 netmask 255.255.255.0 up
```

### DHCP Server:
```bash
sudo ifconfig eth0:1 192.168.0.1 netmask 255.255.255.0 up
```

### DHCP Client:
```bash
sudo ifconfig eth0 0.0.0.0
```


### Compilation

To compile the files, you’ll need a C compiler (like `gcc`) installed.

#### DHCP Server

```bash
gcc -o dhcp_server dhcp_server.c
```

#### DHCP Client

```bash
gcc -o dhcp_client dhcp_client.c
```

#### DHCP Relay

```bash
gcc -o dhcp_relay dhcp_relay.c
```


### Execution

#### Run the DHCP Server

The DHCP server runs by listening for client requests. It must be executed as a superuser to open port 67.

```bash
sudo ./dhcp_server
```

#### Run the DHCP Client

The DHCP client sends a request to the server on port 67 (broadcast). It also requires superuser permissions to send broadcast packets.

```bash
sudo ./dhcp_client
```

#### Run the DHCP Relay

The DHCP relay routes packets between the client and the DHCP server. It must also be run with superuser permissions to receive and send packets on the required network ports.

```bash
sudo ./dhcp_relay
```


## Conclusions

The development of this project has been an enriching experience that provided deep insights into the internal workings of the DHCP protocol and network programming in C. Although initially challenging due to a lack of prior experience with sockets, threads, and communication protocols, the process enabled the acquisition of valuable knowledge in these areas.

Implementing a functional DHCP server and client required a detailed understanding of protocol messages, state management, and efficient resource handling in a concurrent environment. The use of threads and mutexes was essential to ensure the server could handle multiple requests safely and efficiently.

One of the most significant challenges was ensuring proper synchronization when accessing the IP address pool and avoiding race conditions. Additionally, handling duplicate requests and properly managing the leases of assigned IPs added an extra layer of complexity to the project.

Despite not implementing all advanced features of the DHCP protocol, a solid foundation was built that meets the main requirements and can be expanded in future work.


## References

- **Dynamic Host Configuration Protocol (DHCP) Basics**: Microsoft Docs
- **DHCP Configuration Guide**: Hewlett Packard Enterprise
- **DHCP Lease Time Explained**: ManageEngine OpUtils
- **Networking Basics**: Cisco Network Academy
