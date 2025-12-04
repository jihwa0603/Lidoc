#ifndef LOGIN_H
#define LOGIN_H

// DB 경로와 성공 시 ID를 담을 버퍼를 인자로 받음
int do_auth_process(const char *db_path, char *username_out);
int network_login_process(int sock, char *username_out);
int ask_auth_menu();

#endif