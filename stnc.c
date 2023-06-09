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
#include <sys/mman.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <openssl/sha.h>

#define SHM_FILE "/FileSM"
#define SHM_FILE_NAME "/FileName"
#define SHM_FILE_CS "/FileCS"
#define BUF_SIZE 64000
#define TCP_BUF_SIZE 1000000
#define FIFO_NAME "/tmp/myfifo"
#define DATA_SIZE 100000000
#define SOCKET_PATH "/tmp/my_socket.sock"
#define SERVER_SOCKET_PATH "/tmp/uds_dgram_server"
#define CLIENT_SOCKET_PATH "/tmp/uds_dgram_client"
enum addr
{
    IPV4,
    IPV6
};

// main functions
int client(int argc, char *argv[]);
int server(int argc, char *argv[]);
int tcp_client(int argc, char *argv[], enum addr type);
int tcp_server(int argc, char *argv[], enum addr type);
int udp_client(int argc, char *argv[], enum addr type);
int udp_server(int argc, char *argv[], enum addr type);
int uds_stream_client(int argc, char *argv[]);
int uds_stream_server(int argc, char *argv[]);
int uds_dgram_client(int argc, char *argv[]);
int uds_dgram_server(int argc, char *argv[]);
int mmap_client(int argc, char *argv[]);
int mmap_server(int argc, char *argv[]);
int pipe_client(int argc, char *argv[]);
int pipe_server(int argc, char *argv[]);

// aid functions
char *getServerType(int argc, char *argv[]);
int send_type_to_server(int argc, char *argv[], char *type);
char *generate_rand_str(int length);

int client(int argc, char *argv[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    if (inet_pton(AF_INET, (const char *)argv[2], &serv_addr.sin_addr) <= 0)
    {
        perror("address invalid or not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("connected to server\n");

    fd_set readfds;
    char message[BUF_SIZE];

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
        int res = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (res == -1)
        {
            perror("error in select");
            exit(EXIT_FAILURE);
        }
        else if (res == 0)
        {
            // puts("timeout");
            continue; // no data available from either socket or stdin
        }
        else
        {
            // printf("Select returned for fd: %d\n", res);
            if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                if (fgets(message, BUF_SIZE, stdin) == NULL)
                {
                    perror("error reading input");
                    exit(EXIT_FAILURE);
                }
                int bytes = send(sockfd, message, strlen(message), 0);
                if (bytes == -1)
                {
                    perror("error sending message");
                    exit(EXIT_FAILURE);
                }
                /*
                else
                {
                    printf("sent message to server: %s\n", message);
                }
                */
            }
            if (FD_ISSET(sockfd, &readfds))
            {
                int bytes = recv(sockfd, message, BUF_SIZE, 0);
                if (bytes == -1)
                {
                    perror("error receiving message");
                    exit(EXIT_FAILURE);
                }
                else if (bytes == 0)
                {
                    printf("server closed the connection\n");
                    break;
                }
                else
                {
                    message[bytes] = '\0';
                    printf("Server msg: %s", message);
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
int server(int argc, char *argv[])
{
    int server_fd, clientSocket;
    struct sockaddr_in address;
    int opt = 1;
    char buffer[BUF_SIZE] = {0};
    fd_set readfds;

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[2]));

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %s\n", argv[2]);

    // Accept and handle incoming connections
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddressLen = sizeof(clientAddress);
    clientSocket = accept(server_fd, (struct sockaddr *)&clientAddress, &clientAddressLen);
    if (clientSocket == -1)
    {
        printf("listen failed with error code : %d", errno);
        close(server_fd);
        close(clientSocket);
        return -1;
    }
    printf("Client connected: %s:%d\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(clientSocket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int maxfd = (server_fd > clientSocket) ? server_fd : clientSocket;
        int res = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (res == -1)
        {
            perror("error in select");
            exit(EXIT_FAILURE);
        }
        else if (res == 0)
        {
            continue;
        }
        else
        {
            if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                if (fgets(buffer, BUF_SIZE, stdin) == NULL)
                {
                    perror("error reading input");
                    exit(EXIT_FAILURE);
                }
                int bytes = send(clientSocket, buffer, strlen(buffer), 0);
                if (bytes == -1)
                {
                    perror("error sending message");
                    exit(EXIT_FAILURE);
                }
                /*
                else
                {
                    printf("sent message to client: %s\n", message);
                }
                */
            }

            if (FD_ISSET(clientSocket, &readfds))
            {
                int bytes = read(clientSocket, buffer, BUF_SIZE);
                if (bytes == -1)
                {
                    perror("error receiving message");
                    exit(EXIT_FAILURE);
                }
                else if (bytes == 0)
                {
                    // client closed the connection
                    printf("Client disconnected\n");
                    close(clientSocket);
                    FD_CLR(clientSocket, &readfds);
                    clientSocket = accept(server_fd, (struct sockaddr *)&clientAddress, &clientAddressLen);
                    if (clientSocket == -1)
                    {
                        printf("listen failed with error code : %d", errno);
                        close(server_fd);
                        close(clientSocket);
                        return -1;
                    }
                    printf("Client connected: %s:%d\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
                }
                else
                {
                    buffer[bytes] = '\0';
                    printf("Client msg: %s", buffer);
                }
            }
        }
    }
    return 0;
}
int tcp_client(int argc, char *argv[], enum addr type)
{
    char *serverType;
    if (type == IPV4)
    {
        serverType = "tcp4";
    }
    else
    {
        serverType = "tcp6";
    }
    send_type_to_server(argc, argv, serverType);

    int sock = 0;
    int sendStream = 0, totalSent = 0;
    char buffer[TCP_BUF_SIZE] = {0};
    struct sockaddr_in serv_addr4;
    struct sockaddr_in6 serv_addr6;
    struct timeval start, end;

    if (type == IPV4)
    {
        // Create socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        memset(&serv_addr4, 0, sizeof(serv_addr4));

        // Set socket address
        serv_addr4.sin_family = AF_INET;
        serv_addr4.sin_port = htons(atoi(argv[3]));

        // Convert IPv4 and store in sin_addr
        if (inet_pton(AF_INET, argv[2], &serv_addr4.sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        // Connect to server socket
        if (connect(sock, (struct sockaddr *)&serv_addr4, sizeof(serv_addr4)) < 0)
        {
            perror("\nConnection Failed \n");
            return -1;
        }
    }
    else if (type == IPV6)
    {
        // Create socket
        if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        memset(&serv_addr6, '0', sizeof(serv_addr6));

        // Set socket address
        serv_addr6.sin6_family = AF_INET6;
        serv_addr6.sin6_port = htons(atoi(argv[3]));

        // Convert IPv4 and store in sin_addr
        if (inet_pton(AF_INET6, argv[2], &serv_addr6.sin6_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        // Connect to server socket
        if (connect(sock, (struct sockaddr *)&serv_addr6, sizeof(serv_addr6)) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }
    }
    else
    {
        printf("Invalid address type\n");
        return -1;
    }

    printf("Connected to server\n");

    // Generate data
    char *data = generate_rand_str(DATA_SIZE);

    // Calculate and send checksum
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    hash_str[SHA_DIGEST_LENGTH * 2] = '\0';
    sendStream = send(sock, hash_str, strlen(hash_str), 0);
    if (-1 == sendStream)
    {
        printf("send() failed");
        close(sock);
        exit(1);
    }

    gettimeofday(&start, 0);
    while (totalSent < strlen(data))
    {
        int bytes_to_read = (TCP_BUF_SIZE < strlen(data) - totalSent) ? TCP_BUF_SIZE : strlen(data) - totalSent;
        memcpy(buffer, data + totalSent, bytes_to_read);
        sendStream = send(sock, buffer, bytes_to_read, 0);
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        // printf("Bytes sent: %d\n", totalSent);
        // printf ("bytes to read: %d\n", bytes_to_read);
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);
    // Close socket
    close(sock);
    free(data);
    return 0;
}
int tcp_server(int argc, char *argv[], enum addr type)
{
    int ServerSocket, ClientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    struct sockaddr_in6 serverAddr6, clientAddr6;
    socklen_t clientAddressLen;
    int opt = 1, bytes = -1, countbytes = 0;
    char buffer[TCP_BUF_SIZE] = {0}, *totalData = malloc(DATA_SIZE);
    struct timeval start, end;

    if (type == IPV4)
    {
        // Create server socket
        if ((ServerSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            printf("\nSocket creation error \n");
            return -1;
        }

        memset(&serverAddr, '0', sizeof(serverAddr));
        memset(&clientAddr, '0', sizeof(clientAddr));
        clientAddressLen = sizeof(clientAddr);

        // Set socket address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(atoi(argv[2]));

        // Set socket options
        if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        {
            printf("\nSetsockopt error \n");
            return -1;
        }

        // Bind socket to address
        if (bind(ServerSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            perror("\nTCP bind failed \n");
            return -1;
        }
    }
    else if (type == IPV6)
    {
        // Create server socket
        if ((ServerSocket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
        {
            printf("\nSocket creation error \n");
            return -1;
        }

        memset(&serverAddr6, '0', sizeof(serverAddr6));
        memset(&clientAddr6, '0', sizeof(clientAddr6));
        clientAddressLen = sizeof(clientAddr6);

        // Set socket address
        serverAddr6.sin6_family = AF_INET6;
        serverAddr6.sin6_addr = in6addr_any;
        serverAddr6.sin6_port = htons(atoi(argv[2]));

        // Set socket options
        if (setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        {
            printf("\nSetsockopt error \n");
            return -1;
        }

        // Bind socket to address
        if (bind(ServerSocket, (struct sockaddr *)&serverAddr6, sizeof(serverAddr6)) < 0)
        {
            printf("\nTCP bind failed \n");
            return -1;
        }
    }
    else
    {
        printf("Invalid address type\n");
        return -1;
    }

    // Listen for incoming connections
    if (listen(ServerSocket, 3) < 0)
    {
        printf("\nListen error \n");
        return -1;
    }
    // printf("Server is listening for incoming connections...\n");

    if (type == IPV4)
    {
        if ((ClientSocket = accept(ServerSocket, (struct sockaddr *)&clientAddr, &clientAddressLen)) < 0)
        {
            printf("\nAccept error \n");
            return -1;
        }
    }
    else if (type == IPV6)
    {
        if ((ClientSocket = accept(ServerSocket, (struct sockaddr *)&clientAddr6, &clientAddressLen)) < 0)
        {
            printf("\nAccept error \n");
            return -1;
        }
    }

    // Receive checksum
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    bytes = recv(ClientSocket, hash_str, sizeof(hash_str), 0);
    if (bytes < 0)
    {
        printf("recv failed. Sender inactive.\n");
        close(ServerSocket);
        close(ClientSocket);
        return -1;
    }
    unsigned char recv_hash[SHA_DIGEST_LENGTH];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sscanf(&hash_str[i * 2], "%2hhx", &recv_hash[i]);
    }

    gettimeofday(&start, 0);
    while (bytes != 0)
    {
        if ((bytes = recv(ClientSocket, buffer, sizeof(buffer), 0)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            // close(ServerSocket);
            // close(ClientSocket);
            return -1;
        }
        memcpy(totalData + countbytes, buffer, bytes);
        countbytes += bytes;
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    // Calculate checksum
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)totalData, strlen(totalData), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != recv_hash[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }

    if (type == IPV4)
    {
        printf("ipv4_tcp,%lu\n", miliseconds);
    }
    else
    {
        printf("ipv6_tcp,%lu\n", miliseconds);
    }

    // Close server socket
    close(ServerSocket);
    close(ClientSocket);
    free(totalData);
    return 0;
}
int udp_client(int argc, char *argv[], enum addr type)
{
    char *serverType;
    if (type == IPV4)
    {
        serverType = "udp4";
    }
    else
    {
        serverType = "udp6";
    }
    send_type_to_server(argc, argv, serverType);

    int sock = 0;
    int sendStream = 0, totalSent = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in6 serv_addr6;
    char buffer[BUF_SIZE] = {0};
    struct timeval start, end;
    const char *endMsg = "END";

    if (type == IPV4)
    {
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
    }
    else if (type == IPV6)
    {
        // Create socket
        if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        memset(&serv_addr6, '0', sizeof(serv_addr6));
        // Set socket address
        serv_addr6.sin6_family = AF_INET6;
        serv_addr6.sin6_port = htons(atoi(argv[3]));

        // Convert IPv4 and store in sin_addr
        if (inet_pton(AF_INET6, argv[2], &serv_addr6.sin6_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
    }
    else
    {
        printf("Invalid address type\n");
        return -1;
    }

    // Generate data
    char *data = generate_rand_str(DATA_SIZE);

    // Calculate and send checksum
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    hash_str[SHA_DIGEST_LENGTH * 2] = '\0';
    if (type == IPV4)
    {
        sendStream = sendto(sock, hash_str, strlen(hash_str), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }
    else if (type == IPV6)
    {
        sendStream = sendto(sock, hash_str, strlen(hash_str), 0, (struct sockaddr *)&serv_addr6, sizeof(serv_addr6));
    }
    if (-1 == sendStream)
    {
        printf("send() failed");
        exit(1);
    }

    gettimeofday(&start, 0);
    int i = 0;
    while (totalSent < strlen(data))
    {
        int bytes_to_read = (BUF_SIZE < strlen(data) - totalSent) ? BUF_SIZE : strlen(data) - totalSent;
        memcpy(buffer, data + totalSent, bytes_to_read);
        if (type == IPV4)
        {
            sendStream = sendto(sock, buffer, bytes_to_read, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        }
        else if (type == IPV6)
        {
            sendStream = sendto(sock, buffer, bytes_to_read, 0, (struct sockaddr *)&serv_addr6, sizeof(serv_addr6));
        }
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        if (i % 200 == 0 || bytes_to_read < BUF_SIZE)
        {
            printf("Total bytes sent: %d\n", totalSent);
        }
        // printf ("bytes to read: %d\n", bytes_to_read);
        sendStream = 0;
        i++;
        bzero(buffer, sizeof(buffer));
    }

    gettimeofday(&end, 0);
    strcpy(buffer, endMsg);
    if (type == IPV4)
    {
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }
    else if (type == IPV6)
    {
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr6, sizeof(serv_addr6));
    }

    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("Time elapsed: %lu miliseconds\n", miliseconds);

    // Close socket
    close(sock);
    free(data);
    return 0;
}
int udp_server(int argc, char *argv[], enum addr type)
{
    struct timeval start, end;
    int ServerSocket;
    struct sockaddr_in serverAddr, clientAddr;
    struct sockaddr_in6 serverAddr6, clientAddr6;
    socklen_t clientAddressLen;
    int bytes = 0, countbytes = 0;
    char buffer[BUF_SIZE] = {0}, decoded[BUF_SIZE], *totalData = malloc(DATA_SIZE);

    if (type == IPV4)
    {
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
    }
    else if (type == IPV6)
    {
        // Create server socket
        if ((ServerSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == 0)
        {
            printf("\nSocket creation error \n");
            return -1;
        }

        memset(&serverAddr6, '0', sizeof(serverAddr6));
        memset(&clientAddr6, '0', sizeof(clientAddr6));

        // Set socket address
        serverAddr6.sin6_family = AF_INET6;
        serverAddr6.sin6_addr = in6addr_any;
        serverAddr6.sin6_port = htons(atoi(argv[2]));

        // Bind socket to address
        if (bind(ServerSocket, (struct sockaddr *)&serverAddr6, sizeof(serverAddr6)) < 0)
        {
            printf("\nBind failed \n");
            return -1;
        }
    }
    else
    {
        printf("Invalid address type\n");
        return -1;
    }

    // Receive checksum
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    if (type == IPV4)
    {
        bytes = recvfrom(ServerSocket, hash_str, sizeof(hash_str), 0, (struct sockaddr *)&clientAddr, &clientAddressLen);
    }
    else if (type == IPV6)
    {
        bytes = recvfrom(ServerSocket, hash_str, sizeof(hash_str), 0, (struct sockaddr *)&clientAddr6, &clientAddressLen);
    }
    if (bytes < 0)
    {
        printf("recv failed. Sender inactive.\n");
        close(ServerSocket);
        return -1;
    }
    unsigned char recv_hash[SHA_DIGEST_LENGTH];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sscanf(&hash_str[i * 2], "%2hhx", &recv_hash[i]);
    }

    // printf("Server is listening for incoming messages...\n");
    gettimeofday(&start, 0);
    while (1)
    {
        if (type == IPV4)
        {
            if ((bytes = recvfrom(ServerSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &clientAddressLen)) < 0)
            {
                printf("recv failed. Sender inactive.\n");
                close(ServerSocket);
                return -1;
            }
        }
        else if (type == IPV6)
        {
            if ((bytes = recvfrom(ServerSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr6, &clientAddressLen)) < 0)
            {
                printf("recv failed. Sender inactive.\n");
                close(ServerSocket);
                return -1;
            }
        }
        if (bytes < 10)
        {
            buffer[bytes] = '\0';
            strncpy(decoded, buffer, sizeof(decoded));
            if (strcmp(decoded, "END") == 0)
            {
                // printf("Total bytes received: %d\n", countbytes);
                break;
            }
        }
        memcpy(totalData + countbytes, buffer, bytes);
        countbytes += bytes;
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    // Calculate checksum
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)totalData, strlen(totalData), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != recv_hash[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }

    if (type == IPV4)
    {
        printf("ipv4_udp,%lu\n", miliseconds);
    }
    else
    {
        printf("ipv6_udp,%lu\n", miliseconds);
    }
    close(ServerSocket);
    free(totalData);
    return 0;
}
int uds_stream_client(int argc, char *argv[])
{
    char *serverType = "udss";
    send_type_to_server(argc, argv, serverType);
    int sock = 0;
    int valread = -1, sendStream = 0, totalSent = 0;
    valread++;
    struct sockaddr_un server_address;
    char buffer[TCP_BUF_SIZE];
    struct timeval start, end;

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
        perror("Failed to connect to server\n");
        return -1;
    }

    printf("Connected to server\n");

    // Generate data
    char *data = generate_rand_str(DATA_SIZE);

    // Calculate and send checksum
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    hash_str[SHA_DIGEST_LENGTH * 2] = '\0';
    sendStream = send(sock, hash_str, strlen(hash_str), 0);
    if (-1 == sendStream)
    {
        printf("send() failed");
        close(sock);
        exit(1);
    }

    gettimeofday(&start, 0);
    while (totalSent < strlen(data))
    {
        int bytes_to_read = (TCP_BUF_SIZE < strlen(data) - totalSent) ? TCP_BUF_SIZE : strlen(data) - totalSent;
        memcpy(buffer, data + totalSent, bytes_to_read);
        sendStream = send(sock, buffer, bytes_to_read, 0);
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        // printf("Bytes sent: %d\n", totalSent);
        // printf ("bytes to read: %d\n", bytes_to_read);
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);

    // Close socket
    close(sock);
    free(data);
    return 0;
}
int uds_stream_server(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_un address;
    char buffer[TCP_BUF_SIZE], *totalData = malloc(DATA_SIZE);
    struct timeval start, end;
    int bytes = 0, countbytes = 0;

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

    // Accept incoming connections
    if ((client_fd = accept(server_fd, NULL, NULL)) == -1)
    {
        printf("Failed to accept incoming connection\n");
        return -1;
    }

    // Receive checksum
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    bytes = recv(client_fd, hash_str, sizeof(hash_str), 0);
    if (bytes < 0)
    {
        printf("recv failed. Sender inactive.\n");
        close(server_fd);
        close(client_fd);
        return -1;
    }
    unsigned char recv_hash[SHA_DIGEST_LENGTH];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sscanf(&hash_str[i * 2], "%2hhx", &recv_hash[i]);
    }

    // Receive data
    gettimeofday(&start, 0);
    while (1)
    {
        if ((bytes = recv(client_fd, buffer, sizeof(buffer), 0)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            close(server_fd);
            close(client_fd);
            return -1;
        }
        else if (countbytes && bytes == 0)
        {
            // printf("Total bytes received: %d\n", countbytes);
            break;
        }
        memcpy(totalData + countbytes, buffer, bytes);
        countbytes += bytes;
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    // Calculate checksum
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)totalData, strlen(totalData), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != recv_hash[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }

    printf("uds_stream,%lu\n", miliseconds);
    close(client_fd);
    close(server_fd);
    free(totalData);
    unlink(SOCKET_PATH);
    return 0;
}
int uds_dgram_client(int argc, char *argv[])
{

    int sendStream = 0, totalSent = 0;
    char buffer[BUF_SIZE] = {0};
    struct timeval start, end;
    char *serverType = "udsd";
    const char *endMsg = "END";
    send_type_to_server(argc, argv, serverType);

    int sock;
    struct sockaddr_un server_addr, client_address;
    // char send_buffer[BUF_SIZE], recv_buffer[BUF_SIZE];

    // Create sending socket
    if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        printf("Failed to create sending socket\n");
        return -1;
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    // Create receiving socket
    memset(&client_address, 0, sizeof(struct sockaddr_un));
    client_address.sun_family = AF_UNIX;
    strncpy(client_address.sun_path, CLIENT_SOCKET_PATH, sizeof(client_address.sun_path) - 1);
    remove(client_address.sun_path);
    printf("Client started\n");

    // Generate data
    char *data = generate_rand_str(DATA_SIZE);

    // Calculate and send checksum
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    hash_str[SHA_DIGEST_LENGTH * 2] = '\0';
    sendStream = sendto(sock, hash_str, strlen(hash_str), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (-1 == sendStream)
    {
        printf("send() failed");
        exit(1);
    }

    // Send data
    int i = 0;
    gettimeofday(&start, 0);
    while (totalSent < strlen(data))
    {
        int bytes_to_read = (BUF_SIZE < strlen(data) - totalSent) ? BUF_SIZE : strlen(data) - totalSent;
        memcpy(buffer, data + totalSent, bytes_to_read);
        sendStream = sendto(sock, buffer, bytes_to_read, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (-1 == sendStream)
        {
            printf("send() failed");
            exit(1);
        }

        totalSent += sendStream;
        if (i % 200 == 0 || bytes_to_read < BUF_SIZE)
        {
            printf("Total bytes sent: %d\n", totalSent);
        }
        i++;
        // printf ("bytes to read: %d\n", bytes_to_read);
        sendStream = 0;
        bzero(buffer, sizeof(buffer));
    }

    gettimeofday(&end, 0);
    strcpy(buffer, endMsg);
    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("Total bytes sent: %d\nTime elapsed: %lu miliseconds\n", totalSent, miliseconds);
    // Close sockets
    close(sock);
    // close(recv_sock);
    unlink(CLIENT_SOCKET_PATH);
    return 0;
}
int uds_dgram_server(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_un server_addr, client_addr;
    int bytes = 0, countbytes = 0;
    char buffer[BUF_SIZE] = {0}, decoded[BUF_SIZE], *totalData = malloc(DATA_SIZE);
    struct timeval start, end;

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
    socklen_t clientAddressLen;

    // Receive checksum
    char hash_str[SHA_DIGEST_LENGTH * 2 + 1];
    bytes = recvfrom(server_fd, hash_str, sizeof(hash_str), 0, (struct sockaddr *)&client_addr, &clientAddressLen);
    if (bytes < 0)
    {
        printf("recv failed. Sender inactive.\n");
        close(server_fd);
        return -1;
    }
    unsigned char recv_hash[SHA_DIGEST_LENGTH];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sscanf(&hash_str[i * 2], "%2hhx", &recv_hash[i]);
    }

    // Receive data
    gettimeofday(&start, 0);
    while (1)
    {

        if ((bytes = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &clientAddressLen)) < 0)
        {
            printf("recv failed. Sender inactive.\n");
            close(server_fd);
            return -1;
        }

        if (bytes < 10)
        {
            buffer[bytes] = '\0';
            strncpy(decoded, buffer, sizeof(decoded));
            if (strcmp(decoded, "END") == 0)
            {
                // printf("Total bytes received: %d\n", countbytes);
                break;
            }
        }
        memcpy(totalData + countbytes, buffer, bytes);
        countbytes += bytes;
    }
    gettimeofday(&end, 0);
    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    // Calculate checksum
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)totalData, strlen(totalData), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != recv_hash[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }

    printf("uds_dgram,%lu\n", miliseconds);
    close(server_fd);
    free(totalData);
    unlink(SERVER_SOCKET_PATH);

    return 0;
}
int mmap_client(int argc, char *argv[])
{
    char *ServerType = "mmap";

    // Generate file
    char *data = generate_rand_str(DATA_SIZE);
    int dataLen = strlen(data);

    FILE *fp = fopen(argv[6], "w");
    if (fp == NULL)
    {
        printf("Error opening file!\n");
        return -1;
    }
    fprintf(fp, "%s", data);
    fclose(fp);

    int fd = open(argv[6], O_RDONLY);
    if (fd == -1)
    {
        printf("open() failed\n");
        return -1;
    }

    void *addr = mmap(NULL, dataLen, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }
    close(fd);

    // Calculate and save checksum
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);
    free(data);

    int shm_fd = shm_open(SHM_FILE, O_CREAT | O_RDWR, 0666);
    int shm_fd_name = shm_open(SHM_FILE_NAME, O_CREAT | O_RDWR, 0666);
    int shm_fd_checksum = shm_open(SHM_FILE_CS, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1 || shm_fd_name == -1 || shm_fd_checksum == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }

    int res = ftruncate(shm_fd, dataLen);
    int res2 = ftruncate(shm_fd_name, strlen(argv[6]));
    int res3 = ftruncate(shm_fd_checksum, sizeof(hash));
    if (res == -1 || res2 == -1 || res3 == -1)
    {
        printf("ftruncate() failed\n");
        return -1;
    }

    void *shm_addr = mmap(NULL, dataLen, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    void *shm_name_addr = mmap(NULL, strlen(argv[6]), PROT_WRITE, MAP_SHARED, shm_fd_name, 0);
    void *shm_checksum_addr = mmap(NULL, sizeof(hash), PROT_WRITE, MAP_SHARED, shm_fd_checksum, 0);
    if (shm_addr == MAP_FAILED || shm_name_addr == MAP_FAILED || shm_checksum_addr == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }

    memcpy(shm_addr, addr, dataLen);
    memcpy(shm_name_addr, argv[6], strlen(argv[6]));
    memcpy(shm_checksum_addr, &hash, sizeof(hash));

    munmap(addr, dataLen);
    munmap(shm_addr, dataLen);
    munmap(shm_name_addr, strlen(argv[6]));
    munmap(shm_checksum_addr, sizeof(hash));
    close(shm_fd);
    close(shm_fd_name);
    close(shm_fd_checksum);

    send_type_to_server(argc, argv, ServerType);
    return 0;
}
int mmap_server(int argc, char *argv[])
{
    // Get SM object of file
    struct timeval start, end;
    gettimeofday(&start, 0);
    int shm_fd = shm_open(SHM_FILE, O_RDONLY, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }
    struct stat sb;
    if (fstat(shm_fd, &sb) == -1)
    {
        printf("fstat() failed\n");
        return -1;
    }
    int len = sb.st_size;

    void *addr = mmap(NULL, len, PROT_READ, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (addr == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }

    // fwrite(addr, len, 1, stdout);
    // printf("Shared memory size: %ld\n", len);

    char *recv_data = malloc(len);
    char *pdata = (char *)addr;
    for (int i = 0; i < len; i++)
    {
        recv_data[i] = pdata[i];
    }
    gettimeofday(&end, 0);
    // printf("Total bytes received: %ld\n", strlen(recv_buffer));

    // Get checksum
    int shm_fd_checksum = shm_open(SHM_FILE_CS, O_RDONLY, 0666);
    if (shm_fd_checksum == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }
    struct stat sb_checksum;
    if (fstat(shm_fd_checksum, &sb_checksum) == -1)
    {
        printf("fstat() failed\n");
        return -1;
    }
    int len_checksum = sb_checksum.st_size;
    void *addr_checksum = mmap(NULL, len_checksum, PROT_READ, MAP_SHARED, shm_fd_checksum, 0);
    if (addr_checksum == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }
    close(shm_fd_checksum);
    

    // Calculate and Compare Checksums
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)recv_data, strlen(recv_data), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != ((unsigned char *)addr_checksum)[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }
    shm_unlink(SHM_FILE_CS);
    free(recv_data);

    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("mmap,%lu\n", miliseconds);

    munmap(addr, len);
    if (shm_unlink(SHM_FILE) == -1)
    {
        printf("shm_unlink() failed\n");
        return -1;
    }

    // Get file name and remove it
    int shm_fd_name = shm_open(SHM_FILE_NAME, O_RDONLY, 0666);
    if (shm_fd_name == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }
    struct stat sb_name;
    if (fstat(shm_fd_name, &sb_name) == -1)
    {
        printf("fstat() failed\n");
        return -1;
    }
    off_t len_name = sb_name.st_size;
    void *addr_name = mmap(NULL, len_name, PROT_READ, MAP_SHARED, shm_fd_name, 0);
    if (addr_name == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }
    close(shm_fd_name);

    int status = remove((char *)addr_name);
    if (status != 0)
    {
        printf("Unable to delete the file\n");
    }

    munmap(addr_name, len_name);
    if (shm_unlink(SHM_FILE_NAME) == -1)
    {
        printf("shm_unlink() failed\n");
        return -1;
    }

    return 0;
}

int pipe_client(int argc, char *argv[])
{
    int fifo_fd;
    char buf[TCP_BUF_SIZE];
    int bytes_read;
    char *data = generate_rand_str(DATA_SIZE);

    // Calculate and save checksum into shared memory
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, strlen(data), hash);

    int shm_fd_checksum = shm_open(SHM_FILE_CS, O_CREAT | O_RDWR, 0666);
    if (shm_fd_checksum == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }

    int res = ftruncate(shm_fd_checksum, sizeof(hash));
    if (res == -1)
    {
        printf("ftruncate() failed\n");
        return -1;
    }

    void *shm_checksum_addr = mmap(NULL, sizeof(hash), PROT_WRITE, MAP_SHARED, shm_fd_checksum, 0);
    if (shm_checksum_addr == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }
    memcpy(shm_checksum_addr, &hash, sizeof(hash));
    munmap(shm_checksum_addr, sizeof(hash));
    close(shm_fd_checksum);

    // Send type to server
    send_type_to_server(argc, argv, "pipe");

    // Open the FIFO for writing
    fifo_fd = open(FIFO_NAME, O_WRONLY);
    if (fifo_fd < 0)
    {
        perror("Error: Could not open FIFO\n");
        exit(1);
    }

    // Write data to FIFO
    FILE *fpw = fopen(argv[6], "w");
    if (fpw == NULL)
    {
        printf("Error opening file!\n");
        return -1;
    }
    fprintf(fpw, "%s", data);
    fclose(fpw);
    FILE *fp = fopen(argv[6], "r");
    if (fp == NULL)
    {
        printf("Error opening file!\n");
        return -1;
    }
    while ((bytes_read = fread(buf, 1, TCP_BUF_SIZE, fp)) > 0)
    {
        if (write(fifo_fd, buf, bytes_read) < 0)
        {
            fprintf(stderr, "Error: Could not write to FIFO\n");
            exit(1);
        }
    }
    if (remove(argv[6]) != 0)
    {
        printf("File %s was not deleted.\n", argv[6]);
    }
    close(fifo_fd);
    free(data);
    fclose(fp);
    return 0;
}

int pipe_server(int argc, char *argv[])
{
    int fifo_fd, countbytes = 0;
    char buf[TCP_BUF_SIZE], *totalData = malloc(DATA_SIZE);
    struct timeval start, end;
    mkfifo(FIFO_NAME, 0666);
    fifo_fd = open(FIFO_NAME, O_RDONLY);
    if (fifo_fd < 0)
    {
        fprintf(stderr, "Error: Could not open FIFO\n");
        exit(1);
    }
    int bytes_read = 0;

    // Get checksum
    int shm_fd_checksum = shm_open(SHM_FILE_CS, O_RDONLY, 0666);
    if (shm_fd_checksum == -1)
    {
        perror("shm_open() failed\n");
        return -1;
    }
    struct stat sb_checksum;
    if (fstat(shm_fd_checksum, &sb_checksum) == -1)
    {
        printf("fstat() failed\n");
        return -1;
    }
    int len_checksum = sb_checksum.st_size;
    void *addr_checksum = mmap(NULL, len_checksum, PROT_READ, MAP_SHARED, shm_fd_checksum, 0);
    if (addr_checksum == MAP_FAILED)
    {
        printf("mmap() failed\n");
        return -1;
    }
    close(shm_fd_checksum);

    // read from FIFO
    gettimeofday(&start, 0);
    while ((bytes_read = read(fifo_fd, buf, TCP_BUF_SIZE)) > 0)
    {
        memcpy(totalData + countbytes, buf, bytes_read);
        countbytes += bytes_read;
        // printf("TotalData size: %ld\n", strlen(totalData));
    }
    gettimeofday(&end, 0);

    // Calculate and Compare Checksums
    unsigned char calculated_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)totalData, strlen(totalData), calculated_hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        if (calculated_hash[i] != ((unsigned char *)addr_checksum)[i])
        {
            printf("Checksums don't match\n");
            break;
        }
    }

    unsigned long miliseconds = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    printf("pipe,%lu\n", miliseconds);
    close(fifo_fd);
    unlink(FIFO_NAME);
    shm_unlink(SHM_FILE_CS);
    free(totalData);
    return 0;
}

int send_type_to_server(int argc, char *argv[], char *type)
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *bufferser = type;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));

    // Set server address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        return -1;
    }
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("\nConnection Failed\n");
        close(sock);
        return -1;
    }
    send(sock, bufferser, strlen(bufferser), 0);
    close(sock);
    usleep(50000);
    return 0;
}

char *getServerType(int argc, char *argv[])
{
    int server_fd = -1, new_socket = -1;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char *buffer = malloc(20);

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[2]));

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("getServer bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept incoming connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    int bytes = recv(new_socket, buffer, sizeof(buffer), 0);
    if (bytes < 0)
    {
        perror("recv");
        close(server_fd);
        close(new_socket);
        exit(EXIT_FAILURE);
    }

    close(server_fd);
    close(new_socket);
    return buffer;
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 7 || (argc == 5 && !strcmp(argv[1], "-c")) || (argc == 6 && !strcmp(argv[1], "-c")))
    {
        puts("Incorrect number of arguments\n");
        printf("Server Usage: ./stnc -s <Port> <Check Flag> <Quiet Flag>\nCheck Flag (not obligatory): -p (test communication)\nQuiet Flag (with check flag): -q (only testing results will be printed)\n\n");
        printf("Client Usage: ./stnc -c <IP> <Port> <Check Flag> <Type> <Param>\nCheck Flag (not obligatory): -p\nType (with check flag): ipv4/ipv6 | uds | mmap/pipe\nParam (with type): udp/tcp | dgram/stream | filename\n");
        return -1;
    }

    if (!strcmp(argv[1], "-c"))
    {
        if (argv[4] == NULL)
        {
            client(argc, argv);
        }
        else if (!strcmp(argv[4], "-p"))
        {
            if (!strcmp(argv[5], "ipv4"))
            {
                if (!strcmp(argv[6], "tcp"))
                {
                    tcp_client(argc, argv, IPV4);
                }
                else if (!strcmp(argv[6], "udp"))
                {
                    udp_client(argc, argv, IPV4);
                }
            }
            else if (!strcmp(argv[5], "ipv6"))
            {
                if (!strcmp(argv[6], "tcp"))
                {
                    tcp_client(argc, argv, IPV6);
                }
                else if (!strcmp(argv[6], "udp"))
                {
                    udp_client(argc, argv, IPV6);
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
                mmap_client(argc, argv);
            }
            else if (!strcmp(argv[5], "pipe"))
            {
                pipe_client(argc, argv);
            }
        }
    }
    else if (!strcmp(argv[1], "-s"))
    {
        if (argv[3] != NULL)
        {
            char *serverType = getServerType(argc, argv);
            // printf("Server type: %s\n", serverType);
            if (strcmp(serverType, "tcp4") == 0)
            {
                tcp_server(argc, argv, IPV4);
            }
            else if (strcmp(serverType, "tcp6") == 0)
            {
                tcp_server(argc, argv, IPV6);
            }
            else if (strcmp(serverType, "udp4") == 0)
            {
                udp_server(argc, argv, IPV4);
            }
            else if (strcmp(serverType, "udp6") == 0)
            {
                udp_server(argc, argv, IPV6);
            }
            else if (strcmp(serverType, "udss") == 0)
            {
                uds_stream_server(argc, argv);
            }
            else if (strcmp(serverType, "udsd") == 0)
            {
                uds_dgram_server(argc, argv);
            }
            else if (strcmp(serverType, "mmap") == 0)
            {
                mmap_server(argc, argv);
            }
            else if (strcmp(serverType, "pipe") == 0)
            {
                pipe_server(argc, argv);
            }
            free(serverType);
        }
        else
        {
            server(argc, argv);
        }
    }
    else
    {
        puts("Incorrect arguments\n");
        printf("Server Usage: ./stnc -s <Port> <Check Flag> <Quiet Flag>\nCheck Flag (not obligatory): -p (test communication)\nQuiet Flag (with check flag): -q (only testing results will be printed)\n\n");
        printf("Client Usage: ./stnc -c <IP> <Port> <Check Flag> <Type> <Param>\nCheck Flag (not obligatory): -p\nType (with check flag): ipv4/ipv6 | uds | mmap/pipe\nParam (with type): udp/tcp | dgram/stream | filename\n");
    }
    return 0;
}

char *generate_rand_str(int length)
{
    char *string = malloc(length + 1);
    if (!string)
    {
        return NULL;
    }

    for (int i = 0; i < length; i++)
    {
        int num = rand() % 26;
        string[i] = 'a' + num;
    }
    string[length] = '\0';
    return string;
}
