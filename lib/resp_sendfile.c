#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"

static int parser_reqline(const char *buf, size_t buflen, char *method, char *path, char *version) {
    char line[256];

    const char *end = strstr(buf, "\r\n");
    if (!end || end - buf >= 256)
        return -1;

    size_t line_len = end - buf;
    memcpy(line, buf, line_len);
    line[line_len - 1] = '\0';

    if (sscanf(line, "%s %s %s", method, path, version) != 3)
        return -1;

    return line_len + 2;
}

static void html_escape(const char *src, char *dst, size_t dstlen) {
    size_t i = 0, j = 0;
    while (src[i] && j < dstlen - 1) {
        if (src[i] == '<') {
            j += snprintf(dst + j, dstlen - j, "&lt");
        } else if (src[i] == '>') {
            j += snprintf(dst + j, dstlen - j, "&gt;");
        } else if (src[i] == '&') {
            j += snprintf(dst + j, dstlen - j, "&amp;");
        } else if (src[i] == '"') {
            j += snprintf(dst + j, dstlen - j, "&quot;");
        } else {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

void resp_listdir(int sockfd, char *filename) {
    DIR *dp;
    size_t dlen;
    int status_code;
    char *dirpath;
    char dirname[256];
    char item_path[512];
    char escaped_name[256];
    struct dirent *ent;
    struct stat st;

    int cork = 1;
    size_t html_len = 0;
    conn_q *conn = &conq[sockfd];
    request_t *req = create_req();
    char *html_content = (char *)tspmalloc(&pool, 65536);

    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    const char msg403[] =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
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

    if (!req) return;

    snprintf(dirname, sizeof(dirname), "%s", filename);

    dlen = strlen(dirname);
    if (dlen > 0 && dirname[dlen - 1] == '/')
        dirname[dlen - 1] = '\0';

    dirpath = ffind(dirname, &status_code);
    if (!dirpath || status_code != 200) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;

        if (dirpath) free(dirpath);

        return;
    }

    dp = opendir(dirpath);
    if (!dp) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg403);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(dirpath);
        return;
    }

    if (!html_content) {
        closedir(dp);
        free(dirpath);
        free_req(req);
        return;
    }

    html_len += snprintf(html_content + html_len, 65563 - html_len, "%s", html_start);
    html_len += snprintf(html_content + html_len, 65536 - html_len, "<h1>Directory List：/%s</h1>\r\n<ul>\r\n", dirname);
    html_len += snprintf(html_content + html_len, 65536 - html_len, "<li><a href=\"../\">[Parent Directory]</a></li>\r\n");

    while ( (ent = readdir64(dp)) != NULL && html_len < 65000) {
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

    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;
}

void resp_sendfile(int sockfd, char *buf) {
    size_t flen;
    int parser_len;
    int ffd, status_code;
    struct stat st;
    request_t *req;
    char *filename, *filepath, *qmark;
    char method[32], path[256], version[32];

    int cork = 1;
    char query_string[512] = {0};
    conn_q *conn = &conq[sockfd];

    const char msg400[] =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    const char msg500[] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    parser_len = parser_reqline(buf, MAXLINE, method, path, version);
    if (parser_len < 0) {
        req = create_req();
        if (!req) return;

        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg400);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    filename = (*path == '/') ? path + 1 : path;

    qmark = strchr(filename, '?');
    if (qmark) {
        snprintf(query_string, sizeof(query_string), "%s", qmark + 1);
        *qmark = '\0';
    }

    flen = strlen(filename);
    if (flen > 0 && filename[flen - 1] == '/') {
        resp_listdir(sockfd, filename);
        return;
    }

    req = create_req();
    if (!req) return;

    filepath = ffind(filename, &status_code);
    if (!filepath) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        return;
    }

    if(is_cgi_script(filepath) && status_code == 200) {
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

            setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

            enqueue_req(sockfd, req);
            conn->wpending = 1;

            free(filepath);
            return;
        } else {
            tspfree(&pool, cgi_out);
        }
    }

    if ( (ffd = open(filepath, O_RDONLY)) == -1 ) {
        snprintf(req->hbuf, sizeof(req->hbuf), "%s", msg500);
        enqueue_req(sockfd, req);
        conn->wpending = 1;
        free(filepath);
        return;
    }

    if (stat(filepath, &st) != 0) {
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

    req->ffd = ffd;
    req->fsize = st.st_size;
    req->hsent = 0;
    req->stats = 0;

    setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));

    enqueue_req(sockfd, req);
    conn->wpending = 1;

    free(filepath);
}