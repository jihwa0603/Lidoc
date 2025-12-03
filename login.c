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

#define MAX_USERNAME_LEN 15
#define MAX_PASSWORD_LEN 15


int WIDTH,HEIGHT;
int loginFlag=0;
char logged_in_user[50];

typedef struct{
    char id[15];
    unsigned long password;
}User_Process;

unsigned long hash_password(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

void draw_border(){
    clear();
    getmaxyx(stdscr,HEIGHT,WIDTH);
    for(int i=0;i<WIDTH-1;i++){
        mvaddch(0,i,'-');
        mvaddch(HEIGHT-2,i,'-');
    }
   
     for(int i=0;i<HEIGHT-1;i++){
        mvaddch(i,0,'|');
        mvaddch(i,WIDTH-1,'|');
    }
    refresh();
}
void init_screen(){
    setlocale(LC_ALL, "");
    initscr();
    curs_set(0);
}

void get_pw(char *buffer, int y, int x) {
    int i=0;
    int ch;
    move(y,x);
    while (1) {
        ch = getch();
        fflush(stdin);
        if (ch == '\n' || ch == '\r') { 
            buffer[i] = '\0';
            break;
        } else if (ch == 127 || ch == 8) { 
            if (i > 0) {
                i--;
                mvaddch(y, x + i, ' '); 
                move(y, x + i);
            }
        } else if (i < MAX_PASSWORD_LEN) {
            buffer[i++] = ch;
            addch('*'); 
        }
        refresh();
    }
}


void RegisterForm(){
    char *buffer=malloc(MAX_PASSWORD_LEN+1);
    clear();
    draw_border();
    refresh();
    User_Process t1;
    mvprintw(HEIGHT/2-2,WIDTH/2-10, "=== REGISTER ===");
    int fid;
    mvprintw(HEIGHT/2,WIDTH/2,"Enter your Id:");
    refresh();
    echo(); 
    mvgetstr(HEIGHT/2 , WIDTH/2+14, t1.id);
    noecho();
    mvprintw(HEIGHT/2+1,WIDTH/2,"Enter your password:");
    get_pw(buffer,HEIGHT/2+1,WIDTH/2+22);
    echo();
    char buf[100];
    unsigned long hashed_pw=hash_password(buffer);
    sprintf(buf,"%s %lu\n", t1.id,hashed_pw);
    if((fid=open("UserLog.txt",O_WRONLY|O_CREAT|O_APPEND,0700))==-1){
        perror("Error!");
        exit(1);
    }
    if (write(fid, buf, strlen(buf)) == -1) {
        endwin();
        perror("Error writing");
        close(fid);
        exit(1);
    }
    
    close(fid);
    mvprintw(HEIGHT/2+3, (WIDTH-25)/2, "Save Complete. Press any key.");
    getch();
    refresh();
}




void loginForm(char *result_id){
    clear();
    draw_border();
    refresh();
    
    char *buffer = malloc(MAX_PASSWORD_LEN+1);
    User_Process t1;
    User_Process che;
    // struct stat fst; // 안 쓰면 삭제
    // int fid;         // 안 쓰면 삭제

    mvprintw(HEIGHT/2-2,WIDTH/2-10, "=== LOGIN ===");
    mvprintw(HEIGHT/2,WIDTH/2,"Enter your Id:");
    refresh();
    
    echo(); 
    mvgetstr(HEIGHT/2 , WIDTH/2+14, t1.id);
    noecho();
    
    mvprintw(HEIGHT/2+1,WIDTH/2,"Enter your password:");
    get_pw(buffer,HEIGHT/2+1,WIDTH/2+22);
    
    unsigned long temp_pw = hash_password(buffer);
    FILE* in = fopen("UserLog.txt","r");
    
    // 파일 없을 때 예외처리 추가 추천
    if(in == NULL) {
        mvprintw(HEIGHT/2+3, (WIDTH-25)/2, "No User Database!");
        getch();
        free(buffer);
        return;
    }

    loginFlag = 0; // 초기화
    while(fscanf(in,"%s %lu", che.id, &che.password) != -1){
        if (strcmp(che.id, t1.id) == 0 && temp_pw == che.password) {
            loginFlag = 1;
            break;
        }
    }
    fclose(in);
    
    if(loginFlag){
        mvprintw(HEIGHT/2+3,(WIDTH-25)/2, "Login Successful!");
        // ★ 성공 시 입력한 ID를 결과 변수에 복사
        strcpy(result_id, t1.id);
        refresh();
    }
    else{
        mvprintw(HEIGHT/2+3,(WIDTH-25)/2, "Login Failed! Retry");
        refresh();
    }
    
    getch();
    free(buffer); // 메모리 해제
}

int do_login_process(char *username_out) {
    setlocale(LC_ALL, "");
    init_screen();
    draw_border();
    
    // loginForm 호출 시 username_out 주소를 넘겨줌
    loginForm(username_out);
    
    if (loginFlag) {
        return 1; // 성공
    }
    return 0; // 실패
}