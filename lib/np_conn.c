#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"
#include "tspmalloc.h"

request_t *create_req(void) {
    request_t *req = (request_t *)tspmalloc(&pool, sizeof(request_t));
    if (!req) return NULL;

    memset(req, 0, sizeof(request_t));

    req->ffd = -1;
    req->stats = 0;
    req->next = NULL;

    return req;
}

void enqueue_req(int sockfd, request_t *req) {
    conn_q *conn = &conq[sockfd];

    if (conn->req_head == NULL) {
        conn->req_head = req;
        conn->req_tail = req;
        conn->req_current = req;
        conn->conn_stats = 2;
    } else {
        conn->req_tail->next = req;
        conn->req_tail = req;
        conn->conn_stats = 1;
    }
}

request_t *dequeue_req(int sockfd) {
    conn_q *conn = &conn[sockfd];
    request_t *req;

    if (conn->req_head == NULL) {
        conn->req_current = NULL;
        conn->conn_stats = 0;
        return NULL;
    }

    req = conn->req_head;
    conn->req_head->next = req->next;

    if (conn->req_head == NULL) {
        conn->req_tail = NULL;
        conn->req_current = NULL;
        conn->conn_stats = 0;
    } else {
        conn->req_current = conn->req_head;
        conn->conn_stats = 2;
    }

    return req;
}

void free_req(request_t *req) {
    if (req->ffd != -1) close(req->ffd);

    tspfree(&pool, req);
}

void init_conn(int fd) {
    conq[fd].fd = fd;

    conq[fd].conn_stats = 0;
    conq[fd].wpending = 0;

    conq[fd].req_head = NULL;
    conq[fd].req_tail = NULL;
    conq[fd].req_current = NULL;
}

int writefbuf(int sockfd) {
    ssize_t n;
    request_t *req;
    size_t remain, headlen;

    int cork = 0;
    conn_q *conn = &conq[sockfd];

    if (conn->req_current == NULL) {
        conn->wpending = 0;
        return 0;
    }

    req = conn->req_current;

    if (req->stats == 0) {
        headlen = strlen(req->hbuf);
        remain = headlen - req->hsent;

        if (remain == 0) {
            req->stats = 1;
            req->foffset = 0;
            return writefbuf(sockfd);
        }

        n = write(sockfd, req->hbuf + req->hsent, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        req->hsent += n;

        if (req->hsent == headlen) {
            req->stats = 1;
            req->foffset = 0;
            return writefbuf(sockfd);
        }

        return -1;
    } else if (req->stats == 1) {
        if (req->ffd == -1 || req->fsize == 0) {
            req->stats = 2;
            return writefbuf(sockfd);
        }

        remain = req->fsize - req->foffset;
        if (remain == 0) {
            req->stats = 2;
            return writefbuf(sockfd);
        }

        n = sendfile(sockfd, req->ffd, &req->foffset, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return - 1;
        }

        if (req->foffset >= req->fsize) {
            req->stats = 2;
            return writefbuf(sockfd);
        }

        return -1;
    } else if (req->stats == 2) {
        setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

        free_req(req);
        dequeue_req(sockfd);

        if (conn->req_current != NULL)
            return writefbuf(sockfd);

        conn->wpending = 0;

        return 0;
    }

    return -1;
}