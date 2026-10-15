/**
 * @file    app_log.c
 * @brief   Timestamped console logging for the demo application.
 */
#include "app_log.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "os.h"

void app_log(const char *tag, const char *fmt, ...)
{
    va_list args;

    (void)printf("[%6" PRIu32 " ms] %-10s ", os_scheduler_get_tick(), tag);

    va_start(args, fmt);
    (void)vprintf(fmt, args);
    va_end(args);

    (void)putchar('\n');
    (void)fflush(stdout);
}
