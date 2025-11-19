#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h> // errno 정의를 위해 필요

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