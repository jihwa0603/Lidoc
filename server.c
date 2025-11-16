#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

#include "server.h"

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int make_listening_socket(int port, int user_count) {
    int sockfd;
    struct sockaddr_in server_addr;

    if (port <= 0) {
        port = PORT;
    }

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error_exit("Socket creation failed");
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        error_exit("Set socket options failed");
    }

    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        error_exit("Bind failed");
    }

    // Listen on socket
    if (listen(sockfd, user_count) < 0) {
        close(sockfd);
        error_exit("Listen failed");
    }

    return sockfd;
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read data from client
    while ((bytes_read = read(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the string
        printf("Received: %s", buffer);
        
        // Echo back to client
        if (write(client_sock, buffer, bytes_read) < 0) {
            perror("Write to client failed");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Read from client failed");
    }

    close(client_sock);
}

void run_server(int port, int user_count) {
    int server_sock, client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_sock = make_listening_socket(port, user_count);
    printf("Server listening on port %d\n", port);

    while (1) {
        // Accept a new client connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_client(client_sock);
        printf("Client disconnected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }

    close(server_sock);
}