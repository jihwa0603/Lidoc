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

#include "login.h"              // 1단계에서 만듦
#include "server.h"             // run_server 등
#include "managing_documents.h" // run_network_text_editor 등

void start_curses() {
    setlocale(LC_ALL, "");
    initscr();              // 화면표기
    cbreak();               // 라인 버퍼링 비활성화
    noecho();               // 입력 문자 화면에 표시 안함
    keypad(stdscr, TRUE);   // 펑션 키와 화살표 키 사용 가능
    curs_set(0);           // 커서 숨기기
}

void end_curses() {
    endwin();               // Curses 모드 종료
}

void default_start(){
    start_curses();
    box(stdscr, 0, 0); // 테두리 그리기

    int height, width;
    getmaxyx(stdscr, height, width); // 사이즈 얻기
    mvprintw(height / 2, (width - 20) / 2, "Welcome to the App!"); // 중간에 메세지 표기
    mvprintw((height) - 2, (width - 16), "from Team Lidoc"); // 팀명 표시
    mvprintw(height - 2, 2, "Press any key to continue...");
    refresh();
}

int show_the_list(){
    clear();
    box(stdscr, 0, 0);

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
                current_selection = 4; // 화살표를 마지막 항목으로 이동
            }
        } else if(ch == KEY_DOWN){
            current_selection++;
            if(current_selection > 4){
                current_selection = 0; // 화살표를 첫 항목으로 이동
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

// IP와 포트를 입력받는 간단한 UI 함수
// IP, Port, 문서 이름을 입력받는 UI 함수 (파일 목록 기능 추가)
void get_connection_info(char *ip, int *port, char *doc_name, int show_file_list) {
    echo();
    curs_set(1);
    clear();

    // 창 크기를 좀 더 키움 (높이 20, 너비 60)
    int win_h = 20;
    int win_w = 60;
    int start_y = LINES / 2 - win_h / 2;
    int start_x = COLS / 2 - win_w / 2;

    WINDOW *win = newwin(win_h, win_w, start_y, start_x);
    box(win, 0, 0);

    // 타이틀
    mvwprintw(win, 1, 2, "=== Connection Setup ===");
    mvwhline(win, 2, 1, ACS_HLINE, win_w - 2); // 구분선

    // 1. 입력 라벨 표시 (위치 정렬)
    mvwprintw(win, 4, 4, "Server IP     : [                          ]"); 
    mvwprintw(win, 6, 4, "Port          : [        ]");
    mvwprintw(win, 8, 4, "Document Name : [                          ]");

    // 2. 파일 목록 보여주기 (show_file_list가 1일 때만)
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
            mvwprintw(win, 13, 4, "(No 'documents' directory found)");
        }
    } else {
        // 게스트일 경우 도움말 표시
        mvwprintw(win, 12, 4, "Enter the Host IP to connect.");
    }

    wrefresh(win);

    // 3. 실제 입력 받기
    char ip_buf[30] = {0};
    char port_buf[10] = {0};

    // IP 입력 이동
    wmove(win, 4, 21); 
    wgetnstr(win, ip_buf, 25);
    if(strlen(ip_buf) == 0) strcpy(ip, "127.0.0.1");
    else strcpy(ip, ip_buf);

    // Port 입력 이동
    wmove(win, 6, 21);
    wgetnstr(win, port_buf, 8);
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
    // 1. 폴더 초기화
    manage_folder();

    // 2. 로그인 프로세스
    char username[20];
    // login.c의 로직을 통해 username을 채워와야 함
    // 임시로 하드코딩된 예시: (실제로는 do_login_process 호출)
    // if (!do_login_process(username)) return 0; 
    strcpy(username, "TestUser"); // 테스트용

    // 3. 메인 UI 시작
    default_start();
    
    timeout(100); 
    while(1){
        int ch = getch(); 
        if (ch != ERR) break; 
    }
    timeout(-1); // 다시 블로킹 모드로

    while(1) {
        int selection = show_the_list(); // 메뉴 선택

        if (selection == 0) { 
            // 1. Start Server (Host Mode)
            char ip[30]; 
            int port;
            char doc_name[30];
            
            get_connection_info(ip, &port, doc_name, 1);
            
            // 서버는 백그라운드 프로세스(fork)로 실행해야 UI가 멈추지 않음
            pid_t pid = fork();
            if (pid == 0) {
                int log_fd = open("server_log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (log_fd > 0) {
                    dup2(log_fd, STDOUT_FILENO); // printf -> 파일
                    dup2(log_fd, STDERR_FILENO); // perror -> 파일
                    close(log_fd);
                }
                
                run_server(port, 10); 
                exit(0);
            } else if (pid > 0) {
                // [부모 프로세스: 클라이언트]
                sleep(1); // 서버 켜질 때까지 잠시 대기
                // is_host = 1
                connect_to_server("127.0.0.1", port, username, doc_name, 1);
                
                // 에디터 끝나면 서버도 종료 (선택 사항)
                kill(pid, SIGTERM);
            }

        } else if (selection == 1) {
            // 2. Connect to Server (Guest Mode)
            char ip[30]; 
            int port;
            char doc_name[30]; // 접속할 때는 필요 없을 수도 있지만, 일단 입력
            
            get_connection_info(ip, &port, doc_name,0);
            
            // Guest 모드로 접속 (is_host = 0)
            if (connect_to_server(ip, port, username, doc_name, 0) < 0) {
                end_curses(); // 에러 메시지 잘 보이게 잠시 종료
                printf("Connection Failed!\n");
                getchar(); // 엔터 대기
                default_start(); // 다시 시작
            }

        } else if (selection == 4) {
            // Exit
            break;
        }
    }
    
    end_curses();
    return 0;
}