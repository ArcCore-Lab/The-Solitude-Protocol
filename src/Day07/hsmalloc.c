#include "unp_day07.h"

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

// lib/scanner.c
static char *search_in_dir(const char *dir, const char *filename)
{
    int fd;
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
                fd = open(path, O_RDONLY);
                if (fd == -1)
                    break;

                size = (size_t)st.st_size;
                if (size > MAX_FILE_SIZE)
                    size = MAX_FILE_SIZE;

                result = malloc(size + 1);
                if (result)
                {
                    if ((n = read(fd, result, size)) > 0)
                    {
                        result[n] = '\0';
                    }
                    else
                    {
                        free(result);
                        result = NULL;
                    }
                }

                close(fd);
                break;
            }
        }
    }

    closedir(dp);
    return result;
}

char *scanner(const char *filename, int *status_code)
{
    char *content;
    struct stat st;
    size_t size, n;
    int fd;

    const char *path404 = "../static/404.html";

    content = search_in_dir("../static", filename);
    if (content)
    {
        *status_code = 200;
        return content;
    }

    if (stat(path404, &st) != 0)
    {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    fd = open(path404, O_RDONLY);
    if (fd == -1)
    {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    size = (size_t)st.st_size;
    if (size > MAX_FILE_SIZE)
        size = MAX_FILE_SIZE;

    content = malloc(size + 1);
    if (content)
    {
        if ((n = read(fd, content, size)) > 0)
        {
            content[n] = '\0';
        }
        else
        {
            free(content);
            content = strdup("Oops! It seems developer made a little mistake!");
        }
    }
    else
    {
        content = strdup("Oops! It seems developer made a little mistake!");
    }

    close(fd);
    *status_code = 404;

    return content;
}

// lib/pre_connection.c
typedef struct {
    int fd;
    char *w_buf;
    size_t w_used;
    size_t w_sent;
    int w_pending;
    size_t w_capacity;
    size_t w_max_size;
} conn_t;

conn_t conns[MAXFD];

void conn_init(int fd) {
    conns[fd].fd = fd;
    conns[fd].w_used = 0;
    conns[fd].w_sent = 0;
    conns[fd].w_pending = 0;
    conns[fd].w_buf = malloc(BUF_8KB);
    conns[fd].w_capacity = BUF_8KB;
    conns[fd].w_max_size = BUF_MAX;
}

int expand_buffer(conn_t *conn) {
    if (conn->w_capacity >= conn->w_max_size) {
        return -1;
    }

    ssize_t new_capacity = conn->w_capacity * 2;
    if (new_capacity > conn->w_max_size) {
        new_capacity = conn->w_max_size;
    }

    char *tmp = realloc(conn->w_buf, new_capacity);
    if (!tmp) return -1;

    conn->w_capacity *= 2;
    conn->w_buf = tmp;

    return 0;
}

int flush_write_buffer(int sockfd) {
    ssize_t n;
    size_t remain;
    conn_t *conn = &conns[sockfd];

    remain = conn->w_used - conn->w_sent;

    if (remain == 0) return 0;

    if ( (n = write(sockfd, conn->w_buf + conn->w_sent, remain)) < 0 ) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }

        return -1;
    }

    conn->w_sent += n;

    if (conn->w_sent == conn->w_used) {
        conn->w_used = 0;
        conn->w_sent = 0;
        conn->w_pending = 0;

        return 0;
    }

    return -1;
}

void buffered_writev(int sockfd, struct iovec *iov, int iovcnt) {
    int i, eb;
    ssize_t n;
    size_t remain, offset, copy_len;
    size_t total_len = 0;
    conn_t *conn = &conns[sockfd];

    for (i = 0; i < iovcnt; i++) {
        total_len += iov[i].iov_len;
    }

    if (conn->w_used == 0 && conn->w_sent == 0) {
        if ( (n = writev(sockfd, iov, iovcnt)) < 0 ) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                for (i = 0; i < iovcnt; i++) {
                    if (iov[i].iov_len <= (conn->w_capacity - conn->w_used)) {
                        memcpy(conn->w_buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
                        conn->w_used += iov[i].iov_len;
                    } else {
                        if ( (eb = expand_buffer(conn)) == 0 ){
                            memcpy(conn->w_buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
                            conn->w_used += iov[i].iov_len;
                        } else {
                            return;
                        }
                    }
                }
                conn->w_pending = 1;
                return;
            }
            return;
        }

        if ((size_t)n == total_len) return;

        remain = total_len - n;
        offset = n;

        for (i = 0; i < iovcnt; i++) {
            if (offset >= iov[i].iov_len) {
                offset -= iov[i].iov_len;
                continue;
            }

            copy_len = iov[i].iov_len - offset;

            if (copy_len <= (conn->w_capacity - conn->w_used)) {
                memcpy(conn->w_buf + conn->w_used, (char *)iov[i].iov_base + offset, copy_len);
                conn->w_used += copy_len;
                offset = 0;
            } else {
                if ( (eb = expand_buffer(conn)) == 0 ) {
                    memcpy(conn->w_buf + conn->w_used, (char *)iov[i].iov_base + offset, copy_len);
                    conn->w_used += copy_len;
                    offset = 0;
                } else {
                    return;
                }
            }
        }

        conn->w_sent = 0;
        conn->w_pending = 1;
        return;
    }

    for (i = 0; i < iovcnt; i++) {
        while (conn->w_used + iov[i].iov_len > conn->w_capacity) {
            if (expand_buffer(conn) < 0) {
                conns[sockfd].w_pending = -1;
                return;
            }
        }
        memcpy(conn->w_buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
        conn->w_used += iov[i].iov_len;
    }

    conn->w_pending = 1;
}

// lib/resp_epoll.c
void response(int sockfd, char *buf, ssize_t n) {
    char *filename, *body;
    char *ptr, *space;
    int status_code;
    char header[256];
    struct iovec iov[2];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    ptr = strchr(buf, ' ');
    if (!ptr) {
        write(sockfd, msg400, strlen(msg400));
        return;
    }
    ptr++;

    space = strchr(ptr, ' ');
    if (space) *space = '\0';

    filename = (*ptr == '/') ? ptr + 1 : ptr;
    body = scanner(filename, &status_code);

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=8\r\n\r\n",
             status_code,
             (status_code == 200) ? "OK" : "Not Found",
             strlen(body));

    // write(sockfd, header, strlen(header));
    // write(sockfd, body, strlen(body));

    iov[0].iov_base = header;
    iov[0].iov_len = strlen(header);
    iov[1].iov_base = body;
    iov[1].iov_len = strlen(body);

    // writev(sockfd, iov, 2);
    buffered_writev(sockfd, iov, 2);

    free(body);
}

// src/Day07/tcpserv_malloc.c
int main(int argc, char **argv) {
    int listenfd, connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    signal(SIGFPE, SIG_IGN);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);
    if(fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0)
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
                connfd = accept(listenfd, (SA *) &cliaddr, &clilen);

                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0)
                    err_sys("fcntl O_NONBLOCK error");

                conn_init(connfd);

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            } else if (events[i].events & EPOLLIN) {
                if ( (n = read(sockfd, buf, MAXLINE)) <= 0 ) {
                    free(conns[sockfd].w_buf);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else {
                    response(sockfd, buf, n);

                    if (conns[sockfd].w_pending) {
                        ev.events = EPOLLIN | EPOLLOUT;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                if (conns[sockfd].w_pending == -1) {
                    free(conns[sockfd].w_buf);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else if (flush_write_buffer(sockfd) == 0) {
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