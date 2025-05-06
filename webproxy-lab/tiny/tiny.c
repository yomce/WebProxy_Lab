/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 요청 라인 읽기
    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    // GET 메소드만 지원
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    // 요청 헤더 읽고 무시
    read_requesthdrs(&rio);

    // URI 파싱
    is_static = parse_uri(uri, filename, cgiargs);

    // 파일 상태 체크
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    }

    if (is_static) {  // 정적 컨텐츠
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else {  // 동적 컨텐츠
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) { // 정적 컨텐츠
        strcpy(cgiargs, "");        // CGI 인자 없음
        strcpy(filename, ".");      // 현재 디렉토리 기준
        strcat(filename, uri);      // 파일 경로 완성
        if (uri[strlen(uri) - 1] == '/') // uri가 디렉토리면
            strcat(filename, "home.html"); // 기본 파일로 설정
        return 1;
    }
    else { // 동적 컨텐츠
        ptr = index(uri, '?');
        if (ptr) {
            *ptr = '\0'; // '?'를 널 문자로 끊어줌
            strcpy(cgiargs, ptr + 1); // 그 다음은 CGI 인자
        }
        else {
            strcpy(cgiargs, ""); // 인자 없으면 빈 문자열
        }
        strcpy(filename, "."); // 현재 디렉토리 기준
        strcat(filename, uri); // 파일 경로 완성
        return 0;
    }
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    // 클라이언트한테 응답 헤더 보내기
    get_filetype(filename, filetype); // 파일 타입 결정
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
    sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
    rio_writen(fd, buf, strlen(buf));

    // 클라이언트한테 실제 파일 보내기
    srcfd = Open(filename, O_RDONLY, 0);               // 파일 열기
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 매핑
    Close(srcfd);                                       // 파일 디스크립터는 바로 닫기
    rio_writen(fd, srcp, filesize);                     // 매핑한 메모리를 클라이언트로 전송
    Munmap(srcp, filesize);                             // 메모리 매핑 해제
}

void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    // HTTP 응답 헤더 일부만 먼저 보내기
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { // 자식 프로세스
        // CGI 프로그램이 결과를 바로 클라이언트로 보내게 fd를 stdout에 복제
        setenv("QUERY_STRING", cgiargs, 1); // 환경변수 설정
        Dup2(fd, STDOUT_FILENO);             // 클라이언트로 바로 출력
        Execve(filename, emptylist, environ); // CGI 프로그램 실행
    }
    Wait(NULL); // 부모 프로세스: 자식 종료 기다림
}
