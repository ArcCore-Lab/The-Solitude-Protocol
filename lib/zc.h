#ifndef ZC_H
#define ZC_H

#include "include/unp.h"

char *ffind(const char *filename, int *status_code);

typedef struct {
    int fd;

    char hbuf[512];
    size_t hsent;

    int ffd;
    off_t foffset;
    size_t fsize;

    int stats;
    int wpending;
} conn_n;

extern conn_n cons[MAXFD];

void init_conn(int fd);

int writefbuf(int sockfd);

void resp_sendfile(int sockfd, char *buf);

#endif /*ZC_H*/