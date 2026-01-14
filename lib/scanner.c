#include "include/unp.h"

static char *search_in_dir(const char *dir, const char *filename) {
    DIR *dp;
    int fd;
    size_t size, n;
    struct stat st;
    struct dirent *ent;
    char path[MAXLINE];
    char *result = NULL;

    if ( (dp = opendir(dir)) == NULL) return NULL;

    while ((ent = readdir(dp))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            result = search_in_dir(path, filename);
            if (result) break;
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(ent->d_name, filename) == 0) {
                if ( (fd = open(path, O_RDONLY)) == -1 ) break;

                size = (size_t)st.st_size;
                if(size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;

                result = malloc(size + 1);
                if (result) {
                    if ( (n = read(fd, result, size)) > 0 ) {
                        result[n] = '\0';
                    } else {
                        free(result);
                        result = NULL;
                    }
                }

                close(fd);
                break;
            }
        }
    }

    closedir(dp);
    return result;
}

char *scanner(const char *filename, int *status_code) {
    int fd;
    size_t size, n;
    struct stat st;
    char *content;

    const char *path404 = "../static/404.html";

    content = search_in_dir("../static", filename);
    if (content) {
        *status_code = 200;
        return content;
    }

    if (stat(path404, &st) != 0) {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    if ( (fd = open(path404, O_RDONLY)) == -1) {
        *status_code = 404;
        return strdup("Oops! It seems developer made a little mistake!");
    }

    size = (size_t)st.st_size;
    if (size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;
    
    content = malloc(size + 1);
    if (content) {
        if ( (n = read(fd, content, size)) > 0 ) {
            content[n] = '\0';
        } else {
            free(content);
            content = strdup("Oops! It seems developer made a little mistake!");
        }
    } else {
        content = strdup("Oops! It seems developer made a little mistake!");
    }

    close(fd);
    *status_code = 404;

    return content;
}