#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h> // 멀티 스레드용 헤더

#include "server.h"
#include "common_protocol.h" // 위에서 정의한 프로토콜
#include "managing_documents.h" // 기존 문서 관리 로직 재사용

#define MAX_CLIENTS 10

// 전역 변수: 클라이언트 소켓 관리 및 동기화
int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t mutx; // 중요: 공유 자원 보호용 뮤텍스

int current_writer_sock = -1; // 현재 작성 중인 사람의 소켓 (-1이면 아무도 안 씀)
char current_writer_name[20] = "";

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// 모든 클라이언트에게 패킷 전송 (Broadcasting)
void send_to_all(Packet *pkt, int sender_sock) {
    pthread_mutex_lock(&mutx);
    for (int i = 0; i < client_count; i++) {
        // sender_sock가 -1이면 조건 없이 모두에게 보냄
        // sender_sock가 특정 소켓이면 그 사람 빼고 보냄
        if (sender_sock == -1 || client_sockets[i] != sender_sock) {
            write(client_sockets[i], (void*)pkt, sizeof(Packet));
        }
    }
    pthread_mutex_unlock(&mutx);
}

// 클라이언트 핸들러 (스레드 함수)
void *handle_client_thread(void *arg) {
    int client_sock = *((int *)arg);
    ssize_t bytes_read;
    Packet pkt;

    pthread_mutex_lock(&mutx);
    if (doc_length > 0) {
        Packet sync_pkt;
        memset(&sync_pkt, 0, sizeof(Packet));
        sync_pkt.command = CMD_SYNC_ALL;
        
        // 서버의 전역 변수 doc_buffer 내용을 패킷에 복사
        // (Cell 구조체 -> char 배열로 변환 필요)
        for(int i=0; i<doc_length; i++) {
            sync_pkt.text_content[i] = doc_buffer[i].ch;
        }
        sync_pkt.text_len = doc_length;
        
        // 전송
        write(client_sock, &sync_pkt, sizeof(Packet));
        printf("Sent initial document (len: %d) to new client.\n", doc_length);
    }
    pthread_mutex_unlock(&mutx);

    // 2. 패킷 수신 루프
    while ((bytes_read = read(client_sock, (void*)&pkt, sizeof(Packet))) > 0) {

        if (pkt.command == CMD_SYNC_ALL) {
            // ========================================================
            // ★ 2. [방장 업로드 처리] 방장이 보낸 문서 서버에 저장
            // ========================================================
            
            // 기존 문서 초기화
            doc_length = 0; 
            memset(doc_buffer, 0, sizeof(doc_buffer));

            // 패킷 내용을 서버 메모리(doc_buffer)에 저장
            int len = pkt.text_len;
            if (len > MAX_BUFFER) len = MAX_BUFFER;
            
            for(int i=0; i<len; i++) {
                doc_buffer[i].ch = pkt.text_content[i];
                strcpy(doc_buffer[i].author, pkt.username); // 작성자는 방장 이름으로 통일
            }
            doc_length = len;

            printf("[SYNC] Host uploaded document. Length: %d\n", doc_length);
            
            // (선택 사항) 만약 방장 말고 이미 들어와 있던 다른 사람들이 있다면
            // 그 사람들에게도 바뀐 문서를 뿌려줘야 할 수 있음.
            // send_to_all(&pkt, client_sock); 

        }else if (pkt.command == CMD_REQUEST_LOCK) {
            // 작성 권한 요청 처리
            if (current_writer_sock == -1) {
                // 자리가 비었음 -> 너 써라!
                current_writer_sock = client_sock;
                strcpy(current_writer_name, pkt.username);
                
                // 1. 요청한 사람에게 승인 패킷 전송
                Packet response;
                response.command = CMD_LOCK_GRANTED;
                write(client_sock, &response, sizeof(Packet));

                // 2. 다른 사람들에게 "A가 쓰는 중이다"라고 방송
                Packet noti;
                strcpy(noti.username, pkt.username);
                noti.command = CMD_LOCK_UPDATE;

                sprintf(noti.message, "[잠금] %s님이 작성 중...", pkt.username);
                // send_to_all은 나(client_sock)를 빼고 보냄
                send_to_all(&noti, -1); 

            } else {
                // 이미 누가 쓰고 있음 -> 거절
                Packet response;
                response.command = CMD_LOCK_DENIED;
                strcpy(response.message, "다른 사용자가 작성 중입니다.");
                write(client_sock, &response, sizeof(Packet));
            }

        } else if (pkt.command == CMD_RELEASE_LOCK) {
            // 로그 추가
            FILE* fd = fopen("hello.txt","w");
            fprintf(fd,"[DEBUG] Release Lock requested by sock %d\n", client_sock);

            if (current_writer_sock == client_sock) {
                current_writer_sock = -1; // 잠금 해제
                strcpy(current_writer_name, "");

                Packet noti;
                memset(&noti, 0, sizeof(Packet)); // 초기화
                noti.command = CMD_LOCK_UPDATE;
                strcpy(noti.message, "[해제] 누구나 작성 가능");
                send_to_all(&noti, -1);
            }
        } else if (pkt.command == CMD_INSERT || pkt.command == CMD_DELETE) {
            // 실제 작성 요청: 권한 있는 사람인지 한 번 더 체크 (보안)
            if (current_writer_sock == client_sock) {
                if (pkt.command == CMD_INSERT) server_insert_char(pkt.cursor_index, pkt.ch, pkt.username);
                else server_delete_char(pkt.cursor_index);
                
                // 변경 사항 방송
                send_to_all(&pkt, -1);
            }
        }
        
        pthread_mutex_unlock(&mutx);
    }

    int was_writer = 0; // 나간 사람이 작성자였는지 체크하는 플래그

    pthread_mutex_lock(&mutx);

    // (1) 만약 나간 사람이 '작성 권한'을 쥐고 있었다면? -> 강제 반납 처리
    if (current_writer_sock == client_sock) {
        current_writer_sock = -1; // 잠금 해제
        strcpy(current_writer_name, ""); // 이름 초기화
        was_writer = 1; // "이 사람이 범인입니다" 표시 (Lock 풀고 방송하기 위해)
    }

    // (2) 클라이언트 명부(배열)에서 삭제
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == client_sock) {
            // 삭제된 자리(i)를 메우기 위해 뒤의 요소들을 한 칸씩 앞으로 당김
            while (i < client_count - 1) {
                client_sockets[i] = client_sockets[i + 1];
                i++;
            }
            break;
        }
    }
    client_count--; // 접속 인원 감소
    printf("Client disconnected. Current users: %d\n", client_count);

    pthread_mutex_unlock(&mutx);


    // (3) 만약 작성자가 나간 거라면, 남은 사람들에게 "잠금 해제" 방송
    // (주의: send_to_all 함수 내부에서 또 Lock을 걸기 때문에, 위에서 Lock을 푼 뒤 호출해야 데드락 안 걸림)
    if (was_writer) {
        Packet noti;
        noti.command = CMD_LOCK_UPDATE;
        strcpy(noti.message, "[해제] 작성자가 퇴장하여 잠금이 해제됨");
        
        // 이미 client_sockets 배열에서 나간 사람은 빠졌으므로, 
        // -1을 넣어 모두(남은 사람들)에게 전송
        send_to_all(&noti, -1);
    }

    // (4) 자원 정리
    close(client_sock); // 소켓 닫기
    free(arg);          // 스레드 인자 메모리 해제 (malloc 했던 것)
    
    return NULL;
}

int make_listening_socket(int port, int user_count) {
    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) error_exit("Socket creation failed");

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) error_exit("Bind failed");
    if (listen(sockfd, user_count) < 0) error_exit("Listen failed");

    return sockfd;
}

void run_server(int port, int user_count) {
    int server_sock, client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t t_id;

    // Mutex 초기화
    pthread_mutex_init(&mutx, NULL);

    server_sock = make_listening_socket(port, user_count);
    printf("Multi-threaded Server listening on port %d\n", port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&mutx);
        if (client_count >= MAX_CLIENTS) {
            close(client_sock);
            pthread_mutex_unlock(&mutx);
            continue;
        }
        client_sockets[client_count++] = client_sock;
        pthread_mutex_unlock(&mutx);

        printf("New Client Connected: %s\n", inet_ntoa(client_addr.sin_addr));

        int *new_sock = (int *)malloc(sizeof(int));
        *new_sock = client_sock;

        // 스레드 생성: handle_client_thread 실행
        if (pthread_create(&t_id, NULL, handle_client_thread, (void*)new_sock) != 0) {
            perror("Thread creation failed");
        } else {
            pthread_detach(t_id); // 스레드가 종료되면 자원을 스스로 반납하도록 설정
        }
    }

    close(server_sock);
    pthread_mutex_destroy(&mutx);
}