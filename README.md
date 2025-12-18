# Lidoc-실시간 문서 관리 프로그램

![Version](https://img.shields.io/badge/version-1.0.0-blue) ![License](https://img.shields.io/badge/license-MIT-green)

## 💡 프로젝트 간단 소개

현대 사회에 필수적인 협업을 돕고자 개발한 문서 관리 프로그램입니다.
파일 관리자가 권한을 주어 원하는 사람만 편집이 가능하게 합니다.

## 🔗 구현한 기능들

* ✅ **기능 1**: 소켓을 통한 웹 기반 통신
* ✅ **기능 2**: 원하는 단어 검색 기능
* ✅ **기능 3**: 로그인을 통한 사용자 구별
* ✅ **기능 4**: 주기적 자동저장 기능
* ✅ **기능 5**: 사용자 구분 및 기여도 관리


## 🎥 데모 영상

https://youtu.be/dbA3HOskZ-0


## 💻 개발 환경
- **OS**: Ubuntu 18.04 LTS
- **Compiler**: GCC 
- **Build Tool**: Make

## 🔧 빌드 및 실행 방법
### ⚙️ 시스템 요구 사항
- OS: Linux(Ubuntu 18.04 혹은 기타 Linux 배포판)
- 컴파일러: GCC 

### 📦 라이브러리 설치 
실행을 위해 ncurses 라이브러리 설치가 필요합니다.
```bash
sudo apt-get update
sudo apt-get install libncurses5-dev libncursesw5-dev
```
### 🛠️ 컴파일
- Makefile 활용한 컴파일
```bash
make
```
- GCC 사용한 컴파일
```bash
gcc -o lidoc start.c login.c server.c managing_documents.c searchTextindocu.c -lncursesw -lpthread
```
### 🚀 실행 
```bash
./lidoc
```


## 👦 팀원 정보

- 홍지환: 서버 및 다중 이용자 문서 처리 
- 이제민: 로그인 및 기타 기능 처리
