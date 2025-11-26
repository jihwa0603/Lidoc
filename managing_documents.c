#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curses.h>
#include <string.h>
#include <errno.h>

#include "managing_documents.h"

Cell doc_buffer[MAX_BUFFER];
int doc_length = 0;

int check_and_create_dir(const char *dirname) {
    if (mkdir(dirname, 0755) == 0) {
        printf("폴더 생성 성공: '%s'\n", dirname);
        return 0;
    } else {
        // mkdir 실패 시
        // 폴더가 이미 존재하는 경우 (EEXIST 오류)
        if (errno == EEXIST) {
            printf("ℹ폴더가 이미 존재함: '%s'\n", dirname);
            return 1; // 이미 존재하는 것은 성공으로 간주
        } else {
            // 다른 오류 (예: 권한 문제)로 생성 실패
            perror("폴더 생성 실패");
            return -1; // 실패
        }
    }
}

int manage_folder() {
    const char *dir1 = "documents";
    const char *dir2 = "documents_with_user";
    const char *dir3 = "user_data";
    
    printf("--- 리눅스 환경에서 폴더 확인 및 생성 시작 ---\n");
    
    // 1. documents 폴더 처리
    if (check_and_create_dir(dir1) != 0) {
        fprintf(stderr, "documents 폴더 처리 중 오류 발생. 프로그램을 종료합니다.\n");
        return 1;
    }
    
    // 2. documents_with_user 폴더 처리
    if (check_and_create_dir(dir2) != 0) {
        fprintf(stderr, "documents_with_user 폴더 처리 중 오류 발생. 프로그램을 종료합니다.\n");
        return 1;
    }

    // 3. user_data 폴더 처리
    if (check_and_create_dir(dir3) != 0) {
        fprintf(stderr, "user_data 폴더 처리 중 오류 발생. 프로그램을 종료합니다.\n");
        return 1;
    }

    printf("--- 모든 폴더 처리 완료 ---\n");
    return 0;
}

void make_document(char *username, char *document_name) {
    FILE *fp = NULL;
    char path1[MAX_PATH];
    char path2[MAX_PATH];
    char path3[MAX_PATH];
    char filename[MAX_PATH];

    // 1. 파일 이름 형식: document_name_by_username
    snprintf(filename, MAX_PATH, "%s_by_%s.txt", document_name, username);
    
    printf("\n--- 문서 생성 시작 (파일명: %s) ---\n", filename);

    // 2. documents 폴더에 파일 생성
    // 경로: documents/파일명
    snprintf(path1, MAX_PATH, "documents/%s", filename);

    fp = fopen(path1, "w");
    if (fp == NULL) {
        perror("❌ [documents] 폴더에 파일 생성 실패");
    } else {
        fclose(fp);
        printf("✅ [documents] 폴더에 문서 생성 성공: '%s'\n", path1);
    }

    // 3. documents_with_user 폴더에 파일 생성
    // 경로: documents_with_user/파일명
    snprintf(path2, MAX_PATH, "documents_with_user/%s", filename);

    fp = fopen(path2, "w");
    if (fp == NULL) {
        perror("❌ [documents_with_user] 폴더에 파일 생성 실패");
    } else {
        fclose(fp);
        printf("✅ [documents_with_user] 폴더에 문서 생성 성공: '%s'\n", path2);
    }

    // 4. user_data 폴더에 파일 생성
    // 경로: user_data/파일명
    snprintf(path3, MAX_PATH, "user_data/%s", filename);
    fp = fopen(path3, "w");
    if (fp == NULL) {
        perror("❌ [user_data] 폴더에 파일 생성 실패");
    } else {
        fprintf(fp, "이 문서는 사용자 데이터 폴더에 생성되었습니다.\n");
        fprintf(fp, "사용자: %s\n", username);
        fprintf(fp, "문서 내용: %s\n", document_name);
        fclose(fp);
        printf("✅ [user_data] 폴더에 문서 생성 성공: '%s'\n", path3);
    }
}

void register_person(const char *filename, const char *username, const char *color) {
    FILE *fp = NULL;
    char path[MAX_PATH];
    
    // 경로: user_data/username_info.txt
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
    
    // 경로: user_data/username_info.txt
    snprintf(path, MAX_PATH, "user_data/%s_users.txt", filename);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("사용자 정보 파일 열기 실패");
        return 0;
    } else {
        // 1. 메모리 할당 구분 가능한 종류는 7가지 이지만 좀 더 크게 10명으로 할당
        int max_size = 10;
        Person *people = (Person*)malloc(sizeof(Person) * max_size);
        
        int i = 0;
        
        // 2. 파일 읽기
        while (fscanf(fp, "%s %s %ld", people[i].username, people[i].color, &people[i].context) != EOF) {
            i++;
            
            // (안전장치) 만약 10명이 넘으면 멈춤
            if (i >= max_size) break; 
        }

        fclose(fp);

        // 3. 함수 밖으로 "몇 명인지" 알려줌
        *count = i;

        // 4. 배열의 주소(포인터) 반환
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
    // 사용자 이름을 찾아서 색상 변경
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
    // 1. 등록된 사용자 목록에서 검색
    if (people != NULL && count > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(people[i].username, target_name) == 0) {
                // 문자열을 ncurses 색상 코드로 변환
                int color_code = get_ncurses_color_code(people[i].color);
                
                // Pair ID 매핑
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
    // 2. 목록에 없으면 기본 흰색
    return 7;
}

void load_smart_document(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "documents_with_user/%s", filename);

    FILE *fp = fopen(path, "r");
    doc_length = 0;
    
    if (fp == NULL) return;

    char current_author[MAX_NAME] = "Unknown";
    int ch;
    int state = 0; // 0: 텍스트, 1: 태그 내부
    char name_buf[MAX_NAME];
    int name_idx = 0;

    while ((ch = fgetc(fp)) != EOF && doc_length < MAX_BUFFER) {
        if (state == 0) {
            if (ch == '[') {
                state = 1; 
                name_idx = 0;
            } else {
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

void save_smart_document(const char *doc_name, Person *people, int user_count) {
    char path_plain[256];
    char path_tagged[256];
    
    // 1. 순수 텍스트 저장
    snprintf(path_plain, sizeof(path_plain), "documents/%s.txt", doc_name);
    FILE *fp_plain = fopen(path_plain, "w");
    if (fp_plain) {
        for (int i = 0; i < doc_length; i++) {
            fputc(doc_buffer[i].ch, fp_plain);
        }
        fclose(fp_plain);
    }

    // 2. 태그 포함 저장
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
            fputc(doc_buffer[i].ch, fp_tagged);
        }
        fclose(fp_tagged);
    }
    save_user_data(doc_name, people, user_count);
}

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

void delete_char(int *cursor) {
    if (*cursor == 0 || doc_length == 0) return;

    int delete_idx = *cursor - 1;
    for (int i = delete_idx; i < doc_length - 1; i++) {
        doc_buffer[i] = doc_buffer[i + 1];
    }

    doc_length--;
    (*cursor)--;
}

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

void run_text_editor(const char *username, const char *document_name) {
    // 1. 사용자 정보 로드
    int user_count = 0;
    Person *users = read_persons(document_name, &user_count);

    // 2. 문서 로드
    char actual_filename[256];
    snprintf(actual_filename, 256, "%s_by_users.txt", document_name);
    load_smart_document(actual_filename);

    int cursor_idx = doc_length;

    // --- Ncurses 초기화 ---
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    start_color();

    // 색상 팔레트 정의 (Pair ID, 글자색, 배경색)
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    init_pair(8, COLOR_BLACK, COLOR_WHITE); // 상단바

    while (1) {
        clear();

        // [상단바]
        attron(COLOR_PAIR(8));
        mvprintw(0, 0, "%-*s", COLS, " "); 
        
        int my_color_id = get_user_color_pair(username, users, user_count);
        mvprintw(0, 0, "[사용자: %s(Color %d)] (F2: 저장 / F10: 종료)", username, my_color_id);
        attroff(COLOR_PAIR(8));

        // [본문 출력]
        int screen_y = 2; 
        move(screen_y, 0); 
        
        for (int i = 0; i < doc_length; i++) {
            int color_id = get_user_color_pair(doc_buffer[i].author, users, user_count);
            
            attron(COLOR_PAIR(color_id));
            printw("%c", doc_buffer[i].ch);
            attroff(COLOR_PAIR(color_id));
        }

        // [커서 위치 설정]
        int cur_y, cur_x;
        get_screen_pos(cursor_idx, &cur_y, &cur_x);
        move(screen_y + cur_y, cur_x);
        
        refresh();

        int ch = getch();

        if (ch == KEY_F(10)) {
            break; 
        } 
        else if (ch == KEY_F(2)) {
            save_smart_document(document_name, users, user_count);
            mvprintw(LINES - 1, 0, "저장 완료!");
            getch();
        }
        else if (ch == KEY_LEFT) {
            if (cursor_idx > 0) cursor_idx--;
        }
        else if (ch == KEY_RIGHT) {
            if (cursor_idx < doc_length) cursor_idx++;
        }
        else if (ch == KEY_UP) {
            if (cursor_idx >= COLS) cursor_idx -= COLS;
            else cursor_idx = 0;
        }
        else if (ch == KEY_DOWN) {
            if (cursor_idx + COLS < doc_length) cursor_idx += COLS;
            else cursor_idx = doc_length;
        }
        else if (ch == KEY_BACKSPACE || ch == 127) {
            delete_char(&cursor_idx);
        }
        else if (ch == '\n' || ch == KEY_ENTER) {
            insert_char(&cursor_idx, '\n', username);
        }
        else {
            insert_char(&cursor_idx, ch, username);
        }
    }
    
    if (users != NULL) free(users);
    endwin();
}