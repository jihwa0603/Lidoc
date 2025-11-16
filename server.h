#ifndef SERVER_H
#define SERVER_H

#define PORT 8080
#define BUFFER_SIZE 1024

int setup_server_socket(int port, int user_count);
void handle_client(int client_sock);
void error_exit(const char *msg);
void run_server(int port, int user_count);


#endif // SERVER_H