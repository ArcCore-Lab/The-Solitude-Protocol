#ifndef _TSP_H
#define _TSP_H

#include "include/unp.h"

void err_sys(const char *fmt, ...);

void err_quit(const char *fmt, ...);

/*
    Parse HTTP request from sockfd.
    Returns a dynamically allocated string containing the requested path.
    Caller is responsible for freeing the returned string.
*/
char *parser(int sockfd);

/*
    Read a line from a descriptor.
    Returns the number of bytes read, 0 on EOF, or -1 on error.
*/
ssize_t Readline(int fd, void *vptr, size_t maxlen);

/*
    Scan for the file with the given filename in the static directory.
    If found, returns a dynamically allocated string containing the file content
    and sets status_code to 200.
    If not found, returns the content of 404.html and sets status_code to 404.
    Caller is responsible for freeing the returned string.
*/
char *scanner(const char *filename, int *status_code);

/*
    Send HTTP response to sockfd.
    The response includes the status line, headers, and body.
*/
void response(int sockfd);

#endif /* _TSP_H */