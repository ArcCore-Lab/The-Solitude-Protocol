#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"

static char *search_in_dir(const char *dir, const char *filename) {
    DIR *dp;
    struct stat st;
    char path[MAXLINE];
    struct dirent *ent;
    char *result = NULL;

    if ( (dp = opendir(dir)) != NULL ) return NULL;

    while (ent = readdir64(dp)) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) 
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (stat(path, &st) != 0) continue;

        if (strcmp(ent->d_name, filename) == 0) {
            result = strdup(path);
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            result = search_in_dir(path, filename);
            if (result) break;
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(ent->d_name, filename) == 0) {
                result = strdup(path);
                break;
            }
        }
    }

    close(dp);
    return result;
}

int sanitize_path(const char *input, char *output, size_t outlen) {
    size_t i = 0, j = 0;
    int last_was_slash = 0;

    while (input[i] && j < outlen - 1) {
        // NULL Byter Injection
        if (input[i] == '\0') break;

        // Duplicate Slashes
        if (input[i] == '/') {
            if (!last_was_slash) {
                output[j++] = '/';
                last_was_slash = 1;
            }
            i++;
            continue;
        }

        // ../ Path Traversal
        if (input[i] == '.' && input[i+1] == '.' && 
            (input[i+2] == '/' ||  input[i+2] == '\0')) {
                return -1;
        }

        // URL Encoding
        if (input[i] == '%') {
            // Encoding Bypass
            if ((input[i+1] == '2' && input[i+2] == 'e') ||
                (input[i+1] == '2' && input[i+3] == 'E')) {
                return -1;
            }

            // NULL Byte
            if (input[i+1] == '0' && input[i+2] == '0') {
                return -1;
            }
        }

        output[j++] = input[i++];
        last_was_slash = 0;
    }

    output[j] = '\0';
    return 0;
}

static int validate_symlink(const char *filepath, const char *sandbox_root) {
    char link_target[PATH_MAX];
    char resolved[PATH_MAX];
    struct stat st;

    int link_depth = 0;
    const int MAX_LINK_DEPTH = 8;

    if (lstat(filepath, &st) != 0) return 0;

    if (!S_ISLNK(st.st_mode)) return 0;

    ssize_t len = readlink(filepath, link_target, sizeof(link_target) - 1);
    if (len < 0) return -1;
    link_target[len] = '\0';

    if (link_target[0] != '/') {
        char dir[PATH_MAX];
        strncpy(dir, filepath, sizeof(dir) - 1);
        char *last_slash = strchr(dir, '/');
        if (last_slash) {
            *(last_slash + 1) = '\0';
            snprintf(resolved, sizeof(resolved), "%s%s", dir, link_target);
        } else {
            strncpy(resolved, link_target, sizeof(resolved) - 1);
        }
    } else {
        strncpy(resolved, link_target, sizeof(resolved) - 1);
    }

    char real_target[PATH_MAX];
    if (realpath(resolved, real_target) == NULL) return -1;

    if (strncmp(real_target, sandbox_root, strlen(sandbox_root)) != 0)
        return -1;

    if (++link_depth > MAX_LINK_DEPTH) return -1;

    return validate_symlink(real_target, sandbox_root);
}


char *ffind(const char *filename, int *status_code) {
    char sanitized[MAXLINE];
    char filepath[MAXLINE];
    char realpath_buf[MAXLINE];
    char static_realpath[MAXLINE];
    struct stat st;

    char *path404 = "../static/404.html";

    if (sanitize_path(filename, sanitized, sizeof(sanitized)) != 0) {
        *status_code = 400;
        return strdup(path404);
    }

    if (g_is_chrooted) {
        snprintf(filepath, sizeof(filepath), "/%s", sanitized);
    } else {
        snprintf(filepath, sizeof(filepath), "../static/%s", sanitized);
    }

    if (realpath(filepath, realpath_buf) == NULL) {
        *status_code = 404;
        return strdup(path404);
    }

    if (g_is_chrooted) {
        if (realpath("/", static_realpath) == NULL) {
            *status_code = 500;
            return strdup(path404);
        }
    } else {
        if (realpath("../static", static_realpath) == NULL) {
            *status_code = 500;
            return strdup(path404);
        }
    }

    if (strncmp(realpath_buf, static_realpath, strlen(static_realpath)) != 0) {
        *status_code = 403;
        return strdup(path404);
    }

    if (validate_symlink(realpath_buf, static_realpath) != 0) {
        *status_code = 403;
        return strdup(path404);
    }

    if (stat(realpath_buf, &st) == 0) {
        *status_code = 200;
        return strdup(realpath_buf);
    }
    
    // filepath = search_in_dir("../static", filename);
    // if (filepath) {
    //     *status_code = 200;
    //     return filepath;
    // }

    if (stat(path404, &st) == 0) {
        *status_code = 404;
        return strdup(path404);
    }

    *status_code = 500;

    return strdup(path404);
}