#include "include/unp.h"
#include "lib/tsp.h"
#include "lib/atsp.h"
#include "lib/tspmalloc.h"

int main(int arcg, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    log_init();
    tsp_init(&pool);

    master_process();

    tsp_destroy(&pool);

    return 0;
}