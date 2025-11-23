#ifndef MANAGING_DOCUMENTS_H
#define MANAGING_DOCUMENTS_H

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define WHITE "\x1b[37m"
#define BLACK "\x1b[30m"
#define PINK "\x1b[95m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define UNDERLINE "\x1b[4m"

#define MAX_PATH_LENGTH 512
#define MAX_PATH 1024

int check_and_create_dir(const char *dirname);
int manage_folder();

#endif // MANAGING_DOCUMENTS_H