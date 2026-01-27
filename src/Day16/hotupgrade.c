/*
    (1)$ ./hotupgrade
    (2)$ killall -HUP hotupgrade / kill -HUP $(pgrep hotupgrade | head -1)
    (1)$ killall -TERM preforkserv
*/

#include "include/unp.h"
#include "lib/tsp.h"
#include "lib/atsp.h"
#include "lib/tspmalloc.h"

int main(int argc, char **argv) {
    int listenfd;
    const char *listen_fd_env;

    if (argv[0][0] == '/') {
        strncpy(g_program_path, argv[0], PATH_MAX - 1);
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            snprintf(g_program_path, PATH_MAX, "%s/%s", cwd, argv[0]);
        } else {
            strncpy(g_program_path, argv[0], PATH_MAX - 1);
        }
    }

    for (int i = 0; i < argc; i++) {
        g_argv_copy[i] = argv[i];
    }
    g_argv_copy[argc] = NULL;

    signal(SIGPIPE, SIG_IGN);

    log_init();
    tsp_init(&pool);

    listen_fd_env = getenv(LISTEN_FD_ENV);
    if (listen_fd_env != NULL) {
        listenfd = atoi(listen_fd_env);
        fprintf(stderr, "Master recovered from reload with LISTEN_FD=%d\n", listenfd);

        unsetenv(LISTEN_FD_ENV);
    } else {
        listenfd = master_create_listenfd();
    }

    master_process(listenfd);

    tsp_destroy(&pool);

    return 0;
}