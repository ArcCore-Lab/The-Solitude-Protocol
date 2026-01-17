#include "include/unp.h"
#include "tsp.h"

conn_t conns[MAXFD];

void conn_init(int fd) {
    conns[fd].fd = fd;
    conns[fd].w_used = 0;
    conns[fd].w_sent = 0;
    conns[fd].w_pending = 0;
}

int flush_write_buffer(int sockfd) {
    ssize_t n;
    size_t  remain;
    conn_t *conn = &conns[sockfd];

    remain = conn->w_used - conn->w_sent;

    if (remain == 0) return 0;

    if ( (n = write(sockfd, conn->buf + conn->w_sent, remain)) < 0 ) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }

        return -1;
    }

    conn->w_sent += n;

    if (conn->w_sent == conn->w_used) {
        conn->w_used = 0;
        conn->w_sent = 0;
        conn->w_pending = 0;

        return 0;
    }

    return -1;
}

void buffered_writev(int sockfd, struct iovec *iov, int iovcnt) {
    int i;
    ssize_t n;
    size_t remain, offset, copy_len;
    size_t total_len = 0;
    conn_t *conn = &conns[sockfd];

    for (i = 0; i < iovcnt; i++) {
        total_len += iov[i].iov_len;
    }

    if (conn->w_sent == 0 && conn->w_used == 0) {
        if ( (n = writev(sockfd, iov, iovcnt)) < 0 ) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                for (i = 0; i < iovcnt; i++) {
                    memcpy(conn->buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
                    conn->w_used += iov[i].iov_len;
                }
                conn->w_pending = 1;
                return;
            }
            return;
        }

        if ((size_t)n == total_len) return;

        remain = total_len - n;
        offset = n;

        for (i = 0; i < iovcnt; i++) {
            if (offset >= iov[i].iov_len) {
                offset -= iov[i].iov_len;
                continue;
            }

            copy_len = iov[i].iov_len - offset;
            memcpy(conn->buf + conn->w_used, (char *)iov[i].iov_base + offset, copy_len);
            conn->w_used += copy_len;
        }

        conn->w_sent = 0;
        conn->w_pending = 1;
        
        return;
    }

    for (i = 0; i < iovcnt; i++) {
        if (conn->w_used + iov[i].iov_len <= sizeof(conn->buf)) {
            memcpy(conn->buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
            conn->w_used += iov[i].iov_len;
        }
    }

    conn->w_pending = 1;
}