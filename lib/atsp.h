#ifndef ZERO_COPY_H
#define ZERO_COPY_H

#include "include/unp.h"

#define NUM_WORKERS 4
#define LISTEN_FD_ENV "LISTEN_FD"

#define LOG_BUFFER_SIZE 65536
#define LOG_MAX_SIZE (100 * 1024 * 1024)
#define LOG_DIR "../log"

int g_listenfd = -1;
int g_should_reload = 0;
int g_should_shutdown = 0;

char g_program_path[PATH_MAX] = {0};
char *g_argv_copy[128] = {0};

char *ffind(const char *filename, int *status_code);

typedef struct {
    char buf[LOG_BUFFER_SIZE];
    size_t offset;
    pthread_mutex_t mutex;
    int logfd;
    off_t file_size;
    char logfile[256];
} log_buffer_t;

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

    char method[32];
    char path[256];
    char version[32];
    char referer[512];
    char user_agent[512];
    int status_code;
    time_t request_time;

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

/*
    Log an access entry for an HTTP request.
    Parameters:
        sockfd: The socket file descriptor.
        method: The HTTP method used.
        path: The requested path.
        version: The HTTP version.
        status_code: The HTTP status code of the response.
        bytes_sent: The number of bytes sent in the response.
        referer: The HTTP referer header.
        user_agent: The HTTP user-agent header.
*/    
void access_log(int sockfd, const char *method, const char *path, const char *version,
                int status_code, size_t bytes_sent, const char *referer, const char *user_agent);

/*
    Initialize the logging system.
*/
void log_init(void);

/*
    Flush the log buffer to the log file periodically.
*/
void log_flush_periodic(void);

/*
    The main master process function that handles worker processes.
    Parameters:
        listenfd: The listening socket file descriptor.
*/
void master_process(int listenfd);

/*
    Create and set up the listening socket.
    Returns:
        The listening socket file descriptor.
*/
int master_create_listenfd(void);

#endif /*ZERO_COPY_H*/