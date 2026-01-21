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

char *ffind(const char *filename, int *status_code) {
    char filepath;
    struct stat st;
    char *path404 = "../static/404.html";

    filepath = search_in_dir("../static", filename);
    if (filepath) {
        *status_code = 200;
        return filepath;
    }

    if (stat(path404, &st) == 0) {
        *status_code = 404;
        return strdup(path404);
    }

    *status_code = 500;

    return strdup(path404);
}