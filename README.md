<div align="center">
    <a href="https://github.com/jihwa0603/Lidoc">
    <img src="https://img.freepik.com/free-vector/files-blue-colour_78370-6661.jpg?semt=ais_user_personalization&w=740&q=80" alt="Logo" width="80" height="80">
    </a>
    <h3>실시간 문서 관리 프로그램</h3>
    <p>by team Lidoc</p>
</div>

![Version](https://img.shields.io/badge/version-1.0.0-blue) ![License](https://img.shields.io/badge/license-MIT-green)

## 💡 프로젝트 간단 소개

현대 사회에 필수적인 협업을 돕고자 개발한 문서 관리 프로그램입니다.
파일 관리자가 권한을 주어 원하는 사람만 편집이 가능하게 합니다.

## 🔗 구현한 기능들

* ✅ **기능 1**: <a href="#together">소켓을 통한 웹 기반 통신</a>
* ✅ **기능 2**: <a href="#search">원하는 단어 검색 기능 </a>
* ✅ **기능 3**: <a href="#login">로그인을 통한 사용자 구별</a>
* ✅ **기능 4**: <a href="#save">주기적 자동저장 기능</a>
* ✅ **기능 5**: <a href="#count">사용자 구분 및 기여도 관리</a>

<br>

#
<a id="together"></a>

- 소켓을 통한 웹 기반 통신 

<img src="images/색 선택.png"><img src="images/함께 작성.png">
<p>
처음 문서에 작성을 할 경우 먼저 색을 선택을 하고 작성을 합니다. <br>
이미지처럼 사용자 별로 색을 구분하여 문서를 작성합니다.
</p>

<br>

# 
<a id="search"></a>

- 원하는 단어 검색 기능

<img src="images/검색.png"><img src="images/검색찾기.png">
<p>
문서 내부에서 읽기 상태에서 f키를 눌러 단어를 검색하고 문장을 찾아 줍니다.
</p>

<br>
<a id="login"></a>

#
- 로그인을 통한 사용자 구별

<img src="images/로그인_이미지.png">

<p>로그인 기능이 있으며 회원가입 유저의 경우 방장의 폴더에 파일로 저장됩니다. <br>
물론, 비밀번호의 경우 해쉬를 이용하여 암호화 되어 있기에 다른 유저의 비밀번호를 알 수 없습니다.<br>
처음 등록을 할 때 아이디가 겹칠 경우 등록을 할 수 없습니다.<br>(추가로 처음 방장이 문서를 열고 후에 다른 유저가 접속할 때 서버와 문서명을 똑같이 입력해야 합니다.)
</p>

<br>

#
<a id="save"></a>

- 주기적 자동저장 기능

<img src="images/사용자문서.png">
<img src="images/문서 저장.png">
<p>
누군가가 작성 상태에서 작성을 하고 읽기 모드로 전환을 할 때마다 문서를 저장합니다. <br>
폴더 2개를 사용하여 1곳에는 작성자와 문서 내용을, 1곳에는 문서 내용만을 저장합니다.
</p>

<br>

#
<a id="count"></a>

- 사용자 구분 및 기여도 관리

<img src="images/기여도.png">
<p>
폴더 "user_data"의 내부에 로그인 파일과 유저별 색상 및 작성 글자 수를 측정하여 저장합니다.
</p>

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

| [<img src="https://github.com/jihwa0603.png" width="200">](https://github.com/jihwa0603) | [<img src="https://github.com/hellowo1.png" width="200">](https://github.com/hellowo1) |
| :--------------------------------------------------------------------------------------: | :------------------------------------------------------------------------------------: | 
|                        **[홍지환](https://github.com/jihwa0603)**                        |                       **[이제민](https://github.com/hellowo1)**                        |
|                   **22 심컴**                                                            |                        **22 심컴**                                                     |
|서버 및 다중이용자 <br>사용 문서 처리 담당 총괄PM                                                |     로그인 및 기타 기능 처리                                                             |
