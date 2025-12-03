#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curses.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <locale.h>

#include "common_protocol.h"
#include "managing_documents.h"

int my_socket = -1;             // 서버와 연결된 소켓
volatile int can_i_write = 0;            // 0: 읽기 전용, 1: 쓰기 가능
char current_writer[MAX_NAME] = ""; // 지금 누가 쓰고 있는지
pthread_mutex_t win_mutex = PTHREAD_MUTEX_INITIALIZER; // 화면 충돌 방지

Cell doc_buffer[MAX_BUFFER];
int doc_length = 0;

int cursor_idx = 0;     
Person *users = NULL;   
int user_count = 0;

// [클라이언트 전역 변수 혹은 구조체 상태]
char status_msg[100] = "Ready";

char current_working_doc_name[MAX_PATH] = "untitled";

int check_and_create_dir(const char *dirname) {
    // mkdir은 이미 시스템 콜입니다.
    if (mkdir(dirname, 0755) == 0) {
        printf("폴더 생성 성공: '%s'\n", dirname);
        return 0;
    } else {
        // 이미 있을 때
        if (errno == EEXIST) {
            printf("폴더가 이미 존재함: '%s'\n", dirname);
            return 0;
        } else {
            perror("폴더 생성 실패");
            return -1;
        }
    }
}

int manage_folder() {
    const char *dir1 = "documents";
    const char *dir2 = "documents_with_user";
    const char *dir3 = "user_data";
    
    printf("폴더 확인\n");
    
    // documents 폴더 처리
    if (check_and_create_dir(dir1) != 0) {
        fprintf(stderr, "documents 폴더 처리 중 오류 발생\n");
        return 1;
    }
    
    // documents_with_user 폴더 처리
    if (check_and_create_dir(dir2) != 0) {
        fprintf(stderr, "documents_with_user 폴더 처리 중 오류 발생\n");
        return 1;
    }

    // user_data 폴더 처리
    if (check_and_create_dir(dir3) != 0) {
        fprintf(stderr, "user_data 폴더 처리 중 오류 발생\n");
        return 1;
    }

    printf("모든 폴더 처리 완료\n");
    return 0;
}

void make_document(char *username, char *document_name) {
    int fd;
    char path1[MAX_PATH + 50];
    char path2[MAX_PATH + 50];
    char path3[MAX_PATH + 50];
    char filename[MAX_PATH];

    // filename은 documents/filename 이런식으로 들어오는 게 아니라 document_name을 이용해야 할 듯 하나
    // 기존 로직을 유지하기 위해 비워둡니다 (기존 코드에서 filename 초기화가 안되어 있었음)
    // 여기서는 document_name을 filename으로 가정하고 작성합니다.
    strcpy(filename, document_name);

    snprintf(path1, sizeof(path1), "documents/%s.txt", filename);
    
    printf("\n문서 생성(파일명: %s.txt)\n", filename);

    // fopen -> open
    fd = open(path1, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[documents] 폴더에 파일 생성 실패");
    } else {
        close(fd);
        printf("[documents] 폴더에 문서 생성 성공: '%s'\n", path1);
    }

    snprintf(path2, sizeof(path2), "documents_with_user/%s.txt", filename);

    fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[documents_with_user] 폴더에 파일 생성 실패");
    } else {
        close(fd);
        printf("[documents_with_user] 폴더에 문서 생성 성공: '%s'\n", path2);
    }

    snprintf(path3, sizeof(path3), "user_data/%s_users.txt", document_name);
    fd = open(path3, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[user_data] 폴더에 파일 생성 실패");
    } else {
        close(fd);
        printf("[user_data] 폴더에 문서 생성 성공: '%s'\n", path3);
    }
}

void register_person(const char *filename, const char *username, const char *color) {
    int fd;
    char path[MAX_PATH];
    char buf[256];
    
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    // fopen(..., "a") -> open(..., O_APPEND)
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        // 실패하면 새로 생성 시도 (기존 로직 유지)
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (fd != -1) {
        // fprintf -> snprintf + write
        snprintf(buf, sizeof(buf), "%s %s 0\n", username, color);
        write(fd, buf, strlen(buf));
        close(fd);
        printf("사용자 정보 등록 성공: '%s'\n", path);
    } else {
        perror("사용자 정보 파일 열기 실패");
    }
}

Person* read_persons(const char *filename, int *count) {
    int fd;
    char path[MAX_PATH];
    
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("사용자 정보 파일 열기 실패");
        return 0;
    } else {
        // 메모리 할당
        int max_size = 10;
        Person *people = (Person*)malloc(sizeof(Person) * max_size);
        
        int i = 0;
        char line_buf[256];
        int pos = 0;
        char ch;

        // read로 한 글자씩 읽어서 라인 파싱 (fscanf 대체)
        while (read(fd, &ch, 1) == 1) {
            if (ch == '\n') {
                line_buf[pos] = '\0'; // 문자열 종료
                
                // sscanf로 파싱
                if (sscanf(line_buf, "%s %s %ld", people[i].username, people[i].color, &people[i].context) == 3) {
                    i++;
                    if (i >= max_size) break;
                }
                pos = 0; // 버퍼 초기화
            } else {
                if (pos < sizeof(line_buf) - 1) {
                    line_buf[pos++] = ch;
                }
            }
        }
        
        // 마지막 줄에 개행문자가 없는 경우 처리
        if (pos > 0 && i < max_size) {
            line_buf[pos] = '\0';
            if (sscanf(line_buf, "%s %s %ld", people[i].username, people[i].color, &people[i].context) == 3) {
                i++;
            }
        }

        close(fd);

        *count = i;
        return people;
    }
}

void save_user_data(const char *filename, Person *people, int count) {
    int fd;
    char path[MAX_PATH];
    char buf[256];

    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("사용자 정보 파일 열기 실패");
    } else {
        for (int i = 0; i < count; i++) {
            // fprintf -> snprintf + write
            snprintf(buf, sizeof(buf), "%s %s %ld\n", people[i].username, people[i].color, people[i].context);
            write(fd, buf, strlen(buf));
        }
        close(fd);
        printf("사용자 데이터 저장 성공: '%s'\n", path);
    }
}

void change_color(const char* filename, const char* user_name, const char *new_color) {
    Person *people = NULL;
    int count = 0;
    people = read_persons(filename, &count);

    if (people == NULL) {
        fprintf(stderr, "사용자 정보를 읽어올 수 없습니다.\n");
        return;
    }
    // 사용자 이름을 통해 색상 변경
    for (int i = 0; i < count; i++) {
        if (strcmp(people[i].username, user_name) == 0) {
            strcpy(people[i].color, new_color);
            break;
        }
    }
    // 변경된 내용을 파일에 다시 저장 (save_user_data 함수는 이미 open으로 변경됨)
    save_user_data(filename, people, count);
    
    free(people);
}

int get_ncurses_color_code(const char *color_str) {
    if (strcasecmp(color_str, "red") == 0) return COLOR_RED;
    if (strcasecmp(color_str, "green") == 0) return COLOR_GREEN;
    if (strcasecmp(color_str, "yellow") == 0) return COLOR_YELLOW;
    if (strcasecmp(color_str, "blue") == 0) return COLOR_BLUE;
    if (strcasecmp(color_str, "magenta") == 0) return COLOR_MAGENTA;
    if (strcasecmp(color_str, "cyan") == 0) return COLOR_CYAN;
    if (strcasecmp(color_str, "white") == 0) return COLOR_WHITE;
    
    return COLOR_WHITE; // 기본값
}

int get_user_color_pair(const char *target_name, Person *people, int count) {
    if (people != NULL && count > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(people[i].username, target_name) == 0) {
                int color_code = get_ncurses_color_code(people[i].color);
                
                if (color_code == COLOR_RED) return 1;
                if (color_code == COLOR_GREEN) return 2;
                if (color_code == COLOR_YELLOW) return 3;
                if (color_code == COLOR_BLUE) return 4;
                if (color_code == COLOR_MAGENTA) return 5;
                if (color_code == COLOR_CYAN) return 6;
                if (color_code == COLOR_WHITE) return 7;
            }
        }
    }
    return 7;
}

void load_document(const char *filename) {
    char path[256];
    int fd;
    
    snprintf(path, sizeof(path), "documents_with_user/%s", filename);

    fd = open(path, O_RDONLY);
    doc_length = 0;
    
    if (fd == -1) return;

    char current_author[MAX_NAME] = "Unknown";
    char ch; // int ch -> char ch (read용)
    int state = 0;
    int escape = 0;

    char name_buf[MAX_NAME];
    int name_idx = 0;

    // fgetc -> read
    while (read(fd, &ch, 1) == 1 && doc_length < MAX_BUFFER) {
        if (state == 0) {
            if (escape) {
                doc_buffer[doc_length].ch = ch;
                strcpy(doc_buffer[doc_length].author, current_author);
                doc_length++;
                escape = 0; 
            }
            else if (ch == '\\') {
                escape = 1; 
            }
            else if (ch == '[') {
                state = 1; 
                name_idx = 0;
            } 
            else {
                doc_buffer[doc_length].ch = ch;
                strcpy(doc_buffer[doc_length].author, current_author);
                doc_length++;
            }
        } else if (state == 1) {
            if (ch == ']') {
                state = 0; 
                name_buf[name_idx] = '\0';
                strcpy(current_author, name_buf);
            } else {
                if (name_idx < MAX_NAME - 1) name_buf[name_idx++] = ch;
            }
        }
    }
    close(fd);
}

void save_document(const char *doc_name, Person *people, int user_count) {
    char path_plain[256];
    char path_tagged[256];
    int fd_plain, fd_tagged;
    
    // 순수 문서만
    snprintf(path_plain, sizeof(path_plain), "documents/%s.txt", doc_name);
    fd_plain = open(path_plain, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd_plain != -1) {
        for (int i = 0; i < doc_length; i++) {
            // fputc -> write
            write(fd_plain, &doc_buffer[i].ch, 1);
        }
        close(fd_plain);
    }

    // 누가 썼는지 포함
    snprintf(path_tagged, sizeof(path_tagged), "documents_with_user/%s.txt", doc_name);
    fd_tagged = open(path_tagged, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd_tagged != -1) {
        char last_author[MAX_NAME] = "";
        int person_index = -1;

        for(int i = 0; i < user_count; i++) {
            people[i].context = 0;
        }

        for (int i = 0; i < doc_length; i++) {
            if (strcmp(last_author, doc_buffer[i].author) != 0) {
                // fprintf -> snprintf + write
                char tag_buf[100];
                snprintf(tag_buf, sizeof(tag_buf), "[%s]", doc_buffer[i].author);
                write(fd_tagged, tag_buf, strlen(tag_buf));
                
                strcpy(last_author, doc_buffer[i].author);
                for(int j = 0; j < user_count; j++) {
                    if (strcmp(people[j].username, doc_buffer[i].author) == 0) {
                        person_index = j;
                        break;
                    }
                }
                if (person_index != -1) {
                    people[person_index].context++;
                }
            }
            char ch = doc_buffer[i].ch;
            char escape_bs = '\\';
            char escape_ob = '[';
            char escape_cb = ']';
            
            if (ch == '\\') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_bs, 1); 
            }
            else if (ch == '[') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_ob, 1);
            }
            else if (ch == ']') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_cb, 1);
            }
            else {
                write(fd_tagged, &ch, 1);
            }
        }
        close(fd_tagged);
    }
    save_user_data(doc_name, people, user_count);
}

// 중간에 문서 작성
void insert_char(int *cursor, char ch, const char *username) {
    if (doc_length >= MAX_BUFFER - 1) return;

    for (int i = doc_length; i > *cursor; i--) {
        doc_buffer[i] = doc_buffer[i - 1];
    }

    doc_buffer[*cursor].ch = ch;
    strcpy(doc_buffer[*cursor].author, username);

    doc_length++;
    (*cursor)++;
}

// 중간에 문자 삭제
void delete_char(int *cursor) {
    if (*cursor == 0 || doc_length == 0) return;

    int delete_idx = *cursor - 1;
    for (int i = delete_idx; i < doc_length - 1; i++) {
        doc_buffer[i] = doc_buffer[i + 1];
    }

    doc_length--;
    (*cursor)--;
}

// 커서 위치를 화면 좌표로 변환
void get_screen_pos(int target_idx, int *y, int *x) {
    int cur_x = 0;
    int cur_y = 0;
    int width = COLS;

    for (int i = 0; i < target_idx; i++) {
        if (doc_buffer[i].ch == '\n') {
            cur_y++;
            cur_x = 0;
        } else {
            cur_x++;
            if (cur_x >= width) {
                cur_y++;
                cur_x = 0;
            }
        }
    }
    *y = cur_y;
    *x = cur_x;
}

void server_insert_char(int index, char ch, const char *username) {
    if (index < 0 || index > doc_length || doc_length >= MAX_BUFFER - 1) return;

    for (int i = doc_length; i > index; i--) {
        doc_buffer[i] = doc_buffer[i - 1];
    }

    doc_buffer[index].ch = ch;
    strcpy(doc_buffer[index].author, username);
    
    doc_length++;
}

void server_delete_char(int index) {
    if (index < 0 || index >= doc_length) return;

    for (int i = index; i < doc_length - 1; i++) {
        doc_buffer[i] = doc_buffer[i + 1];
    }

    doc_length--;
}

void draw_document(const char *my_username) {
    clear();
    noecho();

    // 1. 상단바 그리기
    if (can_i_write) {
        attron(COLOR_PAIR(1)); // 빨간색 (작성 모드)
        mvprintw(0, 0, "[작성 모드] 작성 중... (ESC: 저장/반납)  현재 작성자 %s", current_writer);
        attroff(COLOR_PAIR(1));
    } else {
        attron(COLOR_PAIR(8)); // 흰배경 검은글씨 (읽기 모드)
        mvprintw(0, 0, "[읽기 모드] 엔터(Enter)를 누르면 작성 권한 요청");
        attroff(COLOR_PAIR(8));
        
        if (strlen(current_writer) > 0) {
            mvprintw(0, 50, "현재 작성자: %s", current_writer);
        }
    }

    // 2. 본문 내용 그리기
    int screen_y = 2; 
    move(screen_y, 0); 
    
    for (int i = 0; i < doc_length; i++) {
        int color_id = get_user_color_pair(doc_buffer[i].author, users, user_count);
        
        attron(COLOR_PAIR(color_id));
        printw("%c", doc_buffer[i].ch);
        attroff(COLOR_PAIR(color_id));
    }

    // 3. 커서 위치 잡기
    int cur_y, cur_x;
    get_screen_pos(cursor_idx, &cur_y, &cur_x);
    move(screen_y + cur_y, cur_x);

    refresh();
}

void *recv_thread_func(void *arg) {
    Packet pkt;
    
    while (read(my_socket, &pkt, sizeof(Packet)) > 0) {
        pthread_mutex_lock(&win_mutex); 

        if (pkt.command == CMD_SYNC_ALL) {
            doc_length = 0;
            int len = pkt.text_len;
            if (len > MAX_BUFFER) len = MAX_BUFFER;

            for (int i = 0; i < len; i++) {
                doc_buffer[i].ch = pkt.text_content[i];
                strcpy(doc_buffer[i].author, pkt.username); 
            }
            doc_length = len;
            
        } else if (pkt.command == CMD_LOCK_GRANTED) {
            can_i_write = 1;
            strcpy(current_writer, current_writer); 
            
        } else if (pkt.command == CMD_LOCK_DENIED) {
            mvprintw(LINES-1, 0, "거절됨: %s", pkt.message);

        } else if (pkt.command == CMD_RELEASE_LOCK) {
            // ★ [추가] 누군가가 작성을 마치고 저장을 요청함
            
            // 1. 내 컴퓨터(로컬)에 현재 내용을 저장
            save_document(current_working_doc_name, users, user_count);
            
            // 2. 잠금 상태 해제 (UI 업데이트)
            strcpy(current_writer, "");
            can_i_write = 0;
            
            // 3. 알림 표시
            mvprintw(0, 40, "[알림] %s님이 저장함 (내 파일도 업데이트됨)   ", pkt.username);

        } else if (pkt.command == CMD_INSERT) {
            server_insert_char(pkt.cursor_index, pkt.ch, pkt.username);
            if (pkt.cursor_index <= cursor_idx) cursor_idx++;
            
        } else if (pkt.command == CMD_DELETE) {
            server_delete_char(pkt.cursor_index);
            if (pkt.cursor_index < cursor_idx) cursor_idx--;
        }

        draw_document(pkt.username);
        
        pthread_mutex_unlock(&win_mutex);
    }
    return NULL;
}

void run_network_text_editor(int socket_fd, char *username, int is_host, char *doc_name) {
    my_socket = socket_fd;

    strcpy(current_working_doc_name, doc_name);

    pthread_t r_thread;

    if (users != NULL) free(users); 
    users = read_persons(doc_name, &user_count);

    setlocale(LC_ALL, "");

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    init_pair(8, COLOR_BLACK, COLOR_WHITE);

    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    timeout(100); 
    curs_set(1);

    pthread_create(&r_thread, NULL, recv_thread_func, NULL);
    pthread_detach(r_thread);

    if (is_host) {
        char actual_filename[256];
        snprintf(actual_filename, 256, "%s_by_users.txt", doc_name);
        load_document(actual_filename); 
        cursor_idx = doc_length; 

        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));
        pkt.command = CMD_SYNC_ALL;
        strcpy(pkt.username, username);
        for (int i = 0; i < doc_length; i++) {
            pkt.text_content[i] = doc_buffer[i].ch;
        }
        pkt.text_len = doc_length;
        write(my_socket, &pkt, sizeof(Packet));
    }
    else {
        doc_length = 0; 
        cursor_idx = 0;
    }

    int ch;

    timeout(100);

    while (1) {
        // Optimistic UI 반영 전 기존 코드 구조 유지 (사용자 요청 반영)
        // 화면 그리기 로직은 recv_thread에서 draw_document 호출 및
        // 아래 refresh()로 처리됨.
        // 깜빡임 방지를 위해 메인 루프에서 draw_document를 지속 호출하는 것은 
        // recv_thread와 mutex 경쟁을 할 수 있으므로 draw_document 호출은 
        // 이벤트 발생 시(키 입력, 수신 등)에만 하는 것이 좋음.
        // 여기서는 기존 구조대로 유지.
        
        // 주의: 이전에 draw_document 호출을 여기서 하도록 수정했었으나,
        // 현재 제공해주신 코드에는 빠져있어 그대로 둡니다.
        // 필요 시 pthread_mutex_lock(&win_mutex); draw_document(username); ... 추가 필요

        refresh();
        pthread_mutex_unlock(&win_mutex); // 여기서 unlock만 하는게 맞는지 확인 필요 (Lock 위치가 애매함)
        // 기존 코드에 위쪽에 lock이 없는데 unlock이 있어서, 
        // 아마 while문 위쪽 어딘가나 draw_document 호출 전 lock이 있어야 하는데
        // 제공된 코드상으로는 unlock만 덩그러니 있습니다. 
        // 에러 방지를 위해 lock/unlock 쌍을 맞추거나 제거하는 것이 좋으나,
        // 원본 유지를 위해 그대로 둡니다.

        // === 키 입력 처리 ===
        ch = getch(); 
        if(ch == ERR){
                continue;
        }
        if (can_i_write) {
            Packet pkt;
            memset(&pkt, 0, sizeof(Packet));
            strcpy(pkt.username, username);

            if (ch == 27) { // [ESC 키 입력]
                
                // ★ [추가] 서버로 반납하기 전에 내 컴퓨터(로컬)에 먼저 저장합니다.
                // doc_name: 현재 열려있는 문서 이름
                // users: 현재 등록된 사용자 정보 (기여도 포함)
                // user_count: 사용자 수
                save_document(doc_name, users, user_count);
                
                // 저장 완료 메시지 (잠깐 보여줌)
                attron(COLOR_PAIR(2)); // 초록색
                mvprintw(LINES-1, 0, " [내 컴퓨터에 저장 완료!] ");
                attroff(COLOR_PAIR(2));
                refresh();
                usleep(500000); // 0.5초 대기

                // 2. 서버에 잠금 해제 요청 전송
                memset(&pkt, 0, sizeof(Packet)); 
                pkt.command = CMD_RELEASE_LOCK;
                strcpy(pkt.username, username); 
                
                write(my_socket, &pkt, sizeof(Packet));
                
                can_i_write = 0; 
                
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                pkt.command = CMD_DELETE;
                pkt.cursor_index = cursor_idx; 
                write(my_socket, &pkt, sizeof(Packet));
                
                // Optimistic UI (내 화면 즉시 반영) 코드가 필요하다면 여기에 추가
                if (cursor_idx > 0) {
                    for (int i = cursor_idx - 1; i < doc_length - 1; i++) {
                        doc_buffer[i] = doc_buffer[i + 1]; 
                    }
                    doc_buffer[doc_length - 1].ch = '\0'; 
                    doc_length--; 
                    cursor_idx--; 
                }

            } else {
                pkt.command = CMD_INSERT;
                pkt.ch = ch;
                pkt.cursor_index = cursor_idx;
                write(my_socket, &pkt, sizeof(Packet));
                
                // Optimistic UI (내 화면 즉시 반영)
                /* 이전에 이중 입력 문제를 해결하기 위해 서버 코드를 수정했으므로,
                   여기서 내 화면에 직접 그리는 코드가 있어야 '내가 쓴 글'이 보입니다.
                   제공해주신 코드에는 이 부분이 없어서 추가하지 않았으나,
                   글자가 안보인다면 아래 주석을 해제하여 추가해야 합니다.
                */
                /*
                if (doc_length < MAX_BUFFER) {
                    for (int i = doc_length; i > cursor_idx; i--) {
                        doc_buffer[i] = doc_buffer[i-1];
                    }
                    doc_buffer[cursor_idx].ch = ch;
                    strcpy(doc_buffer[cursor_idx].author, username);
                    doc_length++;
                    cursor_idx++;
                }
                */
            }
            
        } else {
            if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { 
                Packet pkt;
                memset(&pkt, 0, sizeof(Packet)); 
                pkt.command = CMD_REQUEST_LOCK;
                strcpy(pkt.username, username);
                write(my_socket, &pkt, sizeof(Packet));
                
            } else if (ch == 'q') { 
                timeout(-1);
                break; 
            }
        }
    }

    if(users != NULL) {
        free(users);
        users = NULL;
    }

    endwin();
}