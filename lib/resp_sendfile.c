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

int find_head_end(const char *buf, size_t len) {
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
    char *decoded = (char *)tspmalloc(&pool, len + 1);
    if (!decoded) return NULL;

    size_t j = 0;
    for(size_t i = 0; i < len && encoded[i]; i++) {
        if (encoded[i]== '%' && i + 2 < len) {
            int hex_val;
            if (sscanf(encoded[i] + i + 1, "%2x", &hex_val) == 1) {
                decoded[j++] = (char *)hex_val;
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

    char *buf = (char *)tspmalloc(&pool, len + 1);
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
                form_item_t *item = (form_item_t *)tspmalloc(&pool, sizeof(form_item_t));
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
                    tspfree(&pool, key);
                    tspfree(&pool, value);
                }
            } else {
                if (key) tspfree(&pool, key);
                if (value) tspfree(&pool, value);
            }
        }
        pair = strtok_r(NULL, "&", &saveptr);
    }

    tspfree(&pool, buf);

    return head;
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

    if (strcmp(method, "POST") == 0 && req->content_length > 0) {
        size_t body_start = req->header_end_pos + 4;
        const char *body_data = buf + body_start;
        size_t body_len = req->content_length;

        char *content_type = strstr(buf, "Content-Type:");
        if (content_type && strstr(content_type, "application/x-www-form-urlencoded")) {
            req->form_data = parse_form_data(body_data, body_len);
        }
    }

    if (strcmp(method, "POST") == 0 && req->form_data) {
        char html_body[2048] = {0};
        int offset = 0;

        offset += snprintf(html_body + offset, sizeof(html_body) - offset,
                           "<html><body><h1>Received</h1><ul>");

        form_item_t *cur = req->form_data;
        while (cur && offset < sizeof(html_body) - 256) {
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

        req->body = (char *)tspmalloc(&pool, offset + 1);
        if (req->body) {
            strcpy(req->body, html_body);
            req->bsize = offset;
            req->bsent = 0;
            req->stats = 0;
        }

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