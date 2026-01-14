#include "unp_day03.h"

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
char *parser(int sockfd)
{
    char line[MAXLINE];
    char *path;
    char *ptr, *space;
    size_t n;

    for (ptr = line; ptr < &line[MAXLINE - 1]; ptr++)
    {
        if ((n = read(sockfd, ptr, 1)) != 1)
            break;
        if (*ptr == '\n')
        {
            ptr++;
            break;
        }
    }
    *ptr = '\0';

    ptr = strchr(line, ' ');
    if (!ptr)
        return strdup("");
    ptr++;

    space = strchr(ptr, ' ');
    if (space)
        *space = '\0';

    path = strdup(ptr);

    return path;
}

// lib/scanner.c
static char *search_in_dir(const char *dir, const char *filename) {
    int fd;
    DIR *dp;
    size_t size, n;
    char path[MAXLINE];
    struct stat st;
    struct dirent *ent;
    char *result = NULL;

    if( (dp = opendir(dir)) == NULL ) return NULL;

    while ((ent = readdir(dp))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (stat(path, &st) != 0) continue;

        if(S_ISDIR(st.st_mode)) {
            result = search_in_dir(path, filename);
            if (result) break;
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(ent->d_name, filename) == 0) {
                fd = open(path, O_RDONLY);
                if (fd == -1) break;

                size = (size_t)st.st_size;
                if (size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;

                result = malloc(size + 1);
                if (result) {
                    if ( (n = read(fd, result, size)) > 0 ) {
                        result[n] = '\0';
                    } else {
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

char *scanner(const char *filename, int *status_code) {
    char *content;
    struct stat st;
    size_t size, n;
    int fd;

    const char *path404 = "../static/404.html";

    content = search_in_dir("../static", filename);
    if (content) {
        *status_code = 200;
        return content;
    }

    if (stat(path404, &st) != 0) {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    fd = open(path404, O_RDONLY);
    if (fd == -1) {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    size = (size_t)st.st_size;
    if (size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;

    content = malloc(size + 1);
    if (content) {
        if ( (n = read(fd, content, size)) > 0 ) {
            content[n] = '\0';
        } else {
            free(content);
            content = strdup("Oops! It seems developer made a little mistake!");
        }
    } else {
        content = strdup("Oops! It seems developer made a little mistake!");
    }

    close(fd);
    *status_code = 404;

    return content;
}

// lib/response.c
void response(int sockfd) {
    char *uri, *filename, *body;
    int status_code;
    char header[256];
    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    uri = parser(sockfd);
    if (!uri) {
        write(sockfd, msg400, strlen(msg400));
        return;
    }

    filename = (*uri == '/') ? uri + 1 : uri;
    body = scanner(filename, &status_code);

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n", 
             status_code, 
             (status_code == 200) ? "OK" : "Not Found", 
             strlen(body));

    write(sockfd, header, strlen(header));
    write(sockfd, body, strlen(body));

    free(body);
    free(uri);
}

// src/Day03/respstatfile.c
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

    signal(SIGPIPE, SIG_IGN);

    listen(listenfd, LISTENQ);

    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (SA *) &cliaddr, &clilen);

        if ( (childpid = fork()) == 0 ) {
            close(listenfd);
            response(connfd);
            exit(0);
        }

        close(connfd);
    }
}