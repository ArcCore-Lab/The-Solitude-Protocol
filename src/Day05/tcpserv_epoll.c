#include "include/unp.h"
#include "lib/tsp.h"

int main(int argc, char **argv) {
    int listenfd, connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);
    if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0) 
        err_sys("fcntl O_NONBLOCK error");

    epfd = epoll_create(MAXFD);

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    for (;;) {
        nready = epoll_wait(epfd, events, MAXFD, KEEPALIVE_TIMEOUT);

        for (i = 0; i < nready; i++) {
            sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd, (SA *) &servaddr, &clilen);
                if(fcntl(connfd, F_SETFL, O_NONBLOCK) < 0) 
                    err_sys("fcntl O_NONBLOCK error");

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            } else if (events[i].events & EPOLLIN) {
                if ( (n = read(sockfd, buf, MAXLINE)) <= 0 ) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else {
                    resp_epoll(sockfd, buf, n);
                }
            }
        }
    }

    close(epfd);
    return 0;
}