#ifndef MANAGING_DOCUMENTS_H
#define MANAGING_DOCUMENTS_H

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"

#define RESET "\x1b[0m"

#define MAX_PATH_LENGTH 512
#define MAX_PATH 1024

#define MAX_BUFFER 4096  // 문서 최대 크기 (약 4KB)
#define MAX_NAME 30      // 이름 최대 길이

typedef struct {
    char username[MAX_NAME];
    char color[20];
    long context;
} Person;

// [핵심 구조체] 문자 하나와 그 문자를 쓴 사람을 함께 저장
typedef struct {
    char ch;              // 실제 글자
    char author[MAX_NAME]; // 작성자 이름
} Cell;

int check_and_create_dir(const char *dirname);
int manage_folder();
void make_document(char *username, char *document_name);
void register_person(const char *filename, const char *username, const char *color);
Person* read_persons(const char *filename, int *count);
void change_color(const char* filename, const char* user_name, const char *new_color);
void save_user_data(const char *filename, Person *people, int count);
int get_ncurses_color_code(const char *color_str);
int get_user_color_pair(const char *target_name, Person *people, int count);
void load_smart_document(const char *filename);
void save_smart_document(const char *doc_name, Person *people, int user_count);
void insert_char(int *cursor, char ch, const char *username);
void delete_char(int *cursor);
void get_screen_pos(int target_idx, int *y, int *x);
void run_text_editor(const char *username, const char *document_name);

#endif // MANAGING_DOCUMENTS_H