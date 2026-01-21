#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"

void resp_sendfile(int sockfd, char *buf) {
    int parser_len;
    int ffd, status_code;
    struct stat st;
    request_t *req;
    char *filename, *filepath;
    char method[32], path[256], version[32];

    int cork = 1;
    conn_q *conn = &conq[sockfd];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    parser_len = parser_reqline(buf, MAXLINE, method, path, version);
    if (parser_len < 0) {
        req = create_req();
        if (!req) return;

        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg400);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    filename = (*path == '/') ? path + 1 : path;

    req = create_req();
    if (!req) return;

    filepath = ffind(filename, &status_code);
    if (!filepath) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    if ( (ffd = open(filepath, O_RDONLY)) == -1 ) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(filepath);
        return;
    }

    if (stat(filepath, &st) != 0) {
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

    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;

    free(filepath);
}