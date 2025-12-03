#ifndef SERVER_H
#define SERVER_H

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_BUFFER 4096
#define MAX_NAME 30

#include "common_protocol.h"   // Packet 구조체를 알기 위해
#include "managing_documents.h" // Cell 구조체를 알기 위해

int setup_server_socket(int port, int user_count);
void handle_client(int client_sock);
void error_exit(const char *msg);
void run_server(int port, int user_count);

extern Cell doc_buffer[MAX_BUFFER];
extern int doc_length;

int setup_server_socket(int port, int user_count);
void handle_client(int client_sock);
void error_exit(const char *msg);
void run_server(int port, int user_count);
int make_listening_socket(int port, int user_count);
void *handle_client_thread(void *arg);
void send_to_all(Packet *pkt, int sender_sock);

#endif // SERVER_H