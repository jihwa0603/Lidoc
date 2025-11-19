#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

void start_curses() {
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

int main() {
    default_start();
    
    timeout(100); // 논블로킹 입력 대기 시간 설정 (100ms)

    while(1){
        int ch = getch(); // 입력대기
        if (ch != ERR) {
            break; // 키 입력이 있으면 루프 종료
        }
    }
    show_the_list();

    end_curses();
    return 0;
}