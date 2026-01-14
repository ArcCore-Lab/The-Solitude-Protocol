/*
    Static file server ready!
*/

#include "include/unp.h"
#include "lib/tsp.h"

int main(int agrc, char **argv) {
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

    signal(SIGPIPE, SIG_IGN);

    listen(listenfd, LISTENQ);

    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (SA *) &cliaddr, &clilen);

        if ( (childpid = fork()) == 0 ) {
            close(listenfd);
            response(connfd);
            exit(0);
        }

        close(connfd);
    }
}