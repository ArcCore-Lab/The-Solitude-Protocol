#include "include/unp.h"
#include "tsp.h"

void stc_cli(FILE *fp, int sockfd) {
    int n, maxfdp1, stdineof;
    char buf[MAXLINE];
    fd_set rset;

    stdineof = 0;
    FD_ZERO(&rset);

    for (;;) {
        if (stdineof == 0) FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);

        maxfdp1 = max(fileno(fp), sockfd) + 1;
        select(maxfdp1, &rset, NULL, NULL, NULL);

        // socket is readable
        if (FD_ISSET(sockfd, &rset)) {
            if ( (n = read(sockfd, buf, MAXLINE)) == 0 ) {
                if (stdineof == 1) return;  // normal termination
                else err_quit("stc_cli: server terminated permaturely");
            }

            write(fileno(stdout), buf, n);
        }

        // input is readable
        if (FD_ISSET(fileno(fp), &rset)) {
            if ( (n = read(fileno(fp), buf, MAXLINE)) == 0 ) {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR);  // senf FIN
                FD_CLR(fileno(fp), &rset);
                continue;
            }

            write(sockfd, buf, n);
        }
    }
}