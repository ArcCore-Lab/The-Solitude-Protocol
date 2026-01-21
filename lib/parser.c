#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"

/*
    This function reads a line from the socket and parses the request path.
    It returns the extracted path as a dynamically allocated string.
*/
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

/*
    This function parses the request line into method, path, and version.
    It's used in duplex communication to handle HTTP requests.
    Returns the length of the parsed line or -1 on error. 
*/
int parser_reqline(const char *buf, size_t buflen, char *method, char *path, char *version) {
    char line[256];
    size_t line_len;

    const char *end = strstr(buf, "\r\n");

    if (!end || end - buf >= 256) return -1;

    line_len = end - buf;
    memcpy(line, buf, line_len);

    line[line_len] = '\0';

    if (sscanf(line, "%s %s %s", method, path, version) != 3)
        return -1;

    return line_len + 2;
}