#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

#define MAX_USERNAME_LEN 15
#define MAX_PASSWORD_LEN 15
int WIDTH,HEIGHT;
int loginFlag=0;

typedef struct{
    char id[15];
    char password[15];
}User_Process;

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
    initscr();
    curs_set(0);
}

void RegisterForm(){
    clear();
    draw_border();
    refresh();
    User_Process t1;
    int fid;
    mvprintw(HEIGHT/2,WIDTH/2,"Enter your Id:");
    refresh();
    echo(); 
    mvgetstr(HEIGHT/2 , WIDTH/2+14, t1.id);
    noecho();
    mvprintw(HEIGHT/2+1,WIDTH/2,"Enter your password:");
    mvgetstr(HEIGHT/2+1 , WIDTH/2+22, t1.password);
    char buf[100];
    sprintf(buf,"User: %s\n", t1.id);
    if((fid=open("UserLog.txt",O_WRONLY|O_CREAT|O_APPEND,0700))==-1){
        perror("Error!");
        exit(1);
    }
    if (write(fid, buf, strlen(buf)) == -1) {
        endwin();
        perror("Error writing ID");
        close(fid);
        exit(1);
    }

    sprintf(buf, "Password: %s\n", t1.password);
    if (write(fid, buf, strlen(buf)) == -1) {
        endwin();
        perror("Error writing password");
        close(fid);
        exit(1);
    }
    
    close(fid);
    mvprintw(HEIGHT/2+3, (WIDTH-25)/2, "Save Complete. Press any key.");
    refresh();
}




void loginForm(){
    clear();
    draw_border();
    refresh();

    User_Process t1;
    struct stat fst;
    int fid;
    mvprintw(HEIGHT/2-1,WIDTH/2,"This is loginForm");
    mvprintw(HEIGHT/2,WIDTH/2,"Enter your Id:");
    refresh();
    echo(); 
    mvgetstr(HEIGHT/2 , WIDTH/2+14, t1.id);
    noecho();
    mvprintw(HEIGHT/2+1,WIDTH/2,"Enter your password:");
    mvgetstr(HEIGHT/2+1 , WIDTH/2+22, t1.password);
    if((fid=open("UserLog.txt",O_RDONLY))==-1){
        perror("Error!");
        exit(1);
    }
    if(fstat(fid,&fst)==-1){
        perror("Error!");
        exit(1);
    }
    int lengFile=fst.st_size;
    char* longBuffer=malloc(lengFile+1);
    int readLength;
    if((readLength=read(fid,longBuffer,lengFile))!=lengFile){
        perror("Error!");
        free(longBuffer);
        exit(1);
    }
    char* temp=strtok(longBuffer,"\n");
    while(temp!=NULL){
        char* user,*passwd,*dummy;
        dummy = strtok(temp, ":");    
        user = strtok(NULL, ":");
        temp = strtok(NULL, "\n");
        dummy = strtok(temp, ":");  
        passwd=strtok(NULL,":");
        if(user!=NULL && passwd!=NULL){
            // printf("%s,%s",user,passwd);
            if(strcmp(user,t1.id)==0&&strcmp(passwd,t1.password)==0){
                loginFlag=1;
                break;
            }
        }
       
        temp=strtok(NULL,"\n");
    }
    close(fid);
    free(longBuffer);
    
    refresh();
    if(loginFlag==1){
        mvprintw(2,5,"Login Sucessful!");
    }
    refresh();
}

int main(){
    
    init_screen();
    draw_border();
    signal(SIGINT,loginForm);
    signal(SIGTSTP,RegisterForm);
    while(1){

    }


}