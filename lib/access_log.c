#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"
#include "tspmalloc.h"

static log_buffer_t g_log_buf = {
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

static int log_open(void) {
    snprintf(g_log_buf.logfile, sizeof(g_log_buf.logfile), "%s/access.log", LOG_DIR);

    g_log_buf.logfd = open(g_log_buf.logfile, O_CREAT | O_WRONLY | O_APPEND, 0644);
    
    if (g_log_buf.logfd == -1) {
        perror("open access.log");
        return -1;
    }

    struct stat st;
    if (stat(g_log_buf.logfile, &st) == 0)
        g_log_buf.file_size = st.st_size;

    return 0;
}

void log_init(void) {
    mkdir(LOG_DIR, 0755);
    pthread_mutex_init(&g_log_buf.mutex, NULL);
    log_open();
}

static void log_flush(void) {
    if (g_log_buf.offset == 0 || g_log_buf.logfd == -1)
        return;

    ssize_t written = write(g_log_buf.logfd, g_log_buf.buf, g_log_buf.offset);
    if (written > 0) {
        g_log_buf.file_size += written;
        g_log_buf.offset = 0;

        if (g_log_buf.file_size >= LOG_MAX_SIZE) {
            log_rotate();
            log_open();
        }
    }
}

static void log_write_line(const char *line) {
    pthread_mutex_lock(&g_log_buf.mutex);

    size_t len = strlen(line);

    if (g_log_buf.offset + len > LOG_BUFFER_SIZE)  {
        log_flush();
    }

    if (len <= LOG_BUFFER_SIZE - g_log_buf.offset) {
        memcpy(g_log_buf.buf + g_log_buf.offset, line, len);
        g_log_buf.offset += len;
    }

    pthread_mutex_unlock(&g_log_buf.mutex);
}

static void format_time(char *buf, size_t buflen) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

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

static void get_client_ip(int sockfd, char *ip, size_t iplen) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(sockfd, (SA *)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, ip, iplen);
    } else {
        strncpy(ip, "-", iplen - 1);
    }
}

void access_log(int sockfd, const char *method, const char *path, const char *version,
               int status_code, size_t bytes_sent, const char *referer, const char *user_agent) {

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

void log_flush_periodic(void) {
    pthread_mutex_lock(&g_log_buf.mutex);
    log_flush();
    pthread_mutex_unlock(&g_log_buf.mutex);
}