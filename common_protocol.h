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
    CMD_SYNC_ALL,
    CMD_LOAD_USERS,        // 방장 -> 서버 (DB 내용 전송)
    CMD_AUTH_LOGIN,        // 게스트 -> 서버 (로그인 시도)
    CMD_AUTH_REGISTER,     // 게스트 -> 서버 (회원가입 시도)
    CMD_AUTH_RESULT,       // 서버 -> 게스트 (성공/실패 결과)
    CMD_SAVE_USER,
    CMD_UPDATE_COLOR
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