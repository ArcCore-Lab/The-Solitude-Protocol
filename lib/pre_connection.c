#include "include/unp.h"
#include "tspmalloc.h"
#include "tsp.h"

conn_t conns[MAXFD];
extern MemoryPool pool;

void conn_init(int fd) {
    conns[fd].fd = fd;
    conns[fd].w_used = 0;
    conns[fd].w_sent = 0;
    conns[fd].w_pending = 0;
    conns[fd].w_capacity = BUF_8KB;
    conns[fd].w_max_size = BUF_MAX;
    conns[fd].buf = tspmalloc(&pool, BUF_8KB);
    if (!conns[fd].buf){
        conns[fd].w_pending = -1;
        return;
    }
}

int expand_buffer(conn_t *conn) {
    size_t new_capacity;
    char *tmp;

    if (conn->w_capacity >= conn->w_max_size) return -1;

    new_capacity = conn->w_capacity * 2;
    if (new_capacity > conn->w_max_size)
        new_capacity = conn->w_max_size;

    tmp = tspmalloc(&pool, new_capacity);
    if (!tmp) return -1;

    memcpy(tmp, conn->buf, conn->w_used);
    tspfree(&pool, conn->buf);

    conn->buf = tmp;
    conn->w_capacity = new_capacity;

    return 0;
}

int flush_write_buffer(int sockfd) {
    char *tmp;
    ssize_t n;
    size_t remain;
    conn_t *conn = &conns[sockfd];

    if ( (remain = conn->w_used - conn->w_sent) == 0 ) return 0;

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

        if (conn->w_capacity > BUF_8KB) {
            tmp = tspmalloc(&pool, BUF_8KB);
            if (tmp) {
                memcpy(tmp, conn->buf, conn->w_used);
                tspfree(&pool, conn->buf);
                conn->buf = tmp;
                conn->w_capacity = BUF_8KB;
            }
        }

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
                    while (iov[i].iov_len > (conn->w_capacity - conn->w_used)) {
                        if (expand_buffer(conn) < 0) {
                            conn->w_pending = -1;
                            return;
                        }
                    }

                    memcpy(conn->buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
                    conn->w_used += iov[i].iov_len;
                }

                conn->w_pending = 1;
                return;
            }

            conn->w_pending = -1;
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

            while (copy_len > (conn->w_capacity - conn->w_used)) {
                if (expand_buffer(conn) < 0) {
                    conn->w_pending = -1;
                    return;
                }
            }
                
            memcpy(conn->buf + conn->w_used, (char *)iov[i].iov_base + offset, copy_len);
            conn->w_used += copy_len;
            offset = 0;
       
        }

        conn->w_sent = 0;
        conn->w_pending = 1;

        return;
    }

    for (i = 0; i < iovcnt; i++) {
        while (conn->w_used + iov[i].iov_len > conn->w_capacity) {
            if (expand_buffer(conn) < 0) {
                conn->w_pending = -1;
                return;
            }
        }
        
        memcpy(conn->buf + conn->w_used, iov[i].iov_base, iov[i].iov_len);
        conn->w_used += iov[i].iov_len;
    }

    conn->w_pending = 1;
}