#include "unp_day15.h"

// lib/error.c
int deamon_proc;

static void err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
    int errno_save, n;
    char buf[MAXLINE + 1];

    errno_save = errno;
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf, MAXLINE, fmt, ap);
#else
    vsprintf(buf, fmt, ap);
#endif
    n = strlen(buf);
    if (errnoflag)
        snprintf(buf + n, MAXLINE - n, ": %s", strerror(errno_save));
    strcat(buf, "\n");

    if (deamon_proc)
    {
        syslog(level, buf);
    }
    else
    {
        fflush(stdout);
        fputs(buf, stderr);
        fflush(stderr);
    }

    return;
}

void err_sys(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(1, LOG_INFO, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(0, LOG_INFO, fmt, ap);
    va_end(ap);
    exit(1);
}

// lib/ffind.c
static char *search_in_dir(const char *dir, const char *filename)
{
    DIR *dp;
    size_t size, n;
    char path[MAXLINE];
    struct stat st;
    struct dirent *ent;
    char *result = NULL;

    if ((dp = opendir(dir)) == NULL)
        return NULL;

    while ((ent = readdir(dp)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (stat(path, &st) != 0)
            continue;

        if (strcmp(ent->d_name, filename) == 0)
        {
            result = strdup(path);
            break;
        }

        if (S_ISDIR(st.st_mode))
        {
            result = search_in_dir(path, filename);
            if (result)
                break;
        }
        else if (S_ISREG(st.st_mode))
        {
            if (strcmp(ent->d_name, filename) == 0)
            {
                result = strdup(path);
                break;
            }
        }
    }

    closedir(dp);
    return result;
}

char *scanner(const char *filename, int *status_code)
{
    char filepath[MAXLINE];
    char realpath_buf[MAXLINE];
    char static_realpath[MAXLINE];
    struct stat st;
    size_t size, n;
    int fd;

    const char *path404 = "../static/404.html";

    snprintf(filepath, sizeof(filepath), "../static/%s", filename);

    if (realpath(filepath, realpath_buf) == NULL) {
        *status_code = 404;
        return strdup(path404);
    }

    if (realpath("../static", static_realpath) == NULL) {
        *status_code = 500;
        return strdup(path404);
    }

    if (strncmp(realpath_buf, static_realpath, strlen(static_realpath)) != 0) {
        *status_code = 404;
        return strdup(path404);
    }

    if (stat(realpath_buf, &st) == 0) {
        *status_code = 200;
        return strdup(realpath_buf);
    }

    // filepath = search_in_dir("../static", filename);
    // if (filepath)
    // {
    //     if (stat(filepath, &st) == 0)
    //     {
    //         *status_code = 200;
    //         return filepath;
    //     }
    // }

    if (stat(path404, &st) == 0)
    {
        *status_code = 404;
        return strdup(path404);
    }

    *status_code = 500;

    return strdup(path404);
}

// lib/access_log.c
typedef struct {
    char buf[LOG_BUFFER_SIZE];
    size_t offset;
    pthread_mutex_t mutex;
    int logfd;
    off_t file_size;
    char logfile[256];
} log_buf_t;

static log_buf_t g_log_buf = {
    .offset = 0,
    .logfd = -1,
    .file_size = 0,
};

static void log_rotate(void) {
    if (g_log_buf.logfd != -1) {
        close(g_log_buf.logfd);
        g_log_buf.logfd = -1;
    }

    char backup_name[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    snprintf(backup_name, sizeof(backup_name), "%s/access-%04d%02d%02d-%02d%02d%02d.log",
             LOG_DIR,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    rename(g_log_buf.logfile, backup_name);

    g_log_buf.file_size = 0;
}

static int log_open(void)
{
    snprintf(g_log_buf.logfile, sizeof(g_log_buf.logfile), "%s/access.log", LOG_DIR);

    g_log_buf.logfd = open(g_log_buf.logfile,
                           O_CREAT | O_WRONLY | O_APPEND,
                           0644);

    if (g_log_buf.logfd == -1)
    {
        perror("open access.log");
        return -1;
    }

    struct stat st;
    if (stat(g_log_buf.logfile, &st) == 0)
    {
        g_log_buf.file_size = st.st_size;
    }

    return 0;
}

static void log_init(void)
{
    mkdir(LOG_DIR, 0755);
    pthread_mutex_init(&g_log_buf.mutex, NULL);
    log_open();
}

static void log_flush(void)
{
    if (g_log_buf.offset == 0 || g_log_buf.logfd == -1)
        return;

    ssize_t written = write(g_log_buf.logfd, g_log_buf.buf, g_log_buf.offset);
    if (written > 0)
    {
        g_log_buf.file_size += written;
        g_log_buf.offset = 0;

        if (g_log_buf.file_size >= LOG_MAX_SIZE)
        {
            log_rotate();
            log_open();
        }
    }
}

static void log_write_line(const char *line)
{
    pthread_mutex_lock(&g_log_buf.mutex);

    size_t len = strlen(line);

    if (g_log_buf.offset + len > LOG_BUFFER_SIZE)
    {
        log_flush();
    }

    if (len <= LOG_BUFFER_SIZE - g_log_buf.offset)
    {
        memcpy(g_log_buf.buf + g_log_buf.offset, line, len);
        g_log_buf.offset += len;
    }

    pthread_mutex_unlock(&g_log_buf.mutex);
}

static void format_time(char *buf, size_t buflen)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    strftime(buf, buflen, "%z", tm_info);
    char tz[10];
    strcpy(tz, buf);

    snprintf(buf, buflen, "[%02d/%s/%04d:%02d:%02d:%02d %s]",
             tm_info->tm_mday,
             months[tm_info->tm_mon],
             tm_info->tm_year + 1900,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec,
             tz);
}

static void get_client_ip(int sockfd, char *ip, size_t iplen)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &addr.sin_addr, ip, iplen);
    }
    else
    {
        strncpy(ip, "-", iplen - 1);
    }
}

void access_log(int sockfd, const char *method, const char *path, const char *version,
                int status_code, size_t bytes_sent, const char *referer, const char *user_agent)
{

    char client_ip[INET_ADDRSTRLEN];
    char time_str[64];
    char log_line[2048];

    get_client_ip(sockfd, client_ip, sizeof(client_ip));
    format_time(time_str, sizeof(time_str));

    snprintf(log_line, sizeof(log_line),
             "%s - - %s \"%s %s %s\" %d %zu \"%s\" \"%s\"\n",
             client_ip,
             time_str,
             method,
             path,
             version,
             status_code,
             bytes_sent,
             referer ? referer : "-",
             user_agent ? user_agent : "-");

    log_write_line(log_line);
}

void log_flush_periodic(void)
{
    pthread_mutex_lock(&g_log_buf.mutex);
    log_flush();
    pthread_mutex_unlock(&g_log_buf.mutex);
}

// lib/np_conn.c
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

    int stats;       // 0 = header, 1 = file, 2 = finish

    char *req_buf;
    size_t req_len;
    size_t req_cap;

    int req_parse_state;
    size_t header_end_pos;
    int content_length;

    form_item_t *form_data;

    char method[32];
    char path[256];
    char version[32];
    char referer[512];
    char user_agent[512];
    int status_code;
    time_t req_time;

    struct request *next;
} request_t;

typedef struct {
    int fd;

    request_t *req_head;
    request_t *req_tail;
    request_t *req_current;

    // conn_idle = 0, req_queue = 1, req_process  = 2
    // req_complete = 3, conn_closing = 4
    int conn_stats;  
    int wpending;
} conn_q;

conn_q conns[MAXFD];

void conn_init(int fd)
{
    conns[fd].fd = fd;
    conns[fd].wpending = 0;
    conns[fd].conn_stats = 0; // conn_idle

    conns[fd].req_head = NULL;
    conns[fd].req_tail = NULL;
    conns[fd].req_current = NULL;
}

static request_t *create_req(void) {
    request_t *req = (request_t *)malloc(sizeof(request_t));
    if (!req) return NULL;

    memset(req, 0, sizeof(request_t));
    
    req->ffd = -1;
    req->body = NULL;
    req->bsent = 0;
    req->bsize = 0;
    req->stats = 0;
    req->form_data = NULL;
    req->next = NULL;

    req->req_cap = 8192;
    req->req_buf = (char *)malloc(req->req_cap);
    if (!req->req_buf) {
        free(req);
        return NULL;
    }
    req->req_len = 0;
    req->req_parse_state = 0; // 0 = unparse, 1 = parsed line, 2 = parsed header, 3 = finished 
    req->header_end_pos = 0;
    req->content_length = 0;

    req->req_time = time(NULL);
    strcpy(req->referer, "-");
    strcpy(req->user_agent, "-");

    return req;
}

static void enqueue_req(int sockfd, request_t *req) {
    conn_q *conn = &conns[sockfd];

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

static request_t* dequeue_req(int sockfd) {
    conn_q *conn = &conns[sockfd];
    request_t *req;

    if (conn->req_head == NULL) {
        conn->req_current = NULL;
        conn->conn_stats = 0;
        return NULL;
    }

    req = conn->req_head;
    conn->req_head = req->next;

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

static void free_req(request_t *req) {
    if (req->ffd != -1) close(req->ffd);
    if (req->body != NULL) free(req->body);
    if (req->req_buf != NULL) free(req->req_buf);

    form_item_t *cur = req->form_data;
    while (cur) {
        form_item_t *next = cur->next;
        if (cur->key) free(cur->key);
        if (cur->value) free(cur->value);
        free(cur);
        cur = next;
    }

    free(req);
}

static void extract_http_headers(const char *buf, request_t *req) {
    const char *referer_ptr = strstr(buf, "Referer:");
    if (referer_ptr) {
        sscanf(referer_ptr, "Referer: %511[^\r\n]", req->referer);
    }

    const char *ua_ptr = strstr(buf, "User-Agent:");
    if (ua_ptr) {
        sscanf(ua_ptr, "User-Agent: %511[^\r\n]", req->user_agent);
    }
}

int flush_write_buffer(int sockfd)
{
    request_t *req;
    ssize_t n;
    size_t remain, headlen;
    conn_q *conn = &conns[sockfd];

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
            return flush_write_buffer(sockfd);
        }

        n = write(sockfd, req->hbuf + req->hsent, remain);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

            return -1;
        }

        req->hsent += n;

        if (req->hsent == headlen)
        {
            req->stats = 1;
            req->foffset = 0;
            return flush_write_buffer(sockfd);
        }

        return -1;
    }
    else if (req->stats == 1)
    {
        if (req->body != NULL) {
            remain = req->bsize - req->bsent;
            if (remain == 0) {
                req->stats = 2;
                return flush_write_buffer(sockfd);
            }

            n = write(sockfd, req->body + req->bsent, remain);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

                return -1;
            }

            req->bsent += n;

            if (req->bsent >= req->bsize) {
                req->stats = 2;
                return flush_write_buffer(sockfd);
            }

            return -1;
        } else if (req->ffd != -1 && req->fsize > 0) {
            remain = req->fsize - req->foffset;
            if (remain == 0) {
                req->stats = 2;
                return flush_write_buffer(sockfd);
            }

            n = sendfile(sockfd, req->ffd, &req->foffset, remain);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;

                return -1;
            }

            if (req->foffset >= req->fsize) {
                req->stats = 2;
                return flush_write_buffer(sockfd);
            }

            return -1;
        } else {
            req->stats = 2;
            return flush_write_buffer(sockfd);
        }
    }
    else if (req->stats == 2)
    {
        int cork = 0;
        setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

        size_t total_bytes = strlen(req->hbuf) + req->bsize + req->fsize;

        access_log(sockfd, req->method, req->path, req->version,
                   req->status_code, total_bytes, req->referer, req->user_agent);

        free_req(req);
        dequeue_req(sockfd);

        if (conn->req_current != NULL)
            return flush_write_buffer(sockfd);

        conn->wpending = 0;

        return 0;
    }

    return -1;
}

// lib/cgi.c
int is_cgi_script(const char *filepath) {
    const char *cgi_extensions[] = {".php", ".py", ".sh", ".pl", NULL};

    for (int i = 0; cgi_extensions[i] != NULL; i++) {
        if (strstr(filepath, cgi_extensions[i])) return 1;
    }

    return 0;
}

int execute_cgi(int sockfd, const char *filepath, const char *method, const char *query_string, char **output, size_t *output_len) {
    int pipe_fd[2];
    pid_t pid;
    ssize_t n;
    char buf[MAXLINE];

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return -1;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return - 1;
    }

    if (pid == 0) {
        close(pipe_fd[0]);

        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }

        close(pipe_fd[1]);

        setenv("REQUEST_METHOD", method, 1);
        if (query_string && strlen(query_string) > 0)
        {
            setenv("QUERY_STRING", query_string, 1);
        }
        setenv("SCRIPT_FILENAME", filepath, 1);
        setenv("SERVER_SOFTWARE", "ArcCore/1.0", 1);

        const char *interpreter = NULL;
        if (strstr(filepath, ".php"))
        {
            interpreter = "/usr/bin/php";
        }
        else if (strstr(filepath, ".py"))
        {
            interpreter = "/usr/bin/python3";
        }
        else if (strstr(filepath, ".sh"))
        {
            interpreter = "/bin/sh";
        }

        char *argv[3];
        if (interpreter)
        {
            argv[0] = (char *)interpreter;
            argv[1] = (char *)filepath;
            argv[2] = NULL;
            execve(interpreter, argv, environ);
        }
        else
        {
            char *argv[] = {(char *)filepath, NULL};
            execve(filepath, argv, environ);
        }

        perror("execve");
        exit(1);
    } else {
        close(pipe_fd[1]);

        *output = (char *)malloc(65536);
        if (!output) {
            close(pipe_fd[0]);
            waitpid(pid, NULL, 0);
            return -1;
        }

        *output_len = 0;
        while( (n = read(pipe_fd[0], buf, sizeof(buf))) > 0 ) {
            if (*output_len + n > 65536) break;

            memcpy(*output + *output_len, buf, n);
            *output_len += n;
        }

        close(pipe_fd[0]);
        waitpid(pid, NULL, 0);

        return 0;
    }
}

// lib/resp_sendfile.c
static int parser_reqline(const char *buf, size_t buflen, char *method, char *path, char *version) {
    const char *end = strstr(buf, "\r\n");
    if (!end || end - buf >= 256)
        return -1;

    size_t line_len = end - buf;
    char line[256];
    memcpy(line, buf, line_len);
    line[line_len] = '\0';

    if (sscanf(line, "%s %s %s", method, path, version) != 3)
        return -1;

    return line_len + 2;
}

static void html_escape(const char *src, char *dst, size_t dstlen) {
    size_t i = 0, j = 0;
    while (src[i] && j < dstlen - 1)
    {
        if (src[i] == '<')
        {
            j += snprintf(dst + j, dstlen - j, "&lt;");
        }
        else if (src[i] == '>')
        {
            j += snprintf(dst + j, dstlen - j, "&gt;");
        }
        else if (src[i] == '&')
        {
            j += snprintf(dst + j, dstlen - j, "&amp;");
        }
        else if (src[i] == '"')
        {
            j += snprintf(dst + j, dstlen - j, "&quot;");
        }
        else
        {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

static int find_head_end(const char *buf, size_t len) {
    for (size_t i = 0; i < len - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i;
        }
    }

    return -1;
}


static char *urldecode(const char *encoded) {
    if (!encoded) return NULL;

    size_t len = strlen(encoded);
    char *decoded = (char *)malloc(len + 1);
    if (!decoded) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len && encoded[i]; i++) {
        if (encoded[i] == '%' && i + 2 < len) {
            int hex_val;
            if (sscanf(encoded + i + 1, "%2x", &hex_val) == 1) {
                decoded[j++] = (char)hex_val;
                i += 2;
            } else {
                decoded[j++] = encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded[j++] = ' ';
        } else {
            decoded[j++] = encoded[i];
        }
    }
    decoded[j] = '\0';

    return decoded;
}

static form_item_t *parse_form_data(const char *data, size_t len) {
    if (!data || len == 0) return NULL;

    form_item_t *head = NULL;
    form_item_t *tail = NULL;

    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, data, len);
    buf[len] = '\0';

    char *saveptr = NULL;
    char *pair = strtok_r(buf, "&", &saveptr);

    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';

            char *key = urldecode(pair);
            char *value = urldecode(eq + 1);

            if (key && value) {
                form_item_t *item = (form_item_t *)malloc(sizeof(form_item_t));
                if (item) {
                    item->key = key;
                    item->value = value;
                    item->next = NULL;

                    if (!head) {
                        head = item;
                        tail = item;
                    } else {
                        tail->next = item;
                        tail = item;
                    }
                } else {
                    free(key);
                    free(value);
                }
            } else {
                if (key) free(key);
                if (value) free(value);
            }
        }
        pair = strtok_r(NULL, "&", &saveptr);
    }

    free(buf);
    return head;
}

void resp_listdir(int sockfd, char *filename) {
    int status_code;
    conn_q *conn = &conns[sockfd];
    request_t *req = create_req();
    if (!req) return ;

    char dirname[256];
    snprintf(dirname, sizeof(dirname), "%s", filename);

    size_t dlen = strlen(dirname);
    if (dlen > 0 && dirname[dlen - 1] == '/')
        dirname[dlen - 1] = '\0';

    char *dirpath = scanner(dirname, &status_code);

    if (!dirpath || status_code != 200) {
        const char msg500[] =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        if (dirpath)
            free(dirpath);
        return;
    }

    DIR *dp = opendir(dirpath);
    if (!dp) {
        const char msg403[] =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg403);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(dirpath);
        return;
    }

    char *html_content = (char *)malloc(65536);
    if (!html_content) {
        closedir(dp);
        free(dirpath);
        free_req(req);
        return;
    }

    size_t html_len = 0;
    const char *html_start =
        "<!DOCTYPE html>\r\n"
        "<html>\r\n"
        "<head>\r\n"
        "<meta charset=\"UTF-8\">\r\n"
        "<title>Directory List</title>\r\n"
        "<style>\r\n"
        "body { font-family: Ping Fang SC, sans-serif; margin: 20px; }\r\n"
        "h1 { color: #333; }\r\n"
        "ul { list-style: none; padding: 0; }\r\n"
        "li { padding: 8px; border-bottom: 1px solid #eee; }\r\n"
        "a { color: #0066cc; text-decoration: none; }\r\n"
        "a:hover { text-decoration: underline; }\r\n"
        "</style>\r\n"
        "</head>\r\n"
        "<body>\r\n";

    html_len += snprintf(html_content + html_len, 65563 - html_len, "%s", html_start);
    html_len += snprintf(html_content + html_len, 65536 - html_len, "<h1>Directory List：/%s</h1>\r\n<ul>\r\n", dirname);
    html_len += snprintf(html_content + html_len, 65536 - html_len, "<li><a href=\"../\">[Parent Directory]</a></li>\r\n");

    struct dirent *ent;
    struct stat st;
    char item_path[512];
    char escaped_name[256];

    while ( (ent = readdir(dp)) != NULL && html_len < 65536 ) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(item_path, sizeof(item_path), "%s/%s", dirpath, ent->d_name);

        if (stat(item_path, &st) != 0) continue;

        html_escape(ent->d_name, escaped_name, sizeof(escaped_name));

        if (S_ISDIR(st.st_mode)) {
            html_len += snprintf(html_content + html_len, 65536 - html_len,
                                 "<li><a href=\"%s/\">📁 %s/</a></li>\r\n",
                                 escaped_name, escaped_name);
        } else if (S_ISREG(st.st_mode)) {
            html_len += snprintf(html_content + html_len, 65536 - html_len,
                                 "<li><a href=\"%s\">📄 %s</a> (%zu byte)</li>\r\n",
                                 escaped_name, escaped_name, st.st_size);
        }
    }

    closedir(dp);
    free(dirpath);

    html_len += snprintf(html_content + html_len, 65536 - html_len, "</ul>\r\n</body>\r\n</html>\r\n");

    snprintf(req->hbuf, sizeof(req->hbuf),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=8\r\n\r\n",
             html_len);

    req->body = html_content;
    req->bsize = html_len;
    req->bsent = 0;
    req->stats = 0;

    int cork = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;
}

void resp_sendfile(int sockfd, char *buf) {
    conn_q *conn = &conns[sockfd];
    request_t *req = conn->req_current;
    char method[32], path[256], version[32];
    char query_string[512] = {0};
    int parser_len;

    if (!req)
    {
        req = create_req();
        if (!req)
            return;
        enqueue_req(sockfd, req);
    }

    parser_len = parser_reqline(buf, MAXLINE, method, path, version);

    strcpy(req->method, method);
    strcpy(req->path, path);
    strcpy(req->version, version);
    extract_http_headers(buf, req);

    if (parser_len < 0) {
        req = create_req();
        if (!req) return;

        const char msg400[] =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg400);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    if (strcmp(method, "POST") == 0 && req->content_length > 0) {
        size_t body_start = req->header_end_pos + 4;
        const char *body_data = buf + body_start;
        size_t body_len = req->content_length;

        char *content_type = strstr(buf, "Content-Type:");
        if (content_type && strstr(content_type, "application/x-www-form-urlencoded"))
        {
            req->form_data = parse_form_data(body_data, body_len);
        }
    }


    if (strcmp(method, "POST") == 0 && req->form_data)
    {
        char html_body[2048] = {0};
        int offset = 0;

        offset += snprintf(html_body + offset, sizeof(html_body) - offset,
                           "<html><body><h1>Received</h1><ul>");

        form_item_t *cur = req->form_data;
        while (cur && offset < sizeof(html_body) - 256)
        {
            offset += snprintf(html_body + offset, sizeof(html_body) - offset,
                               "<li><strong>%s</strong>: %s</li>", cur->key, cur->value);
            cur = cur->next;
        }

        offset += snprintf(html_body + offset, sizeof(html_body) - offset,
                           "</ul></body></html>");

        snprintf(req->hbuf, sizeof(req->hbuf),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html; charset=utf-8\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n\r\n",
                 offset);

        req->body = (char *)malloc(offset + 1);
        if (req->body)
        {
            strcpy(req->body, html_body);
            req->bsize = offset;
            req->bsent = 0;
            req->stats = 0;
        }

        conn->wpending = 1;
        return;
    }

    int status_code;
    char *filename = (*path == '/') ? path + 1 : path;

    char *qmark = strchr(filename, '?');
    if (qmark) {
        snprintf(query_string, sizeof(query_string), "%s", qmark + 1);
        *qmark = '\0';
    }

    size_t flen = strlen(filename);
    if (flen > 0 && filename[flen - 1] == '/') {
        resp_listdir(sockfd, filename);
        return;
    }

    req = create_req();
    if (!req)
    {
        return;
    }

    char *filepath = scanner(filename, &status_code);

    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    if(!filepath) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    if (is_cgi_script(filepath) && status_code == 200) {
        char *cgi_out = NULL;
        size_t cgi_out_len = 0;

        if (execute_cgi(sockfd, filepath, method, query_string, &cgi_out, &cgi_out_len) == 0) {
            snprintf(req->hbuf, sizeof(req->hbuf),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: keep-alive\r\n"
                     "Keep-Alive: timeout=8\r\n\r\n",
                     cgi_out_len);

            req->body = cgi_out;
            req->bsize = cgi_out_len;
            req->bsent = 0;
            req->stats = 0;

            int cork = 1;
            setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

            enqueue_req(sockfd, req);
            conn->wpending = 1;
            free(filepath);
            return;
        } else {
            free(cgi_out);
        }
    }

    int ffd = open(filepath, O_RDONLY);
    if (ffd == -1) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        conn->wpending = 1;
        enqueue_req(sockfd, req);
        free(filepath);
        return;
    }

    struct stat st;
    if(stat(filepath, &st) != 0) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        close(ffd);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(filepath);
        return;
    }

    snprintf(req->hbuf, sizeof(req->hbuf),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "Keep-Alive: timeout=8\r\n\r\n",
             status_code,
             (status_code == 200) ? "OK" : "Not Found",
             st.st_size);

    req->status_code = status_code;
    req->ffd = ffd;
    req->fsize = st.st_size;
    req->hsent = 0;
    req->stats = 0;

    int cork = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;

    free(filepath);
}

// lib/prefork.c
void worker_loop(int worker_id) {
    int listenfd, connfd, sockfd;
    int i, epfd, nready;
    ssize_t n;
    socklen_t clilen;
    char buf[MAXLINE];
    struct epoll_event ev, events[MAXFD];
    struct sockaddr_in servaddr, cliaddr;

    int reuse = 1;
    time_t last_log_flush = 0;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) err_sys("socket error");

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
        err_sys("setsockopt SO_REUSEPORT error");

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        err_sys("setsockopt SO_REUSEADDR error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    if (bind(listenfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
        err_sys("bind error");
    }

    if (listen(listenfd, LISTENQ) < 0) {
        err_sys("listen error");
    }

    if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0) {
        err_sys("fcntl O_NONBLOCK error");
    }

    epfd = epoll_create(MAXFD);
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    fprintf(stderr, "Worker %d started (PID: %d)\n", worker_id, getpid());

    for (;;)
    {
        nready = epoll_wait(epfd, events, MAXFD, KEEPALIVE_TIMEOUT);

        time_t now = time(NULL);
        if (now - last_log_flush >= 1) {
            log_flush_periodic();
            last_log_flush = now;
        }

        for (i = 0; i < nready; i++)
        {
            sockfd = events[i].data.fd;

            if (sockfd == listenfd)
            {
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd, (SA *)&cliaddr, &clilen);

                if (fcntl(connfd, F_SETFL, O_NONBLOCK) < 0)
                    err_sys("fcntl O_NONBLOCK error");

                conn_init(connfd);

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);


            }
            else if (events[i].events & EPOLLIN)
            {
                if ((n = read(sockfd, buf, MAXLINE)) <= 0)
                {
                    request_t *req;
                    while ((req = dequeue_req(sockfd)) != NULL)
                    {
                        free_req(req);
                    }

                    epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                }
                else
                {
                    conn_q *conn = &conns[sockfd];
                    request_t *req = conn->req_current;

                    if (!req) {
                        req = create_req();
                        if (!req) {
                            close(sockfd);
                            continue;
                        }
                        enqueue_req(sockfd, req);
                    }

                    if (req->req_len + n > req->req_cap) {
                        size_t new_cap = req->req_cap * 2;
                        char *new_buf = (char *)realloc(req->req_buf, new_cap);
                        if (!new_buf) {
                            close(sockfd);
                            continue;
                        }
                        req->req_buf = new_buf;
                        req->req_cap = new_cap;
                    }

                    memcpy(req->req_buf + req->req_len, buf, n);
                    req->req_len += n;

                    int header_end = find_head_end(req->req_buf, req->req_len);

                    if (header_end >= 0) {
                        req->header_end_pos = header_end;
                        req->req_parse_state = 2;

                        char *content_len_str = strstr(req->req_buf, "Content-Length:");
                        if (content_len_str)
                        {
                            sscanf(content_len_str, "Content-Length: %d", &req->content_length);
                        }

                        size_t body_start = header_end + 4;
                        size_t body_received = req->req_len - body_start;

                        if (req->content_length == 0 || body_received >= req->content_length)
                        {
                            req->req_parse_state = 3;
                            resp_sendfile(sockfd, req->req_buf);

                            if (conns[sockfd].wpending)
                            {
                                ev.events = EPOLLIN | EPOLLOUT;
                                ev.data.fd = sockfd;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                            }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                int ret = flush_write_buffer(sockfd);

                if (ret == 0)
                {
                    if (conns[sockfd].req_head == NULL) {
                        ev.events = EPOLLIN;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    } else {
                        ev.events = EPOLLIN | EPOLLOUT;
                        ev.data.fd = sockfd;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                    }
                } else if (ret < 0) {
                    ev.events = EPOLLIN | EPOLLOUT;
                    ev.data.fd = sockfd;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
                }
            }
        }
    }

    close(epfd);
    close(listenfd);
}

void master_process(void) {
    pid_t pids[NUM_WORKERS];
    int i;

    for (i = 0; i < NUM_WORKERS; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            err_sys("fork error");
        } else if (pids[i] == 0) {
            worker_loop(i);
            exit(0);
        }
    }

    fprintf(stderr, "Master process (PID: %d) spawned %d workers\n", getpid(), NUM_WORKERS);

    for (;;) {
        pid_t dead_pid = wait(NULL);

        for (i = 0; i < NUM_WORKERS; i++) {
            if (pids[i] == dead_pid) {
                fprintf(stderr, "Worker %d died, respawning...\n", i);
                
                pids[i] = fork();
                if (pids[i] < 0) {
                    err_sys("fork error in respawn");
                } else if (pids[i] == 0) {
                    worker_loop(i);
                    exit(0);
                }
                break;
            }
        }
    }
}

// src/Day15/preforkserv.c
int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    log_init();

    master_process();

    return 0;
}