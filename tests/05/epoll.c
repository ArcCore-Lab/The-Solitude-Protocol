#include "unp_day05.h"

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

// lib/resp_epoll.c
void response(int sockfd, char *buf, ssize_t n)
{
    char *filename, *body;
    char *ptr, *space;
    int status_code;
    char header[256];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    ptr = strchr(buf, ' ');
    if (!ptr)
    {
        write(sockfd, msg400, strlen(msg400));
        return;
    }
    ptr++;
    space = strchr(ptr, ' ');
    if (space)
        *space = '\0';

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

    write(sockfd, header, strlen(header));
    write(sockfd, body, strlen(body));

    free(body);
}

// src/Day05/tcpserv_epoll.c
int main(int argc, char **argv) {
    int i, maxi, listenfd, connfd, sockfd;
    int nready, epfd;
    ssize_t n;
    char buf[MAXLINE];
    socklen_t clilen;
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (SA *)&servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);
    if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0) err_sys("fcntl O_NONBLOCK error");

    if ( (epfd = epoll_create(MAXFD)) < 0 ) err_sys("epoll_create error");

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0) err_sys("epoll_ctl add listenfd error");

    for (;;) {
        nready = epoll_wait(epfd, events, MAXFD, KEEPALIVE_TIMEOUT);

        if (nready < 0) {
            if (errno == EINTR) continue;
            err_sys("epoll_wait error");
        }

        for (i = 0; i < nready; i++) {
            sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd, (SA *) &cliaddr, &clilen);
                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0) err_sys("fcntl O_NONBLOCK error");

                ev.events = EPOLLIN;
                ev.data.fd = connfd;

                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            } else if (events[i].events & EPOLLIN) {
                if ( (n = read(sockfd, buf, MAXLINE)) <= 0 ) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else {
                    response(sockfd, buf, n);
                }
            }
        }
    }

    close(epfd);
    return 0;
}