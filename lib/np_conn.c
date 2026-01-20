#include "include/unp.h"
#include "tsp.h"
#include "zc.h"

void init_conn(int fd) {
    cons[fd].fd = fd;

    cons[fd].hsent = 0;

    cons[fd].ffd = -1;
    cons[fd].foffset = 0;
    cons[fd].fsize = 0;

    cons[fd].stats = 0;
    cons[fd].wpending = 0;

    memset(cons[fd].hbuf, 0, sizeof(cons[fd].hbuf));
}

int writefbuf(int sockfd) {
    ssize_t n;
    size_t remain, headlen;

    int cork = 0;
    conn_n *conn = &cons[sockfd];

    if (conn->stats == 0) {
        headlen = strlen(conn->hbuf);
        remain = headlen - conn->hsent;

        if (remain == 0) {
            conn->stats = 1;
            conn->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        n = write(sockfd, conn->hbuf + conn->hsent, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        conn->hsent = n;

        if (conn->hsent == headlen) {
            conn->stats = 1;
            conn->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        return -1;
    } else if (conn->stats == 1) {
        if (conn->ffd == -1 || conn->fsize == 0) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        remain = conn->fsize - conn->foffset;
        if (remain == 0) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        n = sendfile(sockfd, conn->ffd, &conn->foffset, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return - 1;
        }

        if (conn->foffset >= conn->fsize) {
            conn->stats = 2;
            return flush_write_buffer(sockfd);
        }

        return -1;
    } else if (conn->stats == 2) {
        setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

        if (conn->ffd != -1) {
            close(conn->ffd);
            conn->ffd = -1;
        }

        conn->wpending = 0;

        return 0;
    }

    return -1;
}