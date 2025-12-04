#ifndef SEARCH_TEXT_IN_DOCU_H
#define SEARCH_TEXT_IN_DOCU_H
#include <ncurses.h>

void create_question_box(const char *filename);
int search_text_in_file(const char *filename, const char *search_text);
void display_result(char* search_text, const char *filename);

#endif