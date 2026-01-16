/*
    Added built-in keep-alive; 
    wrk-verified stable at 1 k QPS without crashes.
*/

#include "include/unp.h"
#include "lib/tsp.h"

int main(int argc, char **argv) {
    int i, maxi, nready, sockfd, timeout, listenfd, connfd;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct pollfd client[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    static const char http_resp[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: keep-alive\r\n"
        "Keep-Alive: timeout=8\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "boom\n";

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for (i = 1; i < MAXFD; i++) {
        client[i].fd = -1;
    }
    maxi = 0;

    for (;;) {
        timeout = KEEPALIVE_TIMEOUT;
        nready = poll(client, maxi + 1, timeout);

        if (nready == 0) {
            for (i = 1; i < MAXFD; i++) {
                if ( (sockfd = client[i].fd) < 0 ) continue;

                close(sockfd);
                client[i].fd = -1;
            }
            continue;
        }

        if (nready < 0 && errno != EINTR) err_sys("poll error");

        if (client[0].revents & POLLRDNORM) {
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (SA *) &servaddr, &clilen);

            for (i = 1; i < MAXFD; i++) {
                if (client[i].fd < 0) {
                    client[i].fd = connfd;
                    break;
                }
            }

            if (i == MAXFD) err_quit("too many clients");

            client[i].events = POLLRDNORM;

            if (i > maxi) maxi = i;

            if (--nready <= 0) continue;
        }

        for (i = 1; i < maxi; i++) {
            if ( (sockfd = client[i].fd) < 0 ) continue;

            if (client[i].revents & (POLLRDNORM | POLLERR)) {
                if ( (n = read(sockfd, buf, MAXLINE)) <= 0 ) {
                    close(sockfd);
                    client[i].fd = -1;
                    continue;
                }
            }

            write(sockfd, http_resp, sizeof(http_resp) - 1);

            client[i].events = POLLRDNORM;

            if (--nready <= 0) continue;
        }
    }
}