/* Adopted and modified from W. Richard Stevens, "UNIX Network Programming, Vol. 1" */
#ifndef _UNP_H
#define _UNP_H

#define SERV_PORT 8080
#define MAXLINE 4096

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/wait.h>

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

#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#define SA struct sockaddr

#define LISTENQ 1024


#endif /*_UNP_H*/