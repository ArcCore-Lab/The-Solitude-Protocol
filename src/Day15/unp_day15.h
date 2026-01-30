/* Adopted and modified from W. Richard Stevens, "UNIX Network Programming, Vol. 1" */
#ifndef _UNP_H
#define _UNP_H

#define _GNU_SOURCE

#define SERV_PORT 9877
#define MAXLINE 4096
#define MAX_FILE_SIZE 4096

#define NUM_WORKERS 4

#define MAXFD 65536
#define INFTIM (-1)
#define KEEPALIVE_TIMEOUT 8000

#define BUF_8KB 8192
#define BUF_MAX 16384

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
#include <sys/sendfile.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
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

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <poll.h>

#define SA struct sockaddr

#define LISTENQ 1024

#define max(a,b) ((a) < (b) ? (a) : (b))

#define LOG_BUFFER_SIZE 65536
#define LOG_MAX_SIZE (100 * 1024 * 1024)
#define LOG_DIR "../log"

#endif /*_UNP_H*/