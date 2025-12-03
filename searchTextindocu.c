#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include "managing_documents.h"
#define LINES 30
#define COLS 120
#define MAX_RESULTS 100

typedef struct {
    int line_num;
    int col_num;
    char line_info[1024];
} SearchFindInfo;

SearchFindInfo result_array[100];
int result_count=0;
WINDOW *question_win=NULL;
int search_text_in_file(const char *filename, const char *search_text);
void display_result(char* search_text,const char *filename);
//박스 만들기
void create_question_box(const char *filename){
    int height=7, width=50;
    int starty=(LINES-height)/2;
    int startx=(COLS-width)/2;

    question_win=newwin(height, width, starty, startx);
    wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(question_win,1,2, "Search Text");
    mvwprintw(question_win,3,2, "Enter text to search: ");
    wrefresh(question_win);

    char search_text[100];
    echo();
    mvwgetnstr(question_win,3,24,search_text,sizeof(search_text)-1);
    noecho();
    int found = search_text_in_file(filename,search_text);
    if (!found) {
        mvwprintw(question_win,4,2,"'%s' not found.",search_text);
        wrefresh(question_win);
        wgetch(question_win);
    } else {
        delwin(question_win);
        question_win=NULL;
        display_result(search_text,filename);
        return;
    }
    delwin(question_win);
    question_win=NULL;
}

//파일에서 특정 문자 찾기
int search_text_in_file(const char *filename, const char *search_text) {
    char path[MAX_PATH];
    snprintf(path,sizeof(path),"documents/%s",filename);
    FILE *fp=fopen(path,"r");
    if (fp==NULL) {
        perror("파일 열기 실패");
        return 0;
    }
    char line[1024];
    int line_number = 0;
    int result_index = 0;
    int found = 0;
    size_t search_len = strlen(search_text);//찾는 문자 길이 저장
    if (search_len==0) {
        fclose(fp);
        result_count = 0;
        mvwprintw(question_win,4,2, "I'm Sorry, Please more than length of 1");
        return 0;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        char *newline = strchr(line, '\n'); //\n 개행문자 지우기
        if (newline) *newline = '\0';//\0로 덮어 쓰기
        char *search_ptr = line;
        char *found_ptr;

        while ((found_ptr = strstr(search_ptr, search_text)) != NULL) {
            if (result_index >= MAX_RESULTS) {
                fprintf(stderr, "Too many results in documents!\n");
                break;
            }
            //찾은 결과물을 줄 수, 문장 내 위치, 전체 줄의 내용 순으로 구조체 배열에 하나씩 담기
            result_array[result_index].line_num = line_number;
            result_array[result_index].col_num = (int)(found_ptr - line + 1);
            strncpy(result_array[result_index].line_info, line, sizeof(result_array[result_index].line_info) - 1);
            result_array[result_index].line_info[sizeof(result_array[result_index].line_info) - 1] = '\0';
            result_index++;
            found = 1;
            search_ptr=found_ptr+search_len;
        }
        //MAX 개수 넘어가면 종료
        if (result_index>=MAX_RESULTS) break;
    }

    fclose(fp);
    result_count=result_index;
    return found;
}
//결과 출력 함수
void display_result(char* search_text,const char *filename){
    int index=0;
    keypad(stdscr,TRUE);
    noecho();
    //에러 처리
    if (result_count==0) {
        mvprintw(0,0,"No results to display.");
        refresh();
        mvprintw(1,0,"Press any key to quit.");
        getch();
        return;
    }
    //화면 출력 로직 
    while (1) {
        clear();
        mvprintw(1,2,"File: %s    Match %d/%d    (->: next, <-: prev, q: quit)", filename, index+1, result_count);

        SearchFindInfo temp=result_array[index];
        int ln=temp.line_num;
        int col=temp.col_num-1; 
        char display_line[1024];
        strncpy(display_line,temp.line_info,sizeof(display_line)-1);
        display_line[sizeof(display_line)-1] = '\0';

        mvprintw(3,2,"%4d: ",ln);
        //앞 부분 그냥 출력하기
        if (col>0) {
            char prefix[1024];
            int n = (col<(int)sizeof(prefix)-1) ? col : (int)sizeof(prefix)-1;
            strncpy(prefix,display_line,n);
            prefix[n] = '\0';
            mvprintw(3,8,"%s",prefix);
        }
        //중간 부분 색 반전
        attron(A_REVERSE);
        mvprintw(3,8+col,"%s",search_text);
        attroff(A_REVERSE);
        //뒷 부분 그냥 출력
        int rem_start=col+(int)strlen(search_text);
        if (rem_start<(int)strlen(display_line)) {
            mvprintw(3,8+rem_start,"%s",display_line+rem_start);
        }

        refresh();
        //key 입력 처리
        int ch = getch();
        if (ch == KEY_RIGHT) {
            index = (index + 1) % result_count;
        } else if (ch == KEY_LEFT) {
            index = (index - 1) % result_count;
        } else if (ch == 'q' || ch == 'Q') {
            break;
        }
    }
}

