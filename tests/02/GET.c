#include "unp_day02.h"
#include <string.h>

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

// lib/parser.c
char *parser(int sockfd) {
    char line[MAXLINE];
    char *path;
    char *ptr, *space;
    size_t n;

    for (ptr = line; ptr < &line[MAXLINE - 1]; ptr++) {
        if ( (n = read(sockfd, ptr, 1)) != 1) break;
        if (*ptr == '\n') {
            ptr++;
            break;
        }
    }
    *ptr = '\0';

    ptr = strchr(line, ' ');
    if (!ptr) return strdup("");
    ptr++;

    space = strchr(ptr, ' ');
    if (space) *space = '\0';

    path = strdup(ptr);

    return path;
}

// src/Day02/getserver.c
void echo_str(int sockfd) {
    char *path;
    const char *body;
    char header[256];
    int len;

    path = parser(sockfd);

    if (strcmp(path, "/start") == 0) {
        body = "Success visit path `/start`\r\nHello World!\r\n";
    } else if (strcmp(path, "/") == 0) {
        body = "Oops, there is nothing!\r\n";
    } else {
        body = "Apparently!You're Worry!\r\n";
    }

    len = strlen(body);

    if (strcmp(path, "/start") == 0 || strcmp(path, "/") == 0) {
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n", len);
    } else {
        snprintf(header, sizeof(header),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n", len);
    }
    write(sockfd, header, strlen(header));
    write(sockfd, body, len);

    free(path);
}

int main(int argc, char **argv) {
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

    listen(listenfd, LISTENQ);

    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (SA *) &cliaddr, &clilen);
        
        if ( (childpid = fork()) == 0 ) {
            close(listenfd);
            echo_str(connfd);
            exit(0);
        }
        close(connfd);
    }
}