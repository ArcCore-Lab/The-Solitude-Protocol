#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"
#include "tspmalloc.h"

static void handle_reload(int sig) {
    (void)sig;
    g_should_reload = 1;
}

static void handle_shutdown(int sig) {
    (void)sig;
    g_should_shutdown = 1;
}

int master_create_listenfd(void) {
    int listenfd;
    struct sockaddr_in servaddr;

    int reuse = 1;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) err_sys("socket error");

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
        err_sys("setsockopt SO_REUSEPORT error");

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        err_sys("setsockopt SO_REUSEADDR error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    if (bind(listenfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
        err_sys("bind error");
    }

    if (listen(listenfd, LISTENQ) < 0) {
        err_sys("listen error");
    }

    if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0) {
        err_sys("fcntl O_NONBLOCK error");
    }

    fprintf(stderr, "Master created listenfd: %d\n", listenfd);

    return listenfd;
}

void worker_loop(int worker_id, int listenfd) {
    int connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in cliaddr;

    time_t last_log_flush = 0;

    epfd = epoll_create(MAXFD);
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    fprintf(stderr, "Worker %d started (PID: %d)\n", worker_id, getpid());

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

                if (connfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    } else {
                        err_sys("accept error");
                    }
                }

                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0)
                    err_sys("fcntl O_NONBLOCK error");

                conn_init(connfd);

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);


            } else if (events[i].events & EPOLLIN) {
                if ((n = read(sockfd, buf, MAXLINE)) <= 0) {
                    request_t *req;
                    while ((req = dequeue_req(sockfd)) != NULL)
                    {
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

                        if (req->content_length == 0 || body_received >= req->content_length) {
                            req->req_parse_state = 3;
                            resp_sendfile(sockfd, req->reqbuf);

                            if (conq[sockfd].wpending) {
                                ev.events = EPOLLIN | EPOLLOUT;
                                ev.data.fd = sockfd;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                            }
                        }
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                int ret = writefbuf(sockfd);

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

    close(epfd);
}

void master_process(int listenfd) {
    int i, status;
    pid_t pids[NUM_WORKERS];

    int worker_count = NUM_WORKERS;

    signal(SIGHUP, handle_reload);
    signal(SIGTERM, handle_shutdown);
    signal(SIGCHLD, SIG_IGN);

    for (i = 0; i < NUM_WORKERS; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            err_sys("fork error");
        } else if (pids[i] == 0) {
            worker_loop(i, listenfd);
            exit(0);
        }
    }

    fprintf(stderr, "Master process (PID: %d) spawned %d workers\n", getpid(), NUM_WORKERS);

    for (;;) {
        if (g_should_reload) {
            fprintf(stderr, "Master received SIGHUP, preparing reload...\n");

            pid_t new_master = fork();
            if (new_master < 0) {
                err_sys("fork error in reload");
            } else if (new_master == 0) {
                char fd_str[16];
                snprintf(fd_str, sizeof(fd_str), "%d", listenfd);

                setenv(LISTEN_FD_ENV, fd_str, 1);

                fprintf(stderr, "New master execing with LISTEN_FD=%s\n", fd_str);

                extern char **environ;
                execve(g_program_path, g_argv_copy, environ);

                err_sys("execve error");
            }

            g_should_reload = 0;
            g_should_shutdown = 1;
        }

        if (g_should_shutdown) {
            fprintf(stderr, "Master shutting down gracefully...\n");

            pid_t dead_pid;
            int alive_count = worker_count;

            while (alive_count > 0) {
                dead_pid = wait(&status);
                if (dead_pid > 0) {
                    fprintf(stderr, "Worker %d exited\n", dead_pid);
                    alive_count--;
                }
            }

            fprintf(stderr, "All workers shut down, master exiting\n");
            break;
        }

        pid_t dead_pid = wait(&status);
        if (dead_pid > 0) {
            for (i = 0; i < worker_count; i++) {
                if (pids[i] == dead_pid) {
                    fprintf(stderr, "Worker %d died, respawning...\n", i);

                    pids[i] = fork();
                    if (pids[i] < 0) {
                        err_sys("fork error in respawn");
                    } else if (pids[i] == 0) {
                        worker_loop(i, listenfd);
                        exit(0);
                    }
                    break;
                }
            }
        }
    }
}