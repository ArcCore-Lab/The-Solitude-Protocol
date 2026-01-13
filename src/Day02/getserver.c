#include "include/unp.h"

void echo_resp(int sockfd) {
    const char *body;
    char header[256];
    char *path;
    int bodlen;

    path = parser(sockfd);

    if (strcmp(path, "/start") == 0) {
        body = "Successfully!\r\nHello World!\r\n";
    } else if (strcmp(path, "/") == 0) {
        body = "Oops! There is nothing!";
    } else {
        body = "Apparently! You're worry!";
    }

    bodlen = strlen(body);
    
    if (strcmp(path, "/start") == 0 || strcmp(path, "/") == 0) {
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n", bodlen);
    } else {
        snprintf(header, sizeof(header),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n", bodlen);
    }

    write(sockfd, header, strlen(header));
    write(sockfd, body, bodlen);

    free(path);
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    pid_t childpid;
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    for (;;) {
        connfd = accept(listenfd, (SA *) &cliaddr, sizeof(cliaddr));

        if ( (childpid = fork()) == 0 ) {
            close(listenfd);
            echo_resp(connfd);
            exit(0);
        }
        close(connfd);
    }
}