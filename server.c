#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#include "server.h"
#include "common_protocol.h"
#include "managing_documents.h"

#define MAX_CLIENTS 10

int client_sockets[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t mutx;

int current_writer_sock = -1; // 현재 작성 중인 사람의 소켓 (-1이면 아무도 안 쓰고 있음)
char current_writer_name[20] = "";

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

typedef struct {
    char id[20];
    char pw[20]; // 해시된 문자열
} UserInfo;

UserInfo user_list[20]; // 최대 20명 가정
int user_count_db = 0;
int host_sock_fd = -1;  // 방장 소켓 번호 저장 (저장 요청 보낼 때 필요)

// 모든 클라이언트에게 패킷 전송
void send_to_all(Packet *pkt, int sender_sock) {
    pthread_mutex_lock(&mutx);
    for (int i = 0; i < client_count; i++) {
        // sender_sock가 -1이면 조건 없이 모두에게 보냄, 아니면 그 사람을 제외하고 보냄 -> 이제는 필요없지만 혹시나 싶은 에러 방지용으로 일단 삭제는 안했음
        if (sender_sock == -1 || client_sockets[i] != sender_sock) {
            write(client_sockets[i], (void*)pkt, sizeof(Packet));
        }
    }
    pthread_mutex_unlock(&mutx);
}

// 클라이언트 핸들러 (+ 스레드 포함)
void *handle_client_thread(void *arg) {
    int client_sock = *((int *)arg);
    ssize_t bytes_read;
    Packet pkt;

    // 방장의 문서로 접속하는 인원에게 배포? 하여 그 문서를 다운받도록(개인 클라이언트 버퍼에)
    pthread_mutex_lock(&mutx);
    if (doc_length > 0) {
        Packet sync_pkt;
        memset(&sync_pkt, 0, sizeof(Packet));
        sync_pkt.command = CMD_SYNC_ALL;
        
        // 서버의 전역 변수 doc_buffer 내용을 패킷에 복사 (Cell 구조체 -> char 배열로 변환 필요)
        for(int i=0; i<doc_length; i++) {
            sync_pkt.text_content[i] = doc_buffer[i].ch;
        }
        sync_pkt.text_len = doc_length;
        
        // 전송
        write(client_sock, &sync_pkt, sizeof(Packet));
        printf("새로 들어온 클라이언트에게 초기 문서 제공 (문서 길이: %d) \n", doc_length);

    }

    if (current_writer_sock != -1) {
        Packet lock_info;
        memset(&lock_info, 0, sizeof(Packet));
        lock_info.command = CMD_LOCK_UPDATE;
        strcpy(lock_info.username, current_writer_name);
        sprintf(lock_info.message, "[상태동기화] 현재 %s님이 작성 중입니다.", current_writer_name);
        
        write(client_sock, &lock_info, sizeof(Packet));
    }
    pthread_mutex_unlock(&mutx);

    // 서버의 수신
    while ((bytes_read = read(client_sock, (void*)&pkt, sizeof(Packet))) > 0) {

        if (pkt.command == CMD_LOAD_USERS) {
            host_sock_fd = client_sock; // DB를 보낸 사람이 곧 방장
            user_count_db = 0;

            // 텍스트 통으로 된거 파싱 (예: "user1 123\nuser2 456\n")
            char *ptr = pkt.text_content;
            char *line = strtok(ptr, "\n");
            while (line != NULL && user_count_db < 20) {
                sscanf(line, "%s %s", user_list[user_count_db].id, user_list[user_count_db].pw);
                user_count_db++;
                line = strtok(NULL, "\n");
            }
            printf("[SERVER] 유저 DB 로드 완료 (%d명)\n", user_count_db);
        }

        // 2. 로그인 요청
        else if (pkt.command == CMD_AUTH_LOGIN) {
            int success = 0;
            for (int i = 0; i < user_count_db; i++) {
                if (strcmp(user_list[i].id, pkt.username) == 0 && 
                    strcmp(pkt.message, user_list[i].pw) == 0) { // msg에 비번 담겨옴
                    success = 1;
                    break;
                }
            }
            
            Packet res;
            memset(&res, 0, sizeof(Packet));
            res.command = CMD_AUTH_RESULT;
            sprintf(res.message, "%d", success);
            write(client_sock, &res, sizeof(Packet));

            if (success) {
                pthread_mutex_lock(&mutx);
                send_doc_to_client(client_sock);
                pthread_mutex_unlock(&mutx);
            }
        }

        // 3. 회원가입 요청
        else if (pkt.command == CMD_AUTH_REGISTER) {
            int exists = 0;
            for (int i = 0; i < user_count_db; i++) {
                if (strcmp(user_list[i].id, pkt.username) == 0) {
                    exists = 1;
                    break;
                }
            }

            Packet res;
            memset(&res, 0, sizeof(Packet));
            res.command = CMD_AUTH_RESULT;

            if (exists) {
                sprintf(res.message, "0"); // 실패
            } else {
                // 메모리에 추가
                strcpy(user_list[user_count_db].id, pkt.username);
                strcpy(user_list[user_count_db].pw, pkt.message);
                user_count_db++;
                sprintf(res.message, "1"); // 성공

                // [중요] 방장에게 파일 저장 요청
                if (host_sock_fd != -1) {
                    Packet save_pkt;
                    memset(&save_pkt, 0, sizeof(Packet));
                    save_pkt.command = CMD_SAVE_USER;
                    strcpy(save_pkt.username, pkt.username); // ID
                    strcpy(save_pkt.message, pkt.message);   // PW
                    write(host_sock_fd, &save_pkt, sizeof(Packet));
                }
            }
            write(client_sock, &res, sizeof(Packet));
        }

        else if (pkt.command == CMD_SYNC_ALL) {
            // 방장이 보낸 문서 서버에 저장 (처음 방장이 지금 자신이 가진 문서 내용을 업로드)
            
            // 기존 문서 초기화
            doc_length = 0; 
            memset(doc_buffer, 0, sizeof(doc_buffer));

            // 패킷 내용을 서버 메모리(doc_buffer)에 저장
            int len = pkt.text_len;
            if (len > MAX_BUFFER) len = MAX_BUFFER;
            
            for(int i=0; i<len; i++) {
                doc_buffer[i].ch = pkt.text_content[i];
                strcpy(doc_buffer[i].author, pkt.author_contents[i]); // 처음 불러올 때는 각 문자별 작성자를 저장
            }
            doc_length = len;

            printf("[SYNC] Host uploaded document. Length: %d\n", doc_length);

        } else if (pkt.command == CMD_REQUEST_LOCK) {
            // 작성 권한 요청 처리
            if (current_writer_sock == -1) {
                // 자리가 비었음 -> 너 권한 줄게 ㅇㅇ
                current_writer_sock = client_sock;
                strcpy(current_writer_name, pkt.username);
                
                // 요청한 사람에게 승인 패킷 전송
                Packet response;
                response.command = CMD_LOCK_GRANTED;
                strcpy(response.username,pkt.username);
                write(client_sock, &response, sizeof(Packet));

                // 다른 사람들에게 "A가 쓰는 중이다"라고 알림
                Packet noti;
                strcpy(noti.username, pkt.username);
                noti.command = CMD_LOCK_UPDATE;

                sprintf(noti.message, "[잠금] %s님이 작성 중...", pkt.username);

                send_to_all(&noti, client_sock);

            } else {
                // 이미 누가 쓰고 있음 -> 허나 거절한다!
                Packet response;
                response.command = CMD_LOCK_DENIED;
                strcpy(response.message, "다른 사용자가 작성 중입니다.");
                write(client_sock, &response, sizeof(Packet));
            }

        } else if (pkt.command == CMD_RELEASE_LOCK) {

            if (current_writer_sock == client_sock) {
                current_writer_sock = -1;
                strcpy(current_writer_name, "");

                // 작성 끝났다 -> 저장하자
                Packet release_pkt;
                memset(&release_pkt, 0, sizeof(Packet));
                
                release_pkt.command = CMD_RELEASE_LOCK; // 저장 및 해제 신호
                strcpy(release_pkt.username, pkt.username); // 누가 저장을 마쳤는지
                
                // client_sock(작성자)을 제외한 모든 사람에게 전송
                // (작성자는 이미 자신의 컴퓨터에 저장하도록 설정 해놨음)
                send_to_all(&release_pkt, client_sock);
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

    int was_writer = 0; // 나간 사람이 작성자였는지 체크

    pthread_mutex_lock(&mutx);

    // 만약 나간 사람이 작성자였다? -> 강제 반납 처리
    if (current_writer_sock == client_sock) {
        current_writer_sock = -1; // 잠금 해제
        strcpy(current_writer_name, ""); // 이름 초기화
        was_writer = 1; // 이전 사람이 작성자였음
    }

    // 클라이언트 명부(배열)에서 삭제
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

    // 만약 작성자가 나간 거라면, 남은 사람들에게 해제를 알림
    if (was_writer) {
        Packet noti;
        noti.command = CMD_LOCK_UPDATE;
        strcpy(noti.message, "[해제] 작성자가 퇴장하여 잠금이 해제됨");
        
        // 이미 client_sockets 배열에서 나간 사람은 빠졌으므로, 
        // -1을 넣어 모두(남은 사람들)에게 전송
        send_to_all(&noti, -1);
    }

    // 자원 정리
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

    // 서버 초기 설정
    server_sock = make_listening_socket(port, user_count);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        // 들어온 클라이언트 소켓 저장
        pthread_mutex_lock(&mutx);
        if (client_count >= MAX_CLIENTS) {
            close(client_sock);
            pthread_mutex_unlock(&mutx);
            continue;
        }
        client_sockets[client_count++] = client_sock;
        pthread_mutex_unlock(&mutx);

        printf("New Client Connected: %s\n", inet_ntoa(client_addr.sin_addr));

        // 쓰레드로 바꿀 소켓을 계속해서 새롭게 저장해서 값이 갑자기 바뀌는 거 방지
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