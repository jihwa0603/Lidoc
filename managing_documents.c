#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curses.h>
#include <string.h>
#include <errno.h> // errno 정의를 위해 필요

#include "managing_documents.h"


// 폴더를 확인하고, 없으면 생성하는 함수 (Linux/POSIX 전용)
int check_and_create_dir(const char *dirname) {
    // mkdir(경로, 권한) 함수 사용. 0755는 소유자(읽기/쓰기/실행), 그룹/기타(읽기/실행) 권한을 의미
    if (mkdir(dirname, 0755) == 0) {
        printf("✅ 폴더 생성 성공: '%s'\n", dirname);
        return 0;
    } else {
        // mkdir 실패 시
        // 폴더가 이미 존재하는 경우 (EEXIST 오류)
        if (errno == EEXIST) {
            printf("ℹ️ 폴더가 이미 존재함: '%s'\n", dirname);
            return 0; // 이미 존재하는 것은 성공으로 간주
        } else {
            // 다른 오류 (예: 권한 문제)로 생성 실패
            perror("❌ 폴더 생성 실패");
            return -1; // 실패
        }
    }
}

int manage_folder() {
    const char *dir1 = "documents";
    const char *dir2 = "documents_with_user";
    
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

    printf("--- 모든 폴더 처리 완료 ---\n");
    return 0;
}

void make_document(char *username, char *document_name) {
    FILE *fp = NULL;
    char path1[MAX_PATH];
    char path2[MAX_PATH];
    char filename[MAX_PATH];

    // 1. 파일 이름 형식: document_name_by_username
    // 예: report_2025_by_user1
    snprintf(filename, MAX_PATH, "%s_by_%s.txt", document_name, username);
    
    printf("\n--- 문서 생성 시작 (파일명: %s) ---\n", filename);

    // 2. documents 폴더에 파일 생성
    // 경로: documents/파일명
    snprintf(path1, MAX_PATH, "documents/%s", filename);

    fp = fopen(path1, "w"); // 'w' 모드: 쓰기 전용 (파일이 없으면 생성, 있으면 덮어씀)
    if (fp == NULL) {
        perror("❌ [documents] 폴더에 파일 생성 실패");
    } else {
        fprintf(fp, "이 문서는 일반 문서 폴더에 생성되었습니다.\n");
        fprintf(fp, "문서 내용: %s\n", document_name);
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
        fprintf(fp, "이 문서는 사용자 관련 문서 폴더에 생성되었습니다.\n");
        fprintf(fp, "사용자: %s\n", username);
        fprintf(fp, "문서 내용: %s\n", document_name);
        fclose(fp);
        printf("✅ [documents_with_user] 폴더에 문서 생성 성공: '%s'\n", path2);
    }
}