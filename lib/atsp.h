#ifndef ZERO_COPY_H
#define ZERO_COPY_H

#include "include/unp.h"

char *ffind(const char *filename, int *status_code);

typedef struct form_item {
    char *key;
    char *value;
    struct form_item *next;
} form_item_t;

typedef struct request {
    char hbuf[512];
    size_t hsent;

    int ffd;
    off_t foffset;
    size_t fsize;

    char *body;
    size_t bsize;
    size_t bsent;

    // 0 = header, 1 = file, 2 = finish
    int stats;

    char *reqbuf;
    size_t reqlen;
    size_t reqcap;

    // 0 = unparse, 1 = parsed line, 
    // 2 = parsed header, 3 = finished
    int req_parse_state;
    int content_length;
    size_t header_end_pos;

    form_item_t *form_data;

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

/*
    Find the end of HTTP headers in the given buffer.
    Parameters:
        buf: The buffer containing HTTP request data.
        len: The length of the buffer.
    Returns:
        The index of the end of headers, or -1 if not found.
*/
int find_head_end(const char *buf, size_t len);

void resp_sendfile(int sockfd, char *buf);

/*
    Check if the given file path corresponds to a CGI script.
    Parameters:
        filepath: The path to the file.
    Returns:
        1 if it is a CGI script, 0 otherwise.
*/
int is_cgi_script(const char *filepath);

/*
    Execute a CGI script and capture its output.
    Parameters:
        sockfd: The socket file descriptor.
        filepath: The path to the CGI script.
        method: The HTTP method (e.g., "GET", "POST").
        query_string: The query string from the URL.
        output: Pointer to store the allocated output buffer.
        output_len: Pointer to store the length of the output.
    Returns:
        0 on success, -1 on failure.
*/
int execute_cgi(int sockfd, const char *filepath, const char *method,
                const char *query_string, char **output, size_t *output_len);

#endif /*ZERO_COPY_H*/