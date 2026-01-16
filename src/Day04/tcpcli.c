/*
    The real change is in `lib/str_cli.c`: `main()` here is original.
    `str_cli()` now uses `select()` to multiplex stdin/stdout and the socket.

    It's worth mentioning that this client is used to communicate with the server(`tcpserv_select.c`)
    - its sole functions are: connecting to the server, sending characters, returning them exactly as received, and manually closing the connection.
*/

#include "include/unp.h"
#include "lib/tsp.h"

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in servaddr;

    if (argc != 2) err_quit("usage: tcpcli <IPaddress>");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

    str_cli(stdin, sockfd);

    exit(0);
}