/* Adopted and modified from W. Richard Stevens, "UNIX Network Programming, Vol. 1" */
#ifndef _UNP_H
#define _UNP_H

#define _GNU_SOURCE

#define SERV_PORT 8080
#define MAX_FILE_SIZE 4096
#define MAXLINE 4096
#define LISTENQ 1024
#define MAXFD 65536
#define INFTIM (-1)
#define KEEPALIVE_TIMEOUT 8000

#define SA struct sockaddr

#define max(a, b) ((a) < (b) ? (a) : (b))

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>
#include <stddef.h>

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <poll.h>

#endif /*_UNP_H*/