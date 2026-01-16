#include "include/unp.h"
#include "tsp.h"

void resp_epoll(int sockfd, char *buf, ssize_t n) {
    char *filename, *body;
    char *ptr, *space;
    int status_code;
    char header[256];

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

    write(sockfd, header, strlen(header));
    write(sockfd, body, strlen(body));

    free(body);
}