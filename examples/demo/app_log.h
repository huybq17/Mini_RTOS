/**
 * @file    app_log.h
 * @brief   Timestamped console logging for the demo application.
 */
#ifndef APP_LOG_H
#define APP_LOG_H

#if defined(__GNUC__)
#define APP_LOG_PRINTF_FORMAT(fmt_index, args_index) \
    __attribute__((format(printf, fmt_index, args_index)))
#else
#define APP_LOG_PRINTF_FORMAT(fmt_index, args_index)
#endif

/** Print "[  tick ms] TAG      message". */
void app_log(const char *tag, const char *fmt, ...) APP_LOG_PRINTF_FORMAT(2, 3);

#endif /* APP_LOG_H */
