// [common_protocol.h]

#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#define MAX_BUFFER 4096 // managing_documents.h와 크기 맞춤

typedef enum {
    CMD_JOIN,
    CMD_INSERT,
    CMD_DELETE,
    CMD_REQUEST_LOCK,
    CMD_RELEASE_LOCK,
    CMD_LOCK_GRANTED,
    CMD_LOCK_DENIED,
    CMD_LOCK_UPDATE,
    CMD_SYNC_ALL
} CommandType;

typedef struct {
    CommandType command;
    char username[20];
    int cursor_index;
    char ch;
    char message[100];
    
    char text_content[MAX_BUFFER]; 
    char author_contents[MAX_BUFFER][20];
    int text_len; // 문서 길이
} Packet;

#endif