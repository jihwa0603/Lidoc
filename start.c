#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <locale.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

#include "login.h"
#include "server.h"
#include "managing_documents.h"

void start_curses() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
}

void end_curses() {
    endwin();
}

void default_start(){
    start_curses();
    wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');

    int height, width;
    getmaxyx(stdscr, height, width); // 사이즈 얻기
    mvprintw(height / 2, (width - 20) / 2, "Welcome to the App!"); // 중간에 메세지 표기
    mvprintw((height) - 2, (width - 16), "from Team Lidoc"); // 팀명 표시
    mvprintw(height - 2, 2, "Press any key to continue...");
    refresh();
}

int show_the_list(){
    clear();
    wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');

    const char *items[] = {
        "1. Start Server",
        "2. Connect to Server",
        "3. Settings",
        "4. Help",
        "5. Exit"
    };

    int height, width;
    getmaxyx(stdscr, height, width); // 사이즈 얻기
    for (int i = 0; i < 5; i++) {
        mvprintw((height / 2) - 2 + i, (width - strlen(items[i])) / 2, "%s", items[i]);
    }
    mvprintw(height - 2, 2, "Use arrow keys to navigate and Enter to select.");

    mvprintw((height / 2) - 2,(width - strlen(items[0])) /2 - 4, "->"); // 초기 화살표
    
    static int current_selection = 0;

    while(1){
        int ch = getch();

        // 이전 위치의 화살표 지우기
        mvprintw((height / 2) - 2 + current_selection, (width - strlen(items[current_selection])) / 2 - 4, "  ");

        if(ch == KEY_UP){
            current_selection--;
            if(current_selection < 0){
                current_selection = 4;
            }
        } else if(ch == KEY_DOWN){
            current_selection++;
            if(current_selection > 4){
                current_selection = 0;
            }
        } else if(ch == '\n' || ch == KEY_ENTER){
            // 선택
            break;
        }

        // 바뀐 위치에 화살표 그리기
        mvprintw((height / 2) - 2 + current_selection, (width - strlen(items[current_selection])) / 2 - 4, "->");
        refresh();
    }

    return current_selection;
}

// IP, Port, 문서 이름을 입력받기
void get_connection_info(char *ip, int *port, char *doc_name, int show_file_list) {
    echo();
    curs_set(1);
    clear();

    int win_h = 20, win_w = 60;
    int start_y = LINES / 2 - win_h / 2, start_x = COLS / 2 - win_w / 2;

    WINDOW *win = newwin(win_h, win_w, start_y, start_x);
    wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');

    // 타이틀
    mvwprintw(win, 1, 2, "=== Connection Setup ===");
    mvwhline(win, 2, 1, ACS_HLINE, win_w - 2); // 구분선

    // 입력 라벨 표시
    mvwprintw(win, 4, 4, "Server IP     : [                          ]"); 
    mvwprintw(win, 6, 4, "Port          : [        ]");
    mvwprintw(win, 8, 4, "Document Name : [                          ]");

    // 파일 목록 보여주기 (show_file_list가 1일 때만)
    if (show_file_list) {
        mvwhline(win, 10, 1, ACS_HLINE, win_w - 2);
        mvwprintw(win, 11, 2, "[ Existing Files in 'documents/' ]");

        DIR *d;
        struct dirent *dir;
        d = opendir("documents");
        
        if (d) {
            int line = 13;
            int col = 4;
            while ((dir = readdir(d)) != NULL) {
                // ., .. 및 숨김 파일 제외
                if (dir->d_name[0] == '.') continue;

                // 화면 넘어가지 않게 제한
                if (line >= win_h - 1) break;

                mvwprintw(win, line, col, "- %s", dir->d_name);
                
                // 2열로 배치 로직 (간단하게)
                if (col == 4) {
                    col = 30; // 다음 파일은 오른쪽 열에
                } else {
                    col = 4;  // 다시 왼쪽 열로
                    line++;   // 줄 바꿈
                }
            }
            closedir(d);
        } else {
            mvwprintw(win, 13, 4, "폴더 설정이 안됐음");
        }
    } else {
        // 게스트일 경우 도움말 표시
        mvwprintw(win, 12, 4, "Enter the Host IP to connect.");
    }

    wrefresh(win);

    // 실제 입력 받기
    char ip_buf[30] = {0};
    char port_buf[10] = {0};

    flushinp();

    // IP 입력 이동
    wmove(win, 4, 21); 
    wgetnstr(win, ip_buf, 25);
    if(strlen(ip_buf) == 0) strcpy(ip, "127.0.0.1");
    else strcpy(ip, ip_buf);

    // Port 입력 이동
    wmove(win, 6, 21);
    wgetnstr(win, port_buf, 8);
    // 입력안함(초깃값)
    if(strlen(port_buf) == 0) *port = 8080;
    else *port = atoi(port_buf);
    
    // 문서 이름 입력 이동
    wmove(win, 8, 21);
    wgetnstr(win, doc_name, 25);
    // 입력 안 하면 untitled, 확장자 .txt 자동 처리 등은 여기서 로직 추가 가능
    if(strlen(doc_name) == 0) strcpy(doc_name, "untitled");

    delwin(win);
    noecho();
    curs_set(0);
}

// 실제 클라이언트 접속 로직
int connect_to_server(const char *ip, int port, const char *username, const char *doc_name, int is_host) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        return -1;
    }

    // 에디터 실행 (방장 여부 전달)
    run_network_text_editor(sock, (char*)username, is_host, (char*)doc_name);
        
    close(sock);
    return 0;
}

int main() {
    // 폴더 설정
    manage_folder();

    // ncurses 시작
    default_start();
    
    // 처음 시작
    timeout(100); 
    while(1){ int ch = getch(); if (ch != ERR) break; }
    timeout(-1);

    while(1) {

        flushinp();

        int selection = show_the_list(); // 메뉴 선택

        if (selection == 0) { 
            // 1. Server Start (Host Mode)
            char ip[30]; 
            int port;
            char doc_name[30];
            char username[20];
            char db_path[100];

            // 설정 정보 입력 (IP는 127.0.0.1, Port, 문서명)
            // 방장이므로 파일 목록(1) 보여줌
            get_connection_info(ip, &port, doc_name, 1); 

            // 해당 방의 유저 DB 경로 생성
            // 예: user_data/testdoc_userslog.txt
            snprintf(db_path, sizeof(db_path), "user_data/%s_userslog.txt", doc_name);

            // 로그인/회원가입 프로세스 실행
            // 성공해야만(1 반환) 다음으로 넘어감
            if (do_auth_process(db_path, username)) {

                // 색 설정
                start_curses(); 
                process_login_and_color_selection(doc_name, username);
                end_curses();
                
                // 로그인 성공 후 서버 실행
                pid_t pid = fork();
                if (pid == 0) {
                    // 자식: 서버
                    int log_fd = open("server_log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (log_fd > 0) {
                        dup2(log_fd, STDOUT_FILENO);
                        dup2(log_fd, STDERR_FILENO);
                        close(log_fd);
                    }
                    run_server(port, 10); 
                    exit(0);
                } else if (pid > 0) {
                    // 부모: 클라이언트(방장)
                    sleep(1); 
                    connect_to_server("127.0.0.1", port, username, doc_name, 1);
                    kill(pid, SIGTERM); // 에디터 종료 시 서버도 종료
                }
            }
            // 로그인을 취소하거나 실패하면 다시 메뉴로 돌아감

        } else if (selection == 1) {
            // 서버에 연결
            char ip[30]; 
            int port;
            char doc_name[30];
            char username[20];
            char db_path[100];
            
            // 접속 정보 입력 (방 제목을 정확히 입력해야 로그인 가능)
            get_connection_info(ip, &port, doc_name, 0);
            
            // 방 제목을 기반으로 DB 경로 설정
            snprintf(db_path, sizeof(db_path), "user_data/%s_userslog.txt", doc_name);

            // 로그인/회원가입 프로세스 실행
            if (do_auth_process(db_path, username)) {

                // 로그인 성공후 색상 선택
                start_curses();
                process_login_and_color_selection(doc_name, username);
                end_curses();

                // 로그인 성공 시 서버 접속
                if (connect_to_server(ip, port, username, doc_name, 0) < 0) {
                    end_curses();
                    printf("Connection Failed!\n");
                    getchar();
                    default_start();
                }
            }

        } else if (selection == 4) {
            // Exit
            break;
        }
    }
    
    end_curses();
    return 0;
}