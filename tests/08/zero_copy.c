#include "unp_day08.h"

// lib/error.c
int deamon_proc;

static void err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
    int errno_save, n;
    char buf[MAXLINE + 1];

    errno_save = errno;
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf, MAXLINE, fmt, ap);
#else
    vsprintf(buf, fmt, ap);
#endif
    n = strlen(buf);
    if (errnoflag)
        snprintf(buf + n, MAXLINE - n, ": %s", strerror(errno_save));
    strcat(buf, "\n");

    if (deamon_proc)
    {
        syslog(level, buf);
    }
    else
    {
        fflush(stdout);
        fputs(buf, stderr);
        fflush(stderr);
    }

    return;
}

void err_sys(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(1, LOG_INFO, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(0, LOG_INFO, fmt, ap);
    va_end(ap);
    exit(1);
}

// lib/ffind.c
static char *search_in_dir(const char *dir, const char *filename)
{
    DIR *dp;
    size_t size, n;
    char path[MAXLINE];
    struct stat st;
    struct dirent *ent;
    char *result = NULL;

    if ((dp = opendir(dir)) == NULL)
        return NULL;

    while ((ent = readdir(dp)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (stat(path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            result = search_in_dir(path, filename);
            if (result)
                break;
        }
        else if (S_ISREG(st.st_mode))
        {
            if (strcmp(ent->d_name, filename) == 0)
            {
                result = strdup(path);
                break;
            }
        }
    }

    closedir(dp);
    return result;
}

char *scanner(const char *filename, int *status_code)
{
    char *filepath;
    struct stat st;
    size_t size, n;
    int fd;

    const char *path404 = "../static/404.html";

    filepath = search_in_dir("../static", filename);
    if (filepath)
    {
        *status_code = 200;
        return filepath;
    }

    if (stat(path404, &st) == 0)
    {
        *status_code = 404;
        return strdup(path404);
    }

    *status_code = 500;

    return strdup(path404);
}

// lib/np_conn.c
typedef struct {
    int fd;

    char hbuf[512];
    size_t hsent;

    int ffd;
    off_t foffset;
    size_t fsize;

    int stats; // 0=header, 1=file, 2=finish
    int wpending;
} conn_n;

conn_n conns[MAXFD];

void conn_init(int fd)
{
    conns[fd].fd = fd;
    conns[fd].wpending = 0;
    conns[fd].hsent = 0;
    conns[fd].stats = 0;
    conns[fd].fsize = 0;
    conns[fd].foffset = 0;
    conns[fd].ffd = -1;

    memset(conns[fd].hbuf, 0, sizeof(conns[fd].hbuf));
}

int flush_write_buffer(int sockfd)
{
    char *tmp;
    ssize_t n;
    size_t remain, headlen;
    conn_n *conn = &conns[sockfd];

    if (conn->stats == 0) {
        headlen = strlen(conn->hbuf);
        remain = headlen - conn->hsent;

        if (remain == 0) {
            conn->stats = 1;
            conn->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        n = write(sockfd, conn->hbuf + conn->hsent, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        conn->hsent = n;

        if (conn->hsent == headlen) {
            conn->stats = 1;
            conn->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        return -1;
    } else if (conn->stats == 1) {
        if (conn->ffd == -1 || conn->fsize == 0) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        remain = conn->fsize - conn->foffset;
        if (remain == 0) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        n = sendfile(sockfd, conn->ffd, &conn->foffset, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        if (conn->foffset >= conn->fsize) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        return -1;
    } else if (conn->stats == 2) {
        int cork = 0;
        setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

        if (conn->ffd != -1) {
            close(conn->ffd);
            conn->ffd = -1;
        }

        conn->wpending = 0;

        return 0;
    }

    return -1;
}

// lib/resp_sendfile.c
void resp_sendfile(int sockfd, char *buf) {
    conn_n *conn = &conns[sockfd];

    char *ptr = strchr(buf, ' ');
    if (!ptr) {
        const char msg400[] =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg400);
        conn->stats = 0;
        conn->wpending = 1;
        return;
    }
    ptr++;

    char *space = strchr(ptr, ' ');
    if (space) *space = '\0';

    int status_code;
    char *filename = (*ptr == '/') ? ptr + 1 : ptr;
    char *filepath = scanner(filename, &status_code);

    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    if(!filepath) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg500);
        conn->stats = 0;
        conn->wpending = 1;
        free(filepath);
        return;
    }

    int ffd = open(filepath, O_RDONLY);
    if (ffd == -1) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg500);
        conn->wpending = 1;
        conn->stats = 0;
        free(filepath);
        return;
    }

    struct stat st;
    if(stat(filepath, &st) != 0) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg500);
        close(ffd);
        conn->stats = 0;
        conn->wpending = 1;
        free(filepath);
        return;
    }

    snprintf(conn->hbuf, sizeof(conn->hbuf),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=8\r\n\r\n",
             status_code,
             (status_code == 200) ? "OK" : "Not Found",
             st.st_size);

    conn->ffd = ffd;
    conn->fsize = st.st_size;
    conn->hsent = 0;
    conn->stats = 0;
    conn->wpending = 1;

    int cork = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    free(filepath);
}

// src/Day08/tcpzcserv.c
int main(int argc, char **argv)
{
    int listenfd, connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    signal(SIGPIPE, SIG_IGN);

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

    for (;;)
    {
        nready = epoll_wait(epfd, events, MAXFD, KEEPALIVE_TIMEOUT);

        for (i = 0; i < nready; i++)
        {
            sockfd = events[i].data.fd;

            if (sockfd == listenfd)
            {
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd, (SA *)&cliaddr, &clilen);

                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0)
                    err_sys("fcntl O_NONBLOCK error");

                conn_init(connfd);

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            else if (events[i].events & EPOLLIN)
            {
                if ((n = read(sockfd, buf, MAXLINE)) <= 0)
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                }
                else
                {
                    resp_sendfile(sockfd, buf);
                    if (conns[sockfd].wpending)
                    {
                        ev.events = EPOLLIN | EPOLLOUT;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (flush_write_buffer(sockfd) == 0)
                {
                    ev.events = EPOLLIN;
                    ev.data.fd = sockfd;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                }
            }
        }
    }

    close(epfd);
    return 0;
}