#ifndef _TSP_H
#define _TSP_H

#include "include/unp.h"

#define BUF_8KB 8192
#define BUF_MAX 65536

typedef struct {
    int fd;
    char *buf;
    size_t w_used;
    size_t w_sent;
    size_t w_capacity;
    size_t w_max_size;
    int w_pending;
} conn_t;

void err_sys(const char *fmt, ...);

void err_quit(const char *fmt, ...);

/*
    Parse HTTP request from sockfd.
    Returns a dynamically allocated string containing the requested path.
    Caller is responsible for freeing the returned string.
*/
char *parser(int sockfd);

/*
    Read a line from a descriptor.
    Returns the number of bytes read, 0 on EOF, or -1 on error.
*/
ssize_t Readline(int fd, void *vptr, size_t maxlen);

/*
    Scan for the file with the given filename in the static directory.
    If found, returns a dynamically allocated string containing the file content
    and sets status_code to 200.
    If not found, returns the content of 404.html and sets status_code to 404.
    Caller is responsible for freeing the returned string.
*/
char *scanner(const char *filename, int *status_code);

/*
    Send HTTP response to sockfd.
    The response includes the status line, headers, and body.
    Just for fork-based server.
*/
void resp_fork(int sockfd, char *buf, ssize_t n);

/*
    Send HTTP response to sockfd.
    The response includes the status line, headers, and body.
    Just for epoll-based server.
    Use writev for sending data and add pre-connection buffering.
*/
void resp_epoll(int sockfd, char *buf, ssize_t n);

/*
    Client-side function to handle communication with the server.
*/
void str_cli(FILE *fp, int sockfd);

extern conn_t conns[MAXFD];

/*
    Initialize the connection structure for the given file descriptor.
*/
void conn_init(int fd);

/*
    Expand the write buffer for the given connection.
    Returns 0 on success, -1 on failure.
*/
int expand_buffer(conn_t *conn);

/*
    Flush the write buffer for the given socket file descriptor.
    Returns 0 on success, -1 on error.
*/
int flush_write_buffer(int sockfd);

/*
    Buffered writev function to send data using scatter-gather I/O.
    Data is buffered if the socket is not ready for writing.
*/
void buffered_writev(int sockfd, struct iovec *iov, int iovcnt);

#endif /* _TSP_H */