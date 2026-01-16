/*
    Upgraded concurrent server using `select()`, paired with `tcpcli.c`
*/

#include "include/unp.h"
#include "lib/tsp.h"

int main(int argc, char **argv) {
    int i, maxi, maxfd, nready, sockfd, connfd, listenfd;
    ssize_t n;
    socklen_t clilen;
    fd_set rset, allset;
    char buf[MAXLINE];
    int client[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    maxfd = listenfd;
    maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) {
        client[i] = -1;
    }

    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    for (;;) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset)) {
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (SA *) &servaddr, &clilen);

            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = connfd;
                    break;
                }
            }

            if (i == FD_SETSIZE) err_quit("too many clients");

            FD_SET(connfd, &allset);
            if (connfd > maxfd) maxfd = connfd;

            if (i > maxi) maxi = i;

            if (--nready <= 0) continue;
        }

        for (i = 0; i <= maxi; i++) {
            if ( (sockfd = client[1]) < 0 ) continue;

            if (FD_ISSET(sockfd, &rset)) {
                if ( (n = read(sockfd, buf, MAXLINE)) == 0 ) {
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                } else {
                    write(sockfd, buf, n);
                }

                if (--nready < 0) break;
            }
        }
    }
}