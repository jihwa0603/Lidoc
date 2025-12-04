#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <locale.h>

#include "login.h"
#include "managing_documents.h"
#include "common_protocol.h"
#include "server.h"

#define MAX_USERNAME_LEN 15
#define MAX_PASSWORD_LEN 15
#define MAX_USERS 10  // 최대 유저 수 제한

int WIDTH, HEIGHT;
int loginFlag = 0;

typedef struct {
    char id[15];
    unsigned long password;
} User_Process;

// 비밀번호 해시 함수
unsigned long hash_password(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void draw_border() {
    clear();
    getmaxyx(stdscr, HEIGHT, WIDTH);
    wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');
    refresh();
}

void init_screen() {
    setlocale(LC_ALL, "");
    initscr();
    curs_set(0);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
}

// 비밀번호 입력 처리 (별표 표시)
void get_pw(char *buffer, int y, int x) {
    int i = 0;
    int ch;
    move(y, x);
    while (1) {
        ch = getch();
        if (ch == '\n' || ch == '\r') {
            buffer[i] = '\0';
            break;
        } else if (ch == 127 || ch == KEY_BACKSPACE || ch == 8) {
            if (i > 0) {
                i--;
                mvaddch(y, x + i, ' ');
                move(y, x + i);
            }
        } else if (i < MAX_PASSWORD_LEN && ch >= 32 && ch <= 126) { // 출력 가능한 문자만
            buffer[i++] = ch;
            addch('*');
        }
        refresh();
    }
}

// 등록된 유저 수 확인 함수
int get_user_count(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return 0;

    int count = 0;
    char temp_id[50];
    unsigned long temp_pw;
    
    while (fscanf(fp, "%s %lu", temp_id, &temp_pw) != EOF) {
        count++;
    }
    fclose(fp);
    return count;
}

// 회원가입 폼
void RegisterForm(const char *db_path) {
    // 1. 인원 제한 체크
    int count = get_user_count(db_path);
    if (count >= MAX_USERS) {
        clear();
        draw_border();
        mvprintw(HEIGHT/2, (WIDTH-40)/2, "Registration Failed: Max users (%d) reached!", MAX_USERS);
        mvprintw(HEIGHT/2+2, (WIDTH-20)/2, "Press any key to return.");
        getch();
        return;
    }

    char *buffer = malloc(MAX_PASSWORD_LEN + 1);
    clear();
    draw_border();
    
    User_Process t1;
    mvprintw(HEIGHT/2 - 4, (WIDTH-16)/2, "=== REGISTER ===");
    
    // ID 입력
    mvprintw(HEIGHT/2 - 1, WIDTH/2 - 10, "Enter ID: ");
    echo();
    curs_set(1);
    mvgetnstr(HEIGHT/2 - 1, WIDTH/2 + 2, t1.id, MAX_USERNAME_LEN);
    noecho();
    curs_set(0);

    // PW 입력
    mvprintw(HEIGHT/2 + 1, WIDTH/2 - 10, "Enter PW: ");
    get_pw(buffer, HEIGHT/2 + 1, WIDTH/2 + 2);

    // 저장 로직
    char buf[100];
    unsigned long hashed_pw = hash_password(buffer);
    sprintf(buf, "%s %lu\n", t1.id, hashed_pw);

    // 파일 열기 (없으면 생성)
    int fid = open(db_path, O_WRONLY | O_CREAT | O_APPEND, 0644); // 0700 -> 0644 (일반권한)
    if (fid == -1) {
        endwin();
        perror("File Open Error");
        exit(1);
    }
    
    if (write(fid, buf, strlen(buf)) == -1) {
        close(fid);
        endwin();
        perror("Write Error");
        exit(1);
    }

    close(fid);
    free(buffer);

    mvprintw(HEIGHT/2 + 4, (WIDTH-30)/2, "Registration Complete!");
    mvprintw(HEIGHT/2 + 5, (WIDTH-20)/2, "Press any key.");
    getch();
}

// 로그인 폼
void loginForm(const char *db_path, char *result_id) {
    clear();
    draw_border();
    
    char *buffer = malloc(MAX_PASSWORD_LEN + 1);
    User_Process t1;
    User_Process che;

    mvprintw(HEIGHT/2 - 4, (WIDTH-10)/2, "=== LOGIN ===");
    
    // ID 입력
    mvprintw(HEIGHT/2 - 1, WIDTH/2 - 10, "ID: ");
    echo();
    curs_set(1);
    mvgetnstr(HEIGHT/2 - 1, WIDTH/2 - 5, t1.id, MAX_USERNAME_LEN);
    noecho();
    curs_set(0);

    // PW 입력
    mvprintw(HEIGHT/2 + 1, WIDTH/2 - 10, "PW: ");
    get_pw(buffer, HEIGHT/2 + 1, WIDTH/2 - 5);
    
    unsigned long temp_pw = hash_password(buffer);
    FILE *in = fopen(db_path, "r");

    loginFlag = 0;
    if (in == NULL) {
        mvprintw(HEIGHT/2 + 4, (WIDTH-30)/2, "No users found. Please Register.");
    } else {
        while (fscanf(in, "%s %lu", che.id, &che.password) != EOF) {
            if (strcmp(che.id, t1.id) == 0 && temp_pw == che.password) {
                loginFlag = 1;
                break;
            }
        }
        fclose(in);
    }
    
    if (loginFlag) {
        strcpy(result_id, t1.id);
        mvprintw(HEIGHT/2 + 4, (WIDTH-20)/2, "Login Successful!");
    } else {
        mvprintw(HEIGHT/2 + 4, (WIDTH-20)/2, "Login Failed!");
    }
    
    getch();
    free(buffer);
}

int network_login_process(int sock, char *username_out, char* doc_name) {
    char id[20];
    char *pw_buf = malloc(20);
    char hash_str[30];
    int selection;

    while (1) {
        selection = ask_auth_menu(); // 0:로그인, 1:가입

        clear();
        draw_border();

        if (selection == 0) { // === 로그인 ===
            mvprintw(HEIGHT/2 - 4, (WIDTH-10)/2, "=== REMOTE LOGIN ===");
            
            // ID 입력
            mvprintw(HEIGHT/2 - 1, WIDTH/2 - 10, "ID: ");
            echo(); curs_set(1);
            mvgetnstr(HEIGHT/2 - 1, WIDTH/2 - 5, id, 15);
            noecho(); curs_set(0);

            // PW 입력
            mvprintw(HEIGHT/2 + 1, WIDTH/2 - 10, "PW: ");
            get_pw(pw_buf, HEIGHT/2 + 1, WIDTH/2 - 5); // 기존 get_pw 함수 재사용
            
            // 해시 -> 문자열
            unsigned long hashed = hash_password(pw_buf);
            sprintf(hash_str, "%lu", hashed);

            // 서버 전송
            Packet pkt; memset(&pkt, 0, sizeof(Packet));
            pkt.command = CMD_AUTH_LOGIN;
            strcpy(pkt.username, id);
            strcpy(pkt.message, hash_str);
            write(sock, &pkt, sizeof(Packet));

            // 결과 대기
            Packet res;
            while(read(sock, &res, sizeof(Packet)) > 0) {
                if (res.command == CMD_AUTH_RESULT) break;
            }

            if (res.message[0] == '1') { // 성공
                strcpy(username_out, id);
                free(pw_buf);
                mvprintw(HEIGHT/2 + 4, (WIDTH-20)/2, "Login Success!");
                refresh(); sleep(1);
                return 1;
            } else {
                mvprintw(HEIGHT/2 + 4, (WIDTH-20)/2, "Login Failed!");
                getch();
            }

        } else { // === 회원가입 ===
            mvprintw(HEIGHT/2 - 4, (WIDTH-16)/2, "=== REMOTE REGISTER ===");
            
            mvprintw(HEIGHT/2 - 1, WIDTH/2 - 10, "New ID: ");
            echo(); curs_set(1);
            mvgetnstr(HEIGHT/2 - 1, WIDTH/2 + 2, id, 15);
            noecho(); curs_set(0);

            mvprintw(HEIGHT/2 + 1, WIDTH/2 - 10, "New PW: ");
            get_pw(pw_buf, HEIGHT/2 + 1, WIDTH/2 + 2);

            unsigned long hashed = hash_password(pw_buf);
            sprintf(hash_str, "%lu", hashed);

            Packet pkt; memset(&pkt, 0, sizeof(Packet));
            pkt.command = CMD_AUTH_REGISTER;
            strcpy(pkt.username, id);
            strcpy(pkt.message, hash_str);
            write(sock, &pkt, sizeof(Packet));

            Packet res;
            while(read(sock, &res, sizeof(Packet)) > 0) {
                
                // 결과 패킷이면 루프 탈출
                if (res.command == CMD_AUTH_RESULT) {
                    break;
                }
                
                // 방장인 경우, 서버가 "이 유저 저장해"라고 보낸 패킷 처리
                else if (res.command == CMD_SAVE_USER) {
                    char path[2048];
                    snprintf(path, sizeof(path), "user_data/%s_userslog.txt", doc_name);
                    
                    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
                    if (fd != -1) {
                        char line[200];
                        sprintf(line, "%s %s\n", res.username, res.message);
                        write(fd, line, strlen(line));
                        close(fd);
                        // 디버깅용 메시지 (필요 시 주석 해제)
                        // mvprintw(HEIGHT/2 + 8, (WIDTH-40)/2, "[DEBUG] Saved to file locally.");
                        // refresh();
                    }
                }
            }

            if (res.message[0] == '1') {
                mvprintw(HEIGHT/2 + 4, (WIDTH-30)/2, "Register Success!");
            } else {
                mvprintw(HEIGHT/2 + 4, (WIDTH-30)/2, "Failed (ID Exists?)");
            }
            mvprintw(HEIGHT/2 + 6, (WIDTH-20)/2, "Press any key.");
            getch();
        }
    }
}

// 로그인/회원가입 선택 메뉴
int ask_auth_menu() {
    curs_set(0);
    int selection = 0;
    while(1) {
        clear();
        draw_border();
        mvprintw(HEIGHT/2 - 3, (WIDTH-24)/2, "=== Authentication ===");
        
        if(selection == 0) attron(A_REVERSE);
        mvprintw(HEIGHT/2 - 1, (WIDTH-10)/2, "1. Login");
        attroff(A_REVERSE);

        if(selection == 1) attron(A_REVERSE);
        mvprintw(HEIGHT/2 + 1, (WIDTH-13)/2, "2. Register");
        attroff(A_REVERSE);

        int ch = getch();
        if (ch == KEY_UP) selection = 0;
        else if (ch == KEY_DOWN) selection = 1;
        else if (ch == '\n' || ch == KEY_ENTER) curs_set(0); return selection; // 0: Login, 1: Register
    }
}

// 외부에서 호출하는 메인 함수
int do_auth_process(const char *db_path, char *username_out) {
    // ncurses 초기화가 안 되어 있다면 수행 (보통 main에서 하지만 안전장치)
    // init_screen(); 

    while (1) {
        int choice = ask_auth_menu();
        if (choice == 0) { // Login
            loginForm(db_path, username_out);
            if (loginFlag) return 1; // 성공 시 리턴
        } else { // Register
            RegisterForm(db_path);
            // 회원가입 후에는 다시 메뉴로 돌아감 (바로 로그인시키지 않음)
        }
    }
}