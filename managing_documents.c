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
#include "searchTextindocu.h"
#include "common_protocol.h"
#include "managing_documents.h"

volatile int is_searching = 0;
volatile int server_connected = 1; // 1: 연결됨, 0: 끊김
int my_socket = -1;             // 서버와 연결된 소켓
volatile int can_i_write = 0;            // 0: 읽기 전용, 1: 쓰기 가능
char current_writer[MAX_NAME] = ""; // 지금 누가 쓰고 있는지
pthread_mutex_t win_mutex = PTHREAD_MUTEX_INITIALIZER; // 화면 충돌 방지

Cell doc_buffer[MAX_BUFFER];
int doc_length = 0;

int cursor_idx = 0;     
Person *users = NULL;   
int user_count = 0;

// 클라이언트 전역 변수 혹은 구조체 상태
char status_msg[100] = "Ready";

char current_working_doc_name[MAX_PATH] = "untitled";

const char *ALL_COLORS[] = {"red", "green", "yellow", "blue", "magenta", "cyan", "white"};
const int NUM_COLORS = 7;

int check_and_create_dir(const char *dirname) {
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

    strcpy(filename, document_name);

    snprintf(path1, sizeof(path1), "documents/%s.txt", filename);
    
    printf("\n문서 생성(파일명: %s.txt)\n", filename);

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
    
    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        // 실패하면 새로 생성 시도
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (fd != -1) {
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

        // read로 한 글자씩 읽어서 라인 파싱
        while (read(fd, &ch, 1) == 1) {
            if (ch == '\n') {
                line_buf[pos] = '\0';
                
                if (pos > 0 && line_buf[pos-1] == '\r') {
                    line_buf[pos-1] = '\0';
                }

                // sscanf로 파싱
                if (sscanf(line_buf, "%s %s %ld", people[i].username, people[i].color, &people[i].context) == 3) {
                    i++;
                    if (i >= max_size) break;
                }
                pos = 0;
            } else {
                if (pos < sizeof(line_buf) - 1) {
                    line_buf[pos++] = ch;
                }
            }
        }
        
        // 마지막 줄에 개행문자가 없는 경우 처리
        if (pos > 0 && i < max_size) {
            line_buf[pos] = '\0';
            if (pos > 0 && line_buf[pos-1] == '\r') line_buf[pos-1] = '\0'; // 여기도 처리

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
            snprintf(buf, sizeof(buf), "%s %s %ld\n", people[i].username, people[i].color, people[i].context);
            write(fd, buf, strlen(buf));
        }
        close(fd);
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
    // 변경된 내용을 파일에 다시 저장
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
    char ch;
    int state = 0;
    int escape = 0;

    char name_buf[MAX_NAME];
    int name_idx = 0;

    while (read(fd, &ch, 1) == 1 && doc_length < MAX_BUFFER) {
        // 단순 작성일때
        if (state == 0) {
            if (escape) {
                doc_buffer[doc_length].ch = ch;
                strcpy(doc_buffer[doc_length].author, current_author);
                doc_length++;
                escape = 0; 
            }
            // \를 만나면 단순 작성모드
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
        // 작성자를 만났음
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
            write(fd_plain, &doc_buffer[i].ch, 1);
        }

        if (doc_length > 0 && doc_buffer[doc_length - 1].ch != '\n') {
            char newline = '\n';
            write(fd_plain, &newline, 1);
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

            if (person_index != -1) {
                people[person_index].context++;
            }

            char ch = doc_buffer[i].ch;
            char escape_bs = '\\';
            char escape_ob = '[';
            char escape_cb = ']';
            
            // \를 작성한 경우
            if (ch == '\\') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_bs, 1); 
            } // [를 작성했을 때
            else if (ch == '[') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_ob, 1);
            }// ]를 작성했을 때
            else if (ch == ']') {
                write(fd_tagged, &escape_bs, 1);
                write(fd_tagged, &escape_cb, 1);
            }
            else {
                write(fd_tagged, &ch, 1);
            }
        }

        if (doc_length > 0 && doc_buffer[doc_length - 1].ch != '\n') {
            char newline = '\n';
            write(fd_tagged, &newline, 1);
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

// 위/아래 방향키 처리를 위한 함수 (direction: -1은 위, 1은 아래)
void move_cursor_vertically(int direction) {
    int cur_y, cur_x;
    get_screen_pos(cursor_idx, &cur_y, &cur_x); // 현재 커서의 화면 좌표 구하기

    int target_y = cur_y + direction;
    if (target_y < 0) return; // 더 이상 위로 갈 수 없음

    // 목표하는 y좌표(target_y)에 있으면서, 현재 x좌표(cur_x)와 가장 가까운 인덱스를 찾음
    int best_idx = -1;
    int min_dist = 99999; 

    // 문서 전체를 순회하며 목표 위치 찾기 (문서가 크지 않으므로 가능)
    // 최적화를 원한다면 현재 인덱스 위주로 탐색 범위를 좁힐 수 있음
    for (int i = 0; i <= doc_length; i++) {
        int temp_y, temp_x;
        get_screen_pos(i, &temp_y, &temp_x);

        if (temp_y == target_y) {
            int dist = abs(temp_x - cur_x);
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = i;
            }
            // 같은 줄에서 x좌표가 멀어지기 시작하면 루프 중단 (최적화)
            else if (dist > min_dist) {
                 break; 
            }
        }
        // 목표 줄보다 더 아래로 내려갔다면 탐색 종료
        else if (temp_y > target_y) {
            break;
        }
    }

    if (best_idx != -1) {
        cursor_idx = best_idx;
    }
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

    // 상단바 그리기
    if (can_i_write) {
        attron(COLOR_PAIR(1)); // 빨간색 (작성 모드)
        mvprintw(0, 0, "[작성 모드] 작성 중... (ESC: 저장/반납)  현재 작성자 %s", current_writer);
        attroff(COLOR_PAIR(1));
    } else {
        attron(COLOR_PAIR(8)); // 흰배경 검은글씨 (읽기 모드)
        mvprintw(0, 0, "[읽기 모드] 엔터(Enter)를 누르면 작성 권한 요청 (검색: F)");
        if (strlen(current_writer) > 0) {
            mvprintw(0, 60, "현재 작성자: %s", current_writer);
        }
        attroff(COLOR_PAIR(8));
    }

    // 본문 내용 그리기
    int screen_y = 1; 
    move(screen_y, 0); 
    
    for (int i = 0; i < doc_length; i++) {
        int color_id = get_user_color_pair(doc_buffer[i].author, users, user_count);
        
        attron(COLOR_PAIR(color_id));
        printw("%c", doc_buffer[i].ch);
        attroff(COLOR_PAIR(color_id));
    }

    // 커서 위치 잡기
    int cur_y, cur_x;
    get_screen_pos(cursor_idx, &cur_y, &cur_x);
    move(screen_y + cur_y, cur_x);

    refresh();
}

void send_color_db_to_server(int sock, const char *doc_name) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "user_data/%s_users.txt", doc_name);
    
    int fd = open(path, O_RDONLY);
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.command = CMD_SYNC_USER_DB;

    if (fd != -1) {
        // 파일 읽어서 패킷에 담기
        int len = read(fd, pkt.text_content, MAX_BUFFER - 1);
        if (len < 0) len = 0;
        pkt.text_content[len] = '\0';
        pkt.text_len = len;
        close(fd);
    } else {
        pkt.text_len = 0; // 파일 없으면 빈 내용 전송 (초기화)
    }

    write(sock, &pkt, sizeof(Packet));
    printf("[HOST] Sent User Color DB to Server.\n");
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
                strcpy(doc_buffer[i].author, pkt.author_contents[i]);
            }
            doc_length = len;
        } else if (pkt.command == CMD_LOCK_GRANTED) {

            can_i_write = 1;
            char temp[100];
            snprintf(temp, sizeof(temp), "나(%s)", pkt.username);
            strcpy(current_writer, temp); 

        } else if (pkt.command == CMD_LOCK_DENIED) {

            mvprintw(LINES-1, 0, "거절됨: %s", pkt.message);

        } else if (pkt.command == CMD_RELEASE_LOCK) {

            save_document(current_working_doc_name, users, user_count);
            strcpy(current_writer, "");
            can_i_write = 0;
            mvprintw(0, 40, "[알림] %s님이 저장함 (내 파일도 업데이트됨)   ", pkt.username);

        } else if (pkt.command == CMD_INSERT) {

            server_insert_char(pkt.cursor_index, pkt.ch, pkt.username);
            if (pkt.cursor_index <= cursor_idx) cursor_idx++;

        } else if (pkt.command == CMD_DELETE) {

            server_delete_char(pkt.cursor_index);
            if (pkt.cursor_index < cursor_idx) cursor_idx--;

        }  else if (pkt.command == CMD_LOCK_UPDATE){

            strcpy(current_writer,pkt.username);
            can_i_write = 0;

        } else if (pkt.command == CMD_SAVE_USER) {
            char path[2048];
            snprintf(path, sizeof(path), "user_data/%s_userslog.txt", current_working_doc_name);
            
            int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644); // write 전용, 이어쓰기
            if (fd != -1) {
                char line[200];
                sprintf(line, "%s %s\n", pkt.username, pkt.message);
                write(fd, line, strlen(line));
                close(fd);
            }
            
            if (users != NULL) {
                free(users); // 기존 목록 삭제
                users = NULL;
            }
            // 방금 저장한 파일에서 최신 명단을 다시 불러옴
            users = read_persons(current_working_doc_name, &user_count);
            
        } else if (pkt.command == CMD_UPDATE_COLOR) {
            
            // (register_person은 append 모드라 파일이 있으면 끝에 추가됨)
            register_person(current_working_doc_name, pkt.username, pkt.message);

            // 메모리 싹 비우고 파일에서 다시 불러오기
            if (users != NULL) {
                free(users);
                users = NULL;
            }
            users = read_persons(current_working_doc_name, &user_count);
        } else if (pkt.command == CMD_SYNC_USER_DB) {
            
            // 로컬 파일(user_data/xxx_users.txt)에 덮어쓰기
            char path[MAX_PATH + 50];
            snprintf(path, sizeof(path), "user_data/%s_users.txt", current_working_doc_name);
            
            int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); // TRUNC로 내용 싹 지우고 새로 씀
            if (fd != -1) {
                write(fd, pkt.text_content, pkt.text_len);
                close(fd);
            }

            // 메모리 리로드 (화면 갱신용)
            if (users != NULL) {
                free(users);
                users = NULL;
            }
            users = read_persons(current_working_doc_name, &user_count);
            
        }

        if (!is_searching) {
            draw_document(pkt.username);
        }
        
        pthread_mutex_unlock(&win_mutex);
    }
    server_connected = 0; 
    return NULL;
}

void run_network_text_editor(int socket_fd, char *username, int is_host, char *doc_name) {
    my_socket = socket_fd;
    server_connected = 1; // 함수 시작 시 연결 상태 초기화
    strcpy(current_working_doc_name, doc_name);

    can_i_write = 0;              // 강제로 '읽기 모드'로 시작
    memset(current_writer, 0, sizeof(current_writer)); // 작성자 이름 초기화
    strcpy(current_writer, "");

    pthread_t r_thread;

    if (users != NULL) free(users); 
    users = read_persons(doc_name, &user_count);

    setlocale(LC_ALL, ""); // 한글 지원
    initscr();
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

    // 내 소켓으로 쓰레드 생성
    pthread_create(&r_thread, NULL, recv_thread_func, NULL);
    pthread_detach(r_thread);

    //방장이면 처음 문서 올리기
    if (is_host) {
        char actual_filename[256];
        snprintf(actual_filename, 256, "%s.txt", doc_name);
        load_document(actual_filename); 
        cursor_idx = doc_length; 
        
        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));
        pkt.command = CMD_SYNC_ALL;
        strcpy(pkt.username, username);
        for (int i = 0; i < doc_length; i++) {
            pkt.text_content[i] = doc_buffer[i].ch;
            strcpy(pkt.author_contents[i], doc_buffer[i].author);
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

        if (server_connected == 0) {
             break; // 루프 탈출 -> 종료 절차로 이동
        }

        refresh();

        // 키 입력 전 화면 갱신 (로컬 이동 반영을 위해)
        pthread_mutex_lock(&win_mutex);
        if (!is_searching) {
            draw_document(username);
        }
        pthread_mutex_unlock(&win_mutex);
        // 키 입력
        ch = getch(); 
        if(ch == ERR){
                continue;
        }
        if (can_i_write) {
            Packet pkt;
            memset(&pkt, 0, sizeof(Packet));
            strcpy(pkt.username, username);

            if (ch == KEY_LEFT) {
                if (cursor_idx > 0) cursor_idx--;
                continue; // 서버 전송 불필요하므로 continue
            }
            else if (ch == KEY_RIGHT) {
                if (cursor_idx < doc_length) cursor_idx++;
                continue;
            }
            else if (ch == KEY_UP) {
                move_cursor_vertically(-1);
                continue;
            }
            else if (ch == KEY_DOWN) {
                move_cursor_vertically(1);
                continue;
            }

            if (ch == 27) { // ESC 키 입력
                //  내 컴퓨터에 저장
                save_document(doc_name, users, user_count);
                
                // 서버에 반납 신호 전송
                memset(&pkt, 0, sizeof(Packet)); 
                pkt.command = CMD_RELEASE_LOCK;
                strcpy(pkt.username, username); 
                write(my_socket, &pkt, sizeof(Packet));
                
                // 내 상태 변경 (읽기 모드)
                can_i_write = 0; 
                strcpy(current_writer, ""); // 상단바의 이름("나")을 지움
                
                // 화면 강제 갱신 (빨간색 -> 흰색 전환)
                pthread_mutex_lock(&win_mutex);
                draw_document(username);
                pthread_mutex_unlock(&win_mutex);

            } else if (ch == KEY_BACKSPACE || ch == 127) {
                pkt.command = CMD_DELETE;
                pkt.cursor_index = cursor_idx; 
                write(my_socket, &pkt, sizeof(Packet));
                
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
            }
        
        // 읽기 상태일 때 작성 권한 요청
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
            }else if (ch=='f'||ch=='F') {
                pthread_mutex_lock(&win_mutex);
                is_searching=1;
                pthread_mutex_unlock(&win_mutex);

                save_document(doc_name,users,user_count);
                char search_filename[MAX_PATH];
                snprintf(search_filename,sizeof(search_filename),"%s.txt",doc_name);
                create_question_box(search_filename);

                pthread_mutex_lock(&win_mutex);
                is_searching=0; 
                clear();          
                timeout(100);     
                curs_set(1);     
                draw_document(username);
                pthread_mutex_unlock(&win_mutex);
            }
        }
    }

    if(users!=NULL) {
        free(users);
        users=NULL;
    }

    endwin();       // ncurses 모드 종료
    system("clear"); // 터미널 화면을 깔끔하게 지움 (Linux/Mac)

    endwin();
}

// 색이 이미 사용중?
int is_color_taken(const char *color, Person *people, int count) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(people[i].color, color) == 0) {
            return 1; // 이미 사용 중
        }
    }
    return 0; // 사용 가능
}

// 색상 선택 및 사용자 등록을 처리
void process_login_and_color_selection(int socket_fd, char *doc_name, char *username) {
    int user_count = 0;
    Person *existing_users = read_persons(doc_name, &user_count);
    
    // 이미 등록된 유저인지 확인(색상에서)
    int already_registered = 0;
    if (existing_users != NULL) {
        for (int i = 0; i < user_count; i++) {
            if (strcmp(existing_users[i].username, username) == 0) {
                already_registered = 1;
                break;
            }
        }
    }

    // 이미 등록되어 있다면 바로 종료 (기존 색상 사용)
    if (already_registered) {
        if(existing_users) free(existing_users);
        return;
    }

    // 등록되지 않은 신규 유저라면 -> 색상 선택
    curs_set(0);
    noecho();
    keypad(stdscr, TRUE);

    int selected_idx = 0;
    int ch;
    
    // 사용 가능한 색상만 추려서 별도 배열에 저장
    char available_colors[10][20];
    int avail_count = 0;

    for (int i = 0; i < NUM_COLORS; i++) {
        if (!is_color_taken(ALL_COLORS[i], existing_users, user_count)) {
            strcpy(available_colors[avail_count], ALL_COLORS[i]);
            avail_count++;
        }
    }

    // 만약 남은 색상이 없다면 기본값(white)으로 자동 등록 (중복 허용)
    if (avail_count == 0) {
        register_person(doc_name, username, "white");

        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));
        pkt.command = CMD_UPDATE_COLOR;
        strcpy(pkt.username, username);
        strcpy(pkt.message, "white"); // message에 색상 넣기
        write(socket_fd, &pkt, sizeof(Packet));
        if(existing_users) free(existing_users);
        clear();
        mvprintw(LINES/2, (COLS-50)/2, "All colors taken. Auto-registered as White.");
        refresh();
        usleep(1000000);
        return;
    }

    // 색상 선택 루프
    while (1) {
        clear();
        wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');
        
        mvprintw(3, (COLS-40)/2, "=== Welcome, %s! ===", username);
        mvprintw(5, (COLS-50)/2, "You are new to '%s'. Please choose your color.", doc_name);

        int start_y = 8;
        
        for (int i = 0; i < avail_count; i++) {
            if (i == selected_idx) {
                attron(A_REVERSE); // 선택된 항목 반전
                mvprintw(start_y + i, (COLS-20)/2, "-> %s", available_colors[i]);
                attroff(A_REVERSE);
            } else {
                mvprintw(start_y + i, (COLS-20)/2, "   %s", available_colors[i]);
            }
        }
        
        mvprintw(LINES-3, (COLS-30)/2, "[UP/DOWN] Move  [ENTER] Select");
        refresh();

        ch = getch();
        if (ch == KEY_UP) {
            if (selected_idx > 0) selected_idx--;
            else selected_idx = avail_count - 1; 
        } else if (ch == KEY_DOWN) {
            if (selected_idx < avail_count - 1) selected_idx++;
            else selected_idx = 0; 
        } else if (ch == '\n' || ch == KEY_ENTER) {
            // 선택 완료
            break;
        }
    }

    // 선택한 색상으로 파일에 등록
    register_person(doc_name, username, available_colors[selected_idx]);

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.command = CMD_UPDATE_COLOR;
    strcpy(pkt.username, username);
    strcpy(pkt.message, available_colors[selected_idx]); 
    write(socket_fd, &pkt, sizeof(Packet));
    
    clear();
    mvprintw(LINES/2, (COLS-40)/2, "Registered successfully as %s!", available_colors[selected_idx]);
    refresh();
    usleep(800000); 

    if(existing_users) free(existing_users);
}

void send_db_file_to_server(int sock, const char *filename) {
    char path[4096];
    snprintf(path, sizeof(path), "user_data/%s_userslog.txt", filename);

    int fd = open(path, O_RDONLY); 
    
    // 패킷 준비
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.command = CMD_LOAD_USERS;

    if (fd == -1) {
        // 파일이 없으면 새로 생성만 하고, 읽기 과정은 건너뜀 (내용은 비어있음)
        // 하지만 패킷 전송 로직은 수행해야 함!
        int new_fd = open(path, O_CREAT | O_RDWR, 0644);
        if (new_fd != -1) close(new_fd);
        
        pkt.text_len = 0;
        pkt.text_content[0] = '\0';
        
    } else {
        // 파일이 있으면 읽어서 패킷에 담음
        int len = read(fd, pkt.text_content, MAX_BUFFER - 1);
        if (len < 0) len = 0;
        pkt.text_content[len] = '\0'; // 문자열 끝 처리
        pkt.text_len = len;
        
        close(fd);
        printf("[HOST] 기존 유저 DB 로드하여 전송 (len: %d)\n", len);
    }

    // 파일 유무와 상관없이 이 write가 실행되어야 함
    write(sock, &pkt, sizeof(Packet));
}

void send_doc_to_client(int sock) {
    if (doc_length > 0) {
        Packet sync_pkt;
        memset(&sync_pkt, 0, sizeof(Packet));
        sync_pkt.command = CMD_SYNC_ALL;
        
        // 서버 메모리(doc_buffer)에 있는 내용을 패킷에 담음
        for(int i=0; i<doc_length; i++) {
            sync_pkt.text_content[i] = doc_buffer[i].ch;
            strcpy(sync_pkt.author_contents[i], doc_buffer[i].author);
        }
        sync_pkt.text_len = doc_length;
        
        write(sock, &sync_pkt, sizeof(Packet));
    }
}