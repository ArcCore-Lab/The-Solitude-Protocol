#include "include/unp.h"
#include "tsp.h"
#include "zc.h"

void resp_sendfile(int sockfd, char *buf) {
    int ffd, status_code;
    struct stat st;
    char *ptr, *space;
    char *filename, *filepath;

    int cork = 1;
    conn_n *conn = &cons[sockfd];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    ptr = strchr(buf, ' ');
    if (!ptr) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg400);
        conn->stats = 0;
        conn->wpending = 1;
        return;
    }
    ptr++;

    space = strchr(ptr, ' ');
    if (space) *space = '\0';

    filename = (*ptr == '/') ? ptr + 1 : ptr;
    filepath = ffind(filename, &status_code);
    if (!filepath) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg500);
        conn->stats = 0;
        conn->wpending = 1;
        free(filepath);
        return;
    }

    if ( (ffd = open(filepath, O_RDONLY)) == -1 ) {
        snprintf(conn->hbuf, sizeof(conn->hbuf), "%s", msg500);
        conn->stats = 0;
        conn->wpending = 1;
        free(filepath);
        return;
    }

    if (stat(filepath, &st) != 0) {
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

    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    free(filepath);
}