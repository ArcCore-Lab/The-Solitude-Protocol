/*
    Maybe HTTP/1.1 Server — then you can finally test it the normal way!
*/

#include "include/unp.h"

void echo_hello(int sockfd) {
    const char response[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n\r\n"
        "Hello World!\n";

    write(sockfd, response, strlen(response));
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (SA *)&cliaddr, &clilen);
        if ( (childpid = fork()) == 0) {
            close(listenfd);
            echo_hello(connfd);
            exit(0);
        }
        close(connfd);
    }
}