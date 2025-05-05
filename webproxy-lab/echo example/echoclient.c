#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); // 서버에 연결
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) { // 사용자 입력 받기
        Rio_writen(clientfd, buf, strlen(buf)); // 서버로 보내기
        Rio_readlineb(&rio, buf, MAXLINE); // 서버로부터 응답 받기
        Fputs(buf, stdout); // 화면에 출력
    }

    Close(clientfd); // 연결 종료
    exit(0);
}
