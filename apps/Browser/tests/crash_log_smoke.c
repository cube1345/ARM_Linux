#include "browser_log.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief 安装崩溃日志 handler 并主动触发 SIGABRT。 */
int main(int argc, char **argv)
{
    if (argc != 2 || browser_log_install_crash_handler(argv[1]) < 0) {
        fprintf(stderr, "FAIL install crash handler\n");
        return EXIT_FAILURE;
    }
    (void)raise(SIGABRT);
    return EXIT_FAILURE;
}
