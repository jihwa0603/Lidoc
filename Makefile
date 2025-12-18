# 컴파일러 설정
CC = gcc
# 컴파일 옵션 (-Wall: 경고 표시, -g: 디버깅 정보 포함)
CFLAGS = -Wall -g
# 라이브러리 연결 (-lncurses: UI 라이브러리, -lpthread: 스레드 라이브러리)
LDFLAGS = -lncurses -lpthread

# 실행 파일 이름
TARGET = lidoc

# 소스 파일 목록
SRCS = start.c server.c managing_documents.c login.c searchTextindocu.c

# 오브젝트 파일 목록 (소스 파일의 .c를 .o로 변환)
OBJS = $(SRCS:.c=.o)

# 기본 타겟 (make 명령어 입력 시 실행됨)
all: $(TARGET)

# 실행 파일 생성 규칙
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# .c 파일을 .o 파일로 컴파일하는 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 정리 타겟 (make clean 입력 시 실행됨)
clean:
	rm -f $(OBJS) $(TARGET)