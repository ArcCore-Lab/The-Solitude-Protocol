#ifndef ZERO_COPY_H
#define ZERO_COPY_H

#include "include/unp.h"

char *ffind(const char *filename, int *status_code);

typedef struct request {
    char hbuf[512];
    size_t hsent;

    int ffd;
    off_t foffset;
    size_t fsize;

    // 0 = header, 1 = file, 2 = finish
    int stats;
    struct request *next;
} request_t;

typedef struct {
    int fd;

    request_t *req_head;
    request_t *req_tail;
    request_t *req_current;

    // conn_idle = 0, req_queue = 1, req_process = 2
    // req_complete = 3, conn_closing = 4
    int conn_stats;
    int wpending;
} conn_q;

extern conn_q conq[MAXFD];

request_t *create_req(void);

void enqueue_req(int sockfd, request_t *req);

request_t *dequeue_req(int sockfd);

void free_req(request_t *req);

void init_conn(int fd);

int writefbuf(int sockfd);

int parser_reqline(const char *buf, size_t buflen, char *method, char *path, char *version);

void resp_sendfile(int sockfd, char *buf);

#endif /*ZERO_COPY_H*/