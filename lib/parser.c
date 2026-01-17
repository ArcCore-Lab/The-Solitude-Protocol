#include "include/unp.h"
#include "tsp.h"

char *parser(int sockfd) {
    char line[MAXLINE];
    char *ptr, *space, *path;
    size_t n;

    for (ptr = line; ptr < &line[MAXLINE - 1]; ptr++) {
        if ( (n = read(sockfd, ptr, 1)) != 1 ) break;

        if (*ptr == "\n") {
            ptr++;
            break;
        }
    }
    *ptr = '\0';

    ptr = strchr(line, ' ');
    if (!ptr) return strdup("");
    ptr++;

    space = strchr(ptr, ' ');
    if (space) *space = '\0';

    path = strdup(ptr);

    return path;
}