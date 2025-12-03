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
            mvprintw(height - 4, 2, "You selected: %s", items[current_selection]);
            break;
        }

        // 바뀐 위치에 화살표 그리기
        mvprintw((height / 2) - 2 + current_selection, (width - strlen(items[current_selection])) / 2 - 4, "->");
        refresh();
    }

    return current_selection;
}

// IP와 포트를 입력받는 간단한 UI 함수
void get_connection_info(char *ip, int *port, char *doc_name) {
    echo();
    curs_set(1);
    WINDOW *win = newwin(10, 50, LINES/2 - 5, COLS/2 - 25);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "Server IP (default 127.0.0.1): ");
    mvwprintw(win, 3, 2, "Port (default 8080): ");
    mvwprintw(win, 5, 2, "Document Name: ");
    wrefresh(win);

    char ip_buf[30] = {0};
    char port_buf[10] = {0};

    mvwgetnstr(win, 1, 28, ip_buf, 29);
    if(strlen(ip_buf) == 0) strcpy(ip, "127.0.0.1");
    else strcpy(ip, ip_buf);

    mvwgetnstr(win, 3, 23, port_buf, 9);
    if(strlen(port_buf) == 0) *port = 8080;
    else *port = atoi(port_buf);
    
    mvwgetnstr(win, 5, 17, doc_name, 29);
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
            
            get_connection_info(ip, &port, doc_name);
            
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
            
            get_connection_info(ip, &port, doc_name);
            
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