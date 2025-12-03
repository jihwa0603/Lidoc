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
int can_i_write = 0;            // 0: 읽기 전용, 1: 쓰기 가능
char current_writer[MAX_NAME] = ""; // 지금 누가 쓰고 있는지
pthread_mutex_t win_mutex = PTHREAD_MUTEX_INITIALIZER; // 화면 충돌 방지

Cell doc_buffer[MAX_BUFFER];
int doc_length = 0;

int cursor_idx = 0;     
Person *users = NULL;   
int user_count = 0;

// [클라이언트 전역 변수 혹은 구조체 상태]
char status_msg[100] = "Ready";

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
    FILE *fp = NULL;
    char path1[MAX_PATH + 50];
    char path2[MAX_PATH + 50];
    char path3[MAX_PATH + 50];
    char filename[MAX_PATH];

    snprintf(path1, sizeof(path1), "documents/%s", filename);
    
    printf("\n문서 생성(파일명: %s)\n", filename);

    fp = fopen(path1, "w");
    if (fp == NULL) {
        perror("[documents] 폴더에 파일 생성 실패");
    } else {
        fclose(fp);
        printf("[documents] 폴더에 문서 생성 성공: '%s'\n", path1);
    }

    snprintf(path2, sizeof(path2), "documents_with_user/%s", filename);

    fp = fopen(path2, "w");
    if (fp == NULL) {
        perror("[documents_with_user] 폴더에 파일 생성 실패");
    } else {
        fclose(fp);
        printf("[documents_with_user] 폴더에 문서 생성 성공: '%s'\n", path2);
    }

    snprintf(path3, sizeof(path3), "user_data/%s_users.txt", document_name);
    fp = fopen(path3, "w");
    if (fp == NULL) {
        perror("[user_data] 폴더에 파일 생성 실패");
    } else {
        fclose(fp);
        printf("[user_data] 폴더에 문서 생성 성공: '%s'\n", path3);
    }
}

void register_person(const char *filename, const char *username, const char *color) {
    FILE *fp = NULL;
    char path[MAX_PATH];
    
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    fp = fopen(path, "a"); 
    if (fp == NULL) {
        perror("사용자 정보 파일 생성 실패");
    } else {
        fprintf(fp, "%s %s 0\n", username,color);
        fclose(fp);
        printf("사용자 정보 등록 성공: '%s'\n", path);
    }
}

Person* read_persons(const char *filename, int *count) {
    FILE *fp = NULL;
    char path[MAX_PATH];
    
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("사용자 정보 파일 열기 실패");
        return 0;
    } else {
        // 메모리 할당 구분 가능한 종류는 7가지 이지만 좀 더 크게 10명으로 할당
        int max_size = 10;
        Person *people = (Person*)malloc(sizeof(Person) * max_size);
        
        int i = 0;
        
        // 파일 읽기
        while (fscanf(fp, "%s %s %ld", people[i].username, people[i].color, &people[i].context) != EOF) {
            i++;
            
            // 만약 10명이 넘으면 멈춤
            if (i >= max_size) break; 
        }

        fclose(fp);

        // 몇 명인지 알려줌
        *count = i;

        // 배열 반환
        return people;
    }
}

void save_user_data(const char *filename, Person *people, int count) {
    FILE *fp = NULL;
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    fp = fopen(path, "w");
    if (fp == NULL) {
        perror("사용자 정보 파일 열기 실패");
    } else {
        for (int i = 0; i < count; i++) {
            fprintf(fp, "%s %s %ld\n", people[i].username, people[i].color, people[i].context);
        }
        fclose(fp);
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
    // 변경된 내용을 파일에 다시 저장
    FILE *fp = NULL;
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    fp = fopen(path, "w");
    if (fp == NULL) {
        perror("사용자 정보 파일 열기 실패");
    } else {
        for (int i = 0; i < count; i++) {
            fprintf(fp, "%s %s %ld\n", people[i].username, people[i].color, people[i].context);
        }
        fclose(fp);
        printf("사용자 색상 변경 성공: '%s'\n", path);
    }
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
    // 등록된 사용자 목록에서 검색
    if (people != NULL && count > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(people[i].username, target_name) == 0) {
                // 문자열을 ncurses 색상 코드로 변환
                int color_code = get_ncurses_color_code(people[i].color);
                
                // 색상 번호
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
    // 목록에 없으면 기본 흰색
    return 7;
}

void load_document(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "documents_with_user/%s", filename);

    FILE *fp = fopen(path, "r");
    doc_length = 0;
    
    if (fp == NULL) return;

    char current_author[MAX_NAME] = "Unknown";
    int ch;
    int state = 0;
    int escape = 0;

    char name_buf[MAX_NAME];
    int name_idx = 0;

    while ((ch = fgetc(fp)) != EOF && doc_length < MAX_BUFFER) {
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
    fclose(fp);
}

void save_document(const char *doc_name, Person *people, int user_count) {
    char path_plain[256];
    char path_tagged[256];
    
    // 순수 문서만
    snprintf(path_plain, sizeof(path_plain), "documents/%s.txt", doc_name);
    FILE *fp_plain = fopen(path_plain, "w");
    if (fp_plain) {
        for (int i = 0; i < doc_length; i++) {
            fputc(doc_buffer[i].ch, fp_plain);
        }
        fclose(fp_plain);
    }

    // 누가 썼는지 포함
    snprintf(path_tagged, sizeof(path_tagged), "documents_with_user/%s.txt", doc_name);
    FILE *fp_tagged = fopen(path_tagged, "w");
    if (fp_tagged) {
        char last_author[MAX_NAME] = "";
        int person_index = -1;

        for(int i = 0; i < user_count; i++) {
            people[i].context = 0;
        }

        for (int i = 0; i < doc_length; i++) {
            if (strcmp(last_author, doc_buffer[i].author) != 0) {
                fprintf(fp_tagged, "[%s]", doc_buffer[i].author);
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
            
            if (ch == '\\') {
                fputc('\\', fp_tagged);
                fputc('\\', fp_tagged); 
            }
            else if (ch == '[') {
                fputc('\\', fp_tagged);
                fputc('[', fp_tagged);
            }
            else if (ch == ']') {
                fputc('\\', fp_tagged);
                fputc(']', fp_tagged);
            }
            else {
                fputc(ch, fp_tagged);
            }
        }
        fclose(fp_tagged);
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
    // 범위 체크
    if (index < 0 || index > doc_length || doc_length >= MAX_BUFFER - 1) return;

    // 뒤로 한 칸씩 밀기 (공간 확보)
    for (int i = doc_length; i > index; i--) {
        doc_buffer[i] = doc_buffer[i - 1];
    }

    // 값 넣기
    doc_buffer[index].ch = ch;
    strcpy(doc_buffer[index].author, username);
    
    // 길이 증가
    doc_length++;
}

// 서버가 특정 위치(index)의 문자를 삭제하는 함수
void server_delete_char(int index) {
    // 범위 체크
    if (index < 0 || index >= doc_length) return;

    // 앞으로 한 칸씩 당기기 (삭제)
    for (int i = index; i < doc_length - 1; i++) {
        doc_buffer[i] = doc_buffer[i + 1];
    }

    // 길이 감소
    doc_length--;
}

void draw_document(const char *my_username) {
    clear();

    // 1. 상단바 그리기
    if (can_i_write) {
        attron(COLOR_PAIR(1)); // 빨간색 (작성 모드)
        mvprintw(0, 0, "[작성 모드] 작성 중... (ESC: 저장/반납)");
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
        // 작성자에 맞는 색상 찾기
        int color_id = get_user_color_pair(doc_buffer[i].author, users, user_count);
        
        attron(COLOR_PAIR(color_id));
        printw("%c", doc_buffer[i].ch);
        attroff(COLOR_PAIR(color_id));
    }

    // 3. 커서 위치 잡기
    // 내가 작성 중일 때는 내 커서 위치(cursor_idx)를 따라가고,
    // 읽기 모드일 때는 그냥 문서 맨 끝이나 0에 둠 (혹은 cursor_idx 유지)
    int cur_y, cur_x;
    get_screen_pos(cursor_idx, &cur_y, &cur_x);
    move(screen_y + cur_y, cur_x);

    refresh();
}

void *recv_thread_func(void *arg) {
    Packet pkt;
    
    // 서버가 보내주는 패킷을 계속 읽음
    while (read(my_socket, &pkt, sizeof(Packet)) > 0) {
        pthread_mutex_lock(&win_mutex); // 화면 그리는 동안 멈춰!

        if (pkt.command == CMD_SYNC_ALL) {
            // ===================================================
            // ★ [손님/전체] 서버로부터 전체 문서 덮어쓰기
            // ===================================================
            doc_length = 0;
            int len = pkt.text_len;
            if (len > MAX_BUFFER) len = MAX_BUFFER;

            for (int i = 0; i < len; i++) {
                doc_buffer[i].ch = pkt.text_content[i];
                // 서버에서 줄 때 작성자 정보가 없으면 "Server" 또는 받은 username 사용
                strcpy(doc_buffer[i].author, pkt.username); 
            }
            doc_length = len;
            
            // 화면 갱신
            // clear(); // 필요하면 호출
        } else if (pkt.command == CMD_LOCK_GRANTED) {
            // "너 써도 돼!" -> 권한 획득
            can_i_write = 1;
            strcpy(current_writer, pkt.username); // "나"
            
        } else if (pkt.command == CMD_LOCK_DENIED) {
            // "안 돼!" -> 알림 표시
            mvprintw(LINES-1, 0, "거절됨: %s", pkt.message);
            
        } else if (pkt.command == CMD_LOCK_UPDATE) {
            // "지금 누가 쓰는 중이야/해제했어"
            // 메시지를 화면 상단이나 하단에 표시
            mvprintw(0, 40, "[알림: %s]      ", pkt.message);
            
            // 만약 "해제" 메시지라면, 작성자 이름 초기화
            if (strstr(pkt.message, "잠금") != NULL) {
                // 패킷에 들어있는 username을 현재 작성자로 등록
                strcpy(current_writer, pkt.username);
            }
            if (strstr(pkt.message, "해제")) {
                strcpy(current_writer, "");
                can_i_write = 0; // 혹시 모르니 안전하게
            }

        } else if (pkt.command == CMD_INSERT) {
            // 문서 내용 업데이트 (서버 명령대로)
            // 주의: 여기서는 이미 서버가 검증한 위치이므로 바로 넣음
            server_insert_char(pkt.cursor_index, pkt.ch, pkt.username);
            // 남이 썼는데 내 커서보다 앞이면 내 커서도 밀려야 함 (옵션)
            if (pkt.cursor_index <= cursor_idx) cursor_idx++;
            
        } else if (pkt.command == CMD_DELETE) {
            // 문서 삭제 업데이트
            server_delete_char(pkt.cursor_index);
            // 남이 지웠는데 내 커서보다 앞이면 당겨짐 (옵션)
            if (pkt.cursor_index < cursor_idx) cursor_idx--;
        }

        draw_document(pkt.username);
        
        pthread_mutex_unlock(&win_mutex);
    }
    return NULL;
}

// [새로운 함수 2] 네트워크용 에디터 실행 함수 (메인)
void run_network_text_editor(int socket_fd, char *username,int is_host, char *doc_name) {
    my_socket = socket_fd;
    pthread_t r_thread;

    // 1. 사용자 정보 로드 (전역 변수 users에 저장)
    if (users != NULL) free(users); // 혹시 모를 초기화
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
    raw();
    timeout(100); // 0.1초 대기

    // 수신 스레드 시작
    pthread_create(&r_thread, NULL, recv_thread_func, NULL);
    pthread_detach(r_thread);

    // =======================================================
    // ★ [방장 전용] 시작하자마자 내 문서 서버로 업로드
    // =======================================================
    if (is_host) {
        char actual_filename[256];
        snprintf(actual_filename, 256, "%s_by_users.txt", doc_name);
        load_document(actual_filename); 
        cursor_idx = doc_length; // 로드 후 커서 맨 뒤로

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
        // === 화면 그리기 ===
        pthread_mutex_lock(&win_mutex);
        draw_document(username); // ★ 함수 호출로 대체!
        pthread_mutex_unlock(&win_mutex);
        // ==================

        ch = getch(); 

        if (ch == ERR) continue; // 입력 없으면 다시 그림
        
        // 상단바 표시
        if (can_i_write) {
            attron(COLOR_PAIR(1)); // 빨간색 등 강조
            mvprintw(0, 0, "[작성 모드] 작성 중... (ESC: 저장/반납)");
            attroff(COLOR_PAIR(1));
        } else {
            mvprintw(0, 0, "[읽기 모드] 엔터(Enter)를 누르면 작성 권한 요청");
            if (strlen(current_writer) > 0) {
                mvprintw(1,0," (현재 작성자: %s)", current_writer);
            }
        }

        refresh();
        pthread_mutex_unlock(&win_mutex);
        // ======================

        // === 키 입력 처리 ===
        ch = getch(); // 여기서 대기

        if (can_i_write) {
            // [내가 작성자일 때]
            Packet pkt;
            strcpy(pkt.username, username);

            if (ch == 27) { // ESC 키
                pkt.command = CMD_RELEASE_LOCK;
                write(my_socket, &pkt, sizeof(Packet));
                can_i_write = 0; // 즉시 반납 상태로
                
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                pkt.command = CMD_DELETE;
                pkt.cursor_index = cursor_idx; // 현재 내 커서 위치
                write(my_socket, &pkt, sizeof(Packet));
                // 내 커서 이동 로직은 recv_thread가 업데이트 해줄 때까지 기다리거나
                // 예측해서 움직일 수 있음.

            } else {
                pkt.command = CMD_INSERT;
                pkt.ch = ch;
                pkt.cursor_index = cursor_idx;
                write(my_socket, &pkt, sizeof(Packet));
            }
            
        } else {
            // [읽기 전용일 때]
            if (ch == '\n') { // 엔터 키
                Packet pkt;
                pkt.command = CMD_REQUEST_LOCK;
                strcpy(pkt.username, username);
                write(my_socket, &pkt, sizeof(Packet));
                
            } else if (ch == 'q') { // 종료
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