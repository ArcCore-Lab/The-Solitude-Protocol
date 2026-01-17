#include "include/unp.h"
#include "tsp.h"

void resp_fork(int sockfd, char *buf, ssize_t n) {
    char *uri, *body, *filename;
    char header[256];
    int status_code;
    struct iovec iov[2];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    uri = parser(sockfd);
    if(!uri) {
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

    // write(sockfd, header, strlen(header));
    // write(sockfd, body, strlen(body));

    iov[0].iov_base = header;
    iov[0].iov_len = strlen(header);
    iov[1].iov_base = body;
    iov[1].iov_len = strlen(body);

    // writev(sockfd, iov, 2);
    buffered_writev(sockfd, iov, 2);

    free(uri);
    free(body);
}