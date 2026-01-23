#include "include/unp.h"
#include "tsp.h"
#include "atsp.h"

int is_cgi_script(const char *filepath) {
    const char *cgi_extens[] = {".php", ".py", ".sh", ".pl", NULL};

    for (int i = 0; cgi_extens[i] != NULL; i++) {
        if (strstr(filepath, cgi_extens[i])) return 1;
    }

    return 0;
}

int execute_cgi(int sockfd, const char *filepath, const char *method, 
    const char *query_string, char **output, size_t *output_len) {

    int fd[2];
    pid_t pid;
    ssize_t n;
    char *argv[3];
    char buf[MAXLINE];

    const char *interpreter = NULL;

    if (pipe(fd) == -1) {
        perror("pipe");
        return -1;
    }

    if ( (pid = fork()) == -1 ) {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    if (pid == 0) {
        close(fd[0]);

        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }

        close(fd[1]);

        setenv("REQUEST_METHOD", method, 1);
        if (query_string && strlen(query_string) > 0)
        {
            setenv("QUERY_STRING", query_string, 1);
        }
        setenv("SCRIPT_FILENAME", filepath, 1);
        setenv("SERVER_SOFTWARE", "ArcCore/1.0", 1);

        if (strstr(filepath, ".php")) {
            interpreter = "/usr/bin/php";
        } else if (strstr(filepath, ".py")) {
            interpreter = "/usr/bin/python3";
        } else if (strstr(filepath, ".sh")) {
            interpreter = "/bin/sh";
        }

        if (interpreter) {
            argv[0] = (char *)interpreter;
            argv[1] = (char *)filepath;
            argv[2] = NULL;
            execve(interpreter, argv, environ);
        } else {
            char *argv[] = {(char *)filepath, NULL};
            execve(filepath, argv, environ);
        }

        perror("execve");
        exit(1);
    } else {
        close(fd[1]);

        *output = (char *)tspmalloc(&pool, 65536);
        if (!output) {
            close(fd[0]);
            waitpid(pid, NULL, 0);
            return -1;
        }

        *output_len = 0;
        while( (n = read(fd[0], buf, sizeof(buf))) > 0 ) {
            if (*output_len + n > 65536) break;

            memcpy(*output + *output_len, buf, n);
            *output_len += n;
        }

        close(fd[0]);
        waitpid(pid, NULL, 0);

        return 0;
    }
}