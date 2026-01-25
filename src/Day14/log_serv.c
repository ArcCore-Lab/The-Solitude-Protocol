#include "include/unp.h"
#include "lib/tsp.h"
#include "lib/atsp.h"
#include "lib/tspmalloc.h"

int main(int argc, char **argv) {
    int listenfd, connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    time_t last_log_flush = 0;

    signal(SIGPIPE, SIG_IGN);

    log_init();
    tsp_init(&pool);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *)&servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);
    if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0)
        err_sys("fcntl O_NONBLOCK error");

    epfd = epoll_create(MAXFD);
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    for (;;) {
        nready = epoll_wait(epfd, events, MAXFD, KEEPALIVE_TIMEOUT);

        time_t now = time(NULL);
        if (now - last_log_flush >= 1) {
            log_flush_periodic();
            last_log_flush = now;
        }

        for (i = 0; i < nready; i++) {
            sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd, (SA *)&cliaddr, &clilen);

                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0)
                    err_sys("fcntl O_NONBLOCK error");

                conn_init(connfd);

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            } else if (events[i].events & EPOLLIN) {
                if ((n = read(sockfd, buf, MAXLINE)) <= 0) {
                    request_t *req;
                    while ((req = dequeue_req(sockfd)) != NULL) {
                        free_req(req);
                    }

                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else {
                    conn_q *conn = &conns[sockfd];
                    request_t *req = conn->req_current;

                    if (!req) {
                        req = create_req();
                        if (!req) {
                            close(sockfd);
                            continue;
                        }
                        enqueue_req(sockfd, req);
                    }

                    if (req->reqlen + n > req->reqcap) {
                        size_t new_cap = req->reqcap * 2;
                        char *new_buf = (char *)realloc(req->reqbuf, new_cap);
                        if (!new_buf) {
                            close(sockfd);
                            continue;
                        }
                        req->reqbuf = new_buf;
                        req->reqcap = new_cap;
                    }

                    memcpy(req->reqbuf + req->reqlen, buf, n);
                    req->reqlen += n;

                    int header_end = find_head_end(req->reqbuf, req->reqlen);

                    if (header_end >= 0) {
                        req->header_end_pos = header_end;
                        req->req_parse_state = 2;

                        char *content_len_str = strstr(req->reqbuf, "Content-Length:");
                        if (content_len_str) {
                            sscanf(content_len_str, "Content-Length: %d", &req->content_length);
                        }

                        size_t body_start = header_end + 4;
                        size_t body_received = req->reqlen - body_start;

                        if (req->content_length == 0 || body_received >= req->content_length)
                        {
                            req->req_parse_state = 3;
                            resp_sendfile(sockfd, req->reqbuf);

                            if (conq[sockfd].wpending)
                            {
                                ev.events = EPOLLIN | EPOLLOUT;
                                ev.data.fd = sockfd;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                            }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLOUT) {
                int ret = flush_write_buffer(sockfd);

                if (ret == 0) {
                    if (conq[sockfd].req_head == NULL) {
                        ev.events = EPOLLIN;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    } else {
                        ev.events = EPOLLIN | EPOLLOUT;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    }
                } else if (ret < 0) {
                    ev.events = EPOLLIN | EPOLLOUT;
                    ev.data.fd = sockfd;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                }
            }
        }
    }

    tsp_destroy(&pool);
    close(epfd);
    return 0;
}