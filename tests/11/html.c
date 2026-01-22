#include "unp_day10.h"

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
typedef struct request {
    char hbuf[512];
    size_t hsent;

    int ffd;
    off_t foffset;
    size_t fsize;

    char *body;
    size_t bsize;
    size_t bsent;

    int stats;       // 0 = header, 1 = file, 2 = finish
    struct request *next;
} request_t;

typedef struct {
    int fd;

    request_t *req_head;
    request_t *req_tail;
    request_t *req_current;

    // conn_idle = 0, req_queue = 1, req_process  = 2
    // req_complete = 3, conn_closing = 4
    int conn_stats;  
    int wpending;
} conn_q;

conn_q conns[MAXFD];

void conn_init(int fd)
{
    conns[fd].fd = fd;
    conns[fd].wpending = 0;
    conns[fd].conn_stats = 0; // conn_idle

    conns[fd].req_head = NULL;
    conns[fd].req_tail = NULL;
    conns[fd].req_current = NULL;
}

static request_t *create_req(void) {
    request_t *req = (request_t *)malloc(sizeof(request_t));
    if (!req) return NULL;

    memset(req, 0, sizeof(request_t));
    
    req->ffd = -1;

    req->body = NULL;
    req->bsent = 0;
    req->bsize = 0;

    req->stats = 0;
    req->next = NULL;

    return req;
}

static void enqueue_req(int sockfd, request_t *req) {
    conn_q *conn = &conns[sockfd];

    if (conn->req_head == NULL) {
        conn->req_head = req;
        conn->req_tail = req;
        conn->req_current = req;
        conn->conn_stats = 2;
    } else {
        conn->req_tail->next = req;
        conn->req_tail = req;
        conn->conn_stats = 1;
    }
}

static request_t* dequeue_req(int sockfd) {
    conn_q *conn = &conns[sockfd];
    request_t *req;

    if (conn->req_head == NULL) {
        conn->req_current = NULL;
        conn->conn_stats = 0;
        return NULL;
    }

    req = conn->req_head;
    conn->req_head = req->next;

    if (conn->req_head == NULL) {
        conn->req_tail = NULL;
        conn->req_current = NULL;
        conn->conn_stats = 0;
    } else {
        conn->req_current = conn->req_head;
        conn->conn_stats = 2;
    }

    return req;
}

static void free_req(request_t *req) {
    if (req->ffd != -1) close(req->ffd);

    if (req->body != NULL) free(req->body);

    free(req);
}

int flush_write_buffer(int sockfd)
{
    request_t *req;
    ssize_t n;
    size_t remain, headlen;
    conn_q *conn = &conns[sockfd];

    if (conn->req_current == NULL) {
        conn->wpending = 0;
        return 0;
    }

    req = conn->req_current;

    if (req->stats == 0) {
        headlen = strlen(req->hbuf);
        remain = headlen - req->hsent;

        if (remain == 0) {
            req->stats = 1;
            req->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        n = write(sockfd, req->hbuf + req->hsent, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        req->hsent += n;

        if (req->hsent == headlen)
        {
            req->stats = 1;
            req->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        return -1;
    }
    else if (req->stats == 1)
    {
        if (req->ffd == -1 || req->fsize == 0)
        {
            req->stats = 2;
            return flush_write_buffer(sockfd);
        }

        remain = req->fsize - req->foffset;
        if (remain == 0) {
            req->stats = 2;
            return flush_write_buffer(sockfd);
        }

        n = sendfile(sockfd, req->ffd, &req->foffset, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        if (req->foffset >= req->fsize)
        {
            req->stats = 2;
            return flush_write_buffer(sockfd);
        }

        return -1;
    }
    else if (req->stats == 2)
    {
        int cork = 0;
        setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

        free_req(req);
        dequeue_req(sockfd);

        if (conn->req_current != NULL)
            return flush_write_buffer(sockfd);

        conn->wpending = 0;

        return 0;
    }

    return -1;
}

// lib/resp_sendfile.c
static int parser_reqline(const char *buf, size_t buflen, char *method, char *path, char *version) {
    const char *end = strstr(buf, "\r\n");
    if (!end || end - buf >= 256)
        return -1;

    size_t line_len = end - buf;
    char line[256];
    memcpy(line, buf, line_len);
    line[line_len] = '\0';

    if (sscanf(line, "%s %s %s", method, path, version) != 3)
        return -1;

    return line_len + 2;
}

void resp_listdir(int sockfd, char *filename) {
    int status_code;
    conn_q *conn = &conns[sockfd];
    request_t *req = create_req();

    char *dirpath = scanner(filename, &status_code);

    if (!dirpath || status_code != 200) {
        const char msg500[] =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        if (dirpath)
            free(dirpath);
        return;
    }

    DIR *dp = opendir(dirpath);
    if (!dp) {
        const char msg403[] =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg403);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(dirpath);
        return;
    }
}

void resp_sendfile(int sockfd, char *buf) {
    conn_q *conn = &conns[sockfd];
    request_t *req;
    char method[32], path[256], version[32];
    int parser_len;

    parser_len = parser_reqline(buf, MAXLINE, method, path, version);
    if (parser_len < 0) {
        req = create_req();
        if (!req) return;

        const char msg400[] =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg400);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    int status_code;
    char *filename = (*path == '/') ? path + 1 : path;

    size_t flen = strlen(filename);
    if (flen > 0 && filename[flen - 1] == '/') {
        resp_listdir(sockfd, filename);
        return;
    }

    req = create_req();
    if (!req)
    {
        return;
    }

    char *filepath = scanner(filename, &status_code);

    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    if(!filepath) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    int ffd = open(filepath, O_RDONLY);
    if (ffd == -1) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        conn->wpending = 1;
        enqueue_req(sockfd, req);
        free(filepath);
        return;
    }

    struct stat st;
    if(stat(filepath, &st) != 0) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        close(ffd);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(filepath);
        return;
    }

    snprintf(req->hbuf, sizeof(req->hbuf),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=8\r\n\r\n",
             status_code,
             (status_code == 200) ? "OK" : "Not Found",
             st.st_size);

    req->ffd = ffd;
    req->fsize = st.st_size;
    req->hsent = 0;
    req->stats = 0;

    int cork = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;

    free(filepath);
}

// src/Day10/tcpduplexserv.c
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
                    request_t *req;
                    while ((req = dequeue_req(sockfd)) != NULL)
                    {
                        free_req(req);
                    }

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
                int ret = flush_write_buffer(sockfd);

                if (ret == 0)
                {
                    if (conns[sockfd].req_head == NULL) {
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
    return 0;
}