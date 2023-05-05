#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/time.h>

#define BUF_SIZE 64000
#define SOCKET_PATH "/tmp/my_socket.sock"
#define SERVER_SOCKET_PATH "/tmp/uds_dgram_server"
#define CLIENT_SOCKET_PATH "/tmp/uds_dgram_client"
#define FILENAME "file.txt"

unsigned short calculate_checksum(unsigned short *paddress, int len);
int ipv4_tcp_client(int argc, char *argv[]);
int ipv4_tcp_server(int argc, char *argv[]);
int ipv4_udp_client(int argc, char *argv[]);
int ipv4_udp_server(int argc, char *argv[]);
int ipv6_tcp_client(int argc, char *argv[]);
int ipv6_tcp_server(int argc, char *argv[]);
int ipv6_udp_client(int argc, char *argv[]);
int ipv6_udp_server(int argc, char *argv[]);
int uds_stream_client(int argc, char *argv[]);
int uds_stream_server(int argc, char *argv[]);
int uds_dgram_client(int argc, char *argv[]);
int uds_dgram_server(int argc, char *argv[]);

int ipv4_tcp_client(int argc, char *argv[])
{
    FILE *fp;
    int sock = 0;
    int dataStream = -1, sendStream = 0, totalSent = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE] = {0};
    struct timeval start, end;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    // Set socket address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    // Convert IPv4 and store in sin_addr
    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to server socket
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    fp = fopen(FILENAME, "rb");
    if (fp == NULL)
    {
        printf("fopen() failed\n");
        exit(1);
    }

    printf("Connected to server\n");
    gettimeofday(&start, 0);
    while ((dataStream = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0)
    {

        sendStream = send(sock, buffer, dataStream, 0);
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        // printf("Bytes sent: %f\n", totalSent);
        // printf("location in file %ld\n", ftell(fp));
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + end.tv_usec - start.tv_usec / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);

    // Close socket
    close(sock);
    return 0;
}
int ipv4_tcp_server(int argc, char *argv[])
{
    int ServerSocket, ClientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddressLen;
    int opt = 1, bytes = 0, countbytes = 0;
    char buffer[BUF_SIZE] = {0};

    // Create server socket
    if ((ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("\nSocket creation error \n");
        return -1;
    }

    // Set socket options
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)))
    {
        printf("\nSetsockopt error \n");
        return -1;
    }

    memset(&serverAddr, '0', sizeof(serverAddr));

    // Set socket address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(atoi(argv[2]));

    // Bind socket to address
    if (bind(ServerSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("\nBind failed \n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(ServerSocket, 3) < 0)
    {
        printf("\nListen error \n");
        return -1;
    }
    printf("Server is listening for incoming connections...\n");

    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddressLen = sizeof(clientAddr);

    if ((ClientSocket = accept(ServerSocket, (struct sockaddr *)&clientAddr, &clientAddressLen)) < 0)
    {
        printf("\nAccept error \n");
        return -1;
    }

    printf("Client connected\n");

    while (1)
    {
        if ((bytes = recv(ClientSocket, buffer, sizeof(buffer), 0)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            close(ServerSocket);
            close(ClientSocket);
            return -1;
        }
        else if (countbytes && bytes == 0)
        {
            printf("Total bytes received: %d\n", countbytes);
            break;
        }

        countbytes += bytes;
    }

    // Close server socket
    close(ServerSocket);
    close(ClientSocket);
    return 0;
}

int ipv4_udp_client(int argc, char *argv[])
{
    FILE *fp;
    int sock = 0;
    int dataGram = -1, sendStream = 0, totalSent = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE] = {0};
    struct timeval start, end;
    const char *endMsg = "FILEEND";

    // Create socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    // Set socket address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    // Convert IPv4 and store in sin_addr
    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    fp = fopen(FILENAME, "rb");
    if (fp == NULL)
    {
        printf("fopen() failed\n");
        exit(1);
    }

    printf("Connected to server\n");

    gettimeofday(&start, 0);
    while ((dataGram = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0)
    {

        sendStream = sendto(sock, buffer, dataGram, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        // printf("Bytes sent: %f\n", totalSent);
        // printf("location in file %ld\n", ftell(fp));
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }
    gettimeofday(&end, 0);
    strcpy(buffer, endMsg);
    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + end.tv_usec - start.tv_usec / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);

    // Close socket
    close(sock);

    return 0;
}
int ipv4_udp_server(int argc, char *argv[])
{
    int ServerSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddressLen;
    int bytes = 0, countbytes = 0;
    char buffer[BUF_SIZE] = {0}, decoded[BUF_SIZE];

    // Create server socket
    if ((ServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        printf("\nSocket creation error \n");
        return -1;
    }

    memset(&serverAddr, '0', sizeof(serverAddr));
    memset(&clientAddr, '0', sizeof(clientAddr));

    // Set socket address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(atoi(argv[2]));

    // Bind socket to address
    if (bind(ServerSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("\nBind failed \n");
        return -1;
    }

    printf("Server is listening for incoming messages...\n");

    while (1)
    {
        if ((bytes = recvfrom(ServerSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &clientAddressLen)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            close(ServerSocket);
            return -1;
        }
        if (bytes < 10)
        {
            buffer[bytes] = '\0';
            strncpy(decoded, buffer, sizeof(decoded));
            if (strcmp(decoded, "FILEEND") == 0)
            {
                printf("Total bytes received from %s: %d\n", inet_ntoa(clientAddr.sin_addr), countbytes);
                break;
            }
        }

        countbytes += bytes;
    }

    close(ServerSocket);
    return 0;
}
int ipv6_tcp_client(int argc, char *argv[])
{
    FILE *fp;
    int sock = 0;
    int dataStream = -1, sendStream = 0, totalSent = 0;
    struct sockaddr_in6 serv_addr;
    char buffer[BUF_SIZE] = {0};
    struct timeval start, end;

    // Create socket
    if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    // Set socket address
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(atoi(argv[3]));

    // Convert IPv4 and store in sin_addr
    if (inet_pton(AF_INET6, argv[2], &serv_addr.sin6_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to server socket
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    fp = fopen(FILENAME, "rb");
    if (fp == NULL)
    {
        printf("fopen() failed\n");
        exit(1);
    }

    printf("Connected to server\n");
    gettimeofday(&start, 0);
    while ((dataStream = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0)
    {

        sendStream = send(sock, buffer, dataStream, 0);
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        // printf("Bytes sent: %f\n", totalSent);
        // printf("location in file %ld\n", ftell(fp));
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + end.tv_usec - start.tv_usec / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);

    // Close socket
    close(sock);
    return 0;

}
int ipv6_tcp_server(int argc, char *argv[])
{
    int ServerSocket, ClientSocket;
    struct sockaddr_in6 serverAddr, clientAddr;
    socklen_t clientAddressLen;
    int opt = 1, bytes = 0, countbytes = 0;
    char buffer[BUF_SIZE] = {0};

    // Create server socket
    if ((ServerSocket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
    {
        printf("\nSocket creation error \n");
        return -1;
    }

    // Set socket options
    if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)))
    {
        printf("\nSetsockopt error \n");
        return -1;
    }

    memset(&serverAddr, '0', sizeof(serverAddr));

    // Set socket address
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(atoi(argv[2]));

    // Bind socket to address
    if (bind(ServerSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("\nBind failed \n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(ServerSocket, 3) < 0)
    {
        printf("\nListen error \n");
        return -1;
    }
    printf("Server is listening for incoming connections...\n");

    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddressLen = sizeof(clientAddr);

    if ((ClientSocket = accept(ServerSocket, (struct sockaddr *)&clientAddr, &clientAddressLen)) < 0)
    {
        printf("\nAccept error \n");
        return -1;
    }

    printf("Client connected\n");

    while (1)
    {
        if ((bytes = recv(ClientSocket, buffer, sizeof(buffer), 0)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            close(ServerSocket);
            close(ClientSocket);
            return -1;
        }
        else if (countbytes && bytes == 0)
        {
            printf("Total bytes received: %d\n", countbytes);
            break;
        }

        countbytes += bytes;
    }

    // Close server socket
    close(ServerSocket);
    close(ClientSocket);
    return 0;
}
int ipv6_udp_client(int argc, char *argv[])
{
    int sock = 0, valread = -1;
    struct sockaddr_in6 serv_addr;
    char buffer[BUF_SIZE] = {0};
    socklen_t addr_len = sizeof(serv_addr);

    // Create socket
    if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    // Set socket address
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(atoi(argv[3]));

    // Convert IPv4 and store in sin_addr
    if (inet_pton(AF_INET6, argv[2], &serv_addr.sin6_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    printf("Connected to server\n");

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(buffer, BUF_SIZE, stdin);
        // Send input to server socket
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr, addr_len);

        memset(buffer, 0, BUF_SIZE);
        // Receive data from server socket
        valread = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr *)&serv_addr, &addr_len);
        if (valread == 0)
        {
            // Server disconnected
            printf("Server disconnected\n");
            break;
        }
        // Output received data to stdout
        printf("Answer: %s\n", buffer);
    }

    // Close socket
    close(sock);

    return 0;
}
int ipv6_udp_server(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_in6 address, cli_addr;
    socklen_t addr_len = sizeof(cli_addr);
    char buffer[BUF_SIZE] = {0};

    // Create server socket
    if ((server_fd = socket(AF_INET6, SOCK_DGRAM, 0)) == 0)
    {
        printf("\nSocket creation error \n");
        return -1;
    }

    memset(&address, '0', sizeof(address));

    // Set socket address
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(atoi(argv[2]));

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        printf("\nBind failed \n");
        return -1;
    }

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);
        // Receive data from client socket
        int valread = recvfrom(server_fd, buffer, BUF_SIZE, 0, (struct sockaddr *)&cli_addr, &addr_len);
        if (valread == 0)
        {
            // Client disconnected
            printf("Client disconnected\n");
            break;
        }
        // Output received data to stdout
        printf("Received: %s\n", buffer);
        memset(buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(buffer, BUF_SIZE, stdin);
        sendto(server_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&cli_addr, addr_len);
    }

    close(server_fd);
    return 0;
}
int uds_stream_client(int argc, char *argv[])
{
    int sock, valread;
    struct sockaddr_un server_address;
    char buffer[BUF_SIZE];

    // Create socket
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        printf("Failed to create client socket\n");
        return -1;
    }
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, SOCKET_PATH, sizeof(server_address.sun_path) - 1);
    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(struct sockaddr_un)) == -1)
    {
        printf("Failed to connect to server\n");
        return -1;
    }

    printf("Connected to server\n");
    while (1)
    {
        memset(buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(buffer, BUF_SIZE, stdin);
        // Send input to server socket
        send(sock, buffer, strlen(buffer), 0);

        memset(buffer, 0, BUF_SIZE);
        // Receive data from server socket
        valread = read(sock, buffer, BUF_SIZE);
        if (valread == 0)
        {
            // Server disconnected
            printf("Server disconnected\n");
            break;
        }
        // Output received data to stdout
        printf("Answer: %s\n", buffer);
    }

    // Close socket
    close(sock);

    return 0;
}
int uds_stream_server(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_un address;
    char buffer[BUF_SIZE];

    // Create server socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        printf("Failed to create server socket\n");
        return -1;
    }
    remove(SOCKET_PATH);
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_un)) == -1)
    {
        printf("Failed to bind server socket to address\n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1)
    {
        printf("Failed to listen for incoming connections\n");
        return -1;
    }

    printf("Server is listening for incoming connections...\n");

    // Accept incoming connections
    if ((client_fd = accept(server_fd, NULL, NULL)) == -1)
    {
        printf("Failed to accept incoming connection\n");
        return -1;
    }

    printf("Client connected to server\n");

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);

        // Receive data from client socket
        int valread = recv(client_fd, buffer, BUF_SIZE, 0);

        if (valread == 0)
        {
            // Client disconnected
            printf("Client disconnected\n");
            break;
        }

        // Output received data to stdout
        printf("Received: %s\n", buffer);

        memset(buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(buffer, BUF_SIZE, stdin);

        // Send input to client socket
        send(client_fd, buffer, strlen(buffer), 0);
    }

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}
int uds_dgram_client(int argc, char *argv[])
{
    int send_sock, recv_sock;
    struct sockaddr_un server_address, client_address;
    char send_buffer[BUF_SIZE], recv_buffer[BUF_SIZE];

    // Create sending socket
    if ((send_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        printf("Failed to create sending socket\n");
        return -1;
    }
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, SERVER_SOCKET_PATH, sizeof(server_address.sun_path) - 1);

    // Create receiving socket
    if ((recv_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        printf("Failed to create receiving socket\n");
        return -1;
    }
    memset(&client_address, 0, sizeof(struct sockaddr_un));
    client_address.sun_family = AF_UNIX;
    strncpy(client_address.sun_path, CLIENT_SOCKET_PATH, sizeof(client_address.sun_path) - 1);
    remove(client_address.sun_path);
    if (bind(recv_sock, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) == -1)
    {
        printf("Failed to bind receiving socket to address\n");
        return -1;
    }

    printf("Client started\n");
    while (1)
    {
        memset(send_buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(send_buffer, BUF_SIZE, stdin);

        // Send input to server socket
        sendto(send_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_address, sizeof(struct sockaddr_un));

        memset(recv_buffer, 0, BUF_SIZE);
        socklen_t len = sizeof(struct sockaddr_un);

        // Receive data from server socket
        int valread = recvfrom(recv_sock, recv_buffer, BUF_SIZE, 0, (struct sockaddr *)&server_address, &len);
        if (valread == -1)
        {
            printf("Failed to receive message\n");
            continue;
        }

        // Output received data to stdout
        printf("Answer: %s\n", recv_buffer);
    }

    // Close sockets
    close(send_sock);
    close(recv_sock);
    unlink(CLIENT_SOCKET_PATH);

    return 0;
}
int uds_dgram_server(int argc, char *argv[])
{
    int server_fd, client_fd, len;
    struct sockaddr_un server_addr, client_addr;
    char buffer[BUF_SIZE];

    // Create server socket
    if ((server_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        printf("Failed to create server socket\n");
        return -1;
    }

    remove(SERVER_SOCKET_PATH);
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Bind server socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) == -1)
    {
        printf("Failed to bind server socket to address\n");
        return -1;
    }

    printf("Server is waiting for incoming messages...\n");

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);

        // Receive message from client socket
        len = sizeof(struct sockaddr_un);
        if (recvfrom(server_fd, buffer, BUF_SIZE, 0, (struct sockaddr *)&client_addr, (socklen_t *restrict)&len) == -1)
        {
            printf("Failed to receive message from client\n");
            return -1;
        }

        // Output received message to stdout
        printf("Received message: %s\n", buffer);

        // Send response to client socket
        if ((client_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
        {
            printf("Failed to create client socket\n");
            return -1;
        }
        memset(&client_addr, 0, sizeof(struct sockaddr_un));
        client_addr.sun_family = AF_UNIX;
        strncpy(client_addr.sun_path, CLIENT_SOCKET_PATH, sizeof(client_addr.sun_path) - 1);
        memset(buffer, 0, BUF_SIZE);
        printf("Write: ");
        fgets(buffer, BUF_SIZE, stdin);
        if (sendto(client_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_un)) == -1)
        {
            printf("Failed to send message to client\n");
            return -1;
        }

        close(client_fd);
    }

    close(server_fd);
    unlink(SERVER_SOCKET_PATH);

    return 0;
}
int main(int argc, char *argv[])
{

    if (argc < 3 || argc > 7)
    {
        puts("Incorrect number of arguments\n");
        printf("Server Usage: ...\n");
        printf("Client Usage: ...\n");
        return -1;
    }

    if (!strcmp(argv[1], "-c"))
    {
        if (!strcmp(argv[4], "-p"))
        {
            if (!strcmp(argv[5], "ipv4"))
            {
                if (!strcmp(argv[6], "tcp"))
                {
                    ipv4_tcp_client(argc, argv);
                }
                else if (!strcmp(argv[6], "udp"))
                {
                    ipv4_udp_client(argc, argv);
                }
            }
            else if (!strcmp(argv[5], "ipv6"))
            {
                if (!strcmp(argv[6], "tcp"))
                {
                    ipv6_tcp_client(argc, argv);
                }
                else if (!strcmp(argv[6], "udp"))
                {
                    ipv6_udp_client(argc, argv);
                }
            }
            else if (!strcmp(argv[5], "uds"))
            {
                if (!strcmp(argv[6], "stream"))
                {
                    uds_stream_client(argc, argv);
                }
                else if (!strcmp(argv[6], "dgram"))
                {
                    uds_dgram_client(argc, argv);
                }
            }
            else if (!strcmp(argv[5], "mmap"))
            {
            }
            else if (!strcmp(argv[5], "pipe"))
            {
            }
        }
    }
    else if (!strcmp(argv[1], "-s"))
    {
        ipv6_tcp_server(argc, argv);
    }
    else
    {
        puts("Incorrect arguments\n");
        printf("Server Usage: ...\n");
        printf("Client Usage: ...\n");
    }
    return 0;
}

unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}