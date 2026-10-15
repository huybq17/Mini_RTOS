/**
 * @file    os_port_host.c
 * @brief   Port for a desktop process (Linux, macOS, Windows).
 *
 * The kernel runs in a single thread here, so critical sections only track
 * their nesting to catch unbalanced enter/exit pairs. On a microcontroller
 * they would mask interrupts instead.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#endif

#include "os_port.h"

#include "os_config.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#else
#include <time.h>
#endif

static unsigned int s_critical_nesting;

#if defined(_WIN32)

static LARGE_INTEGER s_frequency;
static LARGE_INTEGER s_epoch;

void os_port_init(void)
{
    /* Sleep() granularity defaults to ~15.6 ms; request 1 ms for the process lifetime. */
    (void)timeBeginPeriod(1U);
    (void)QueryPerformanceFrequency(&s_frequency);
    (void)QueryPerformanceCounter(&s_epoch);
    s_critical_nesting = 0U;
}

os_tick_t os_port_get_tick(void)
{
    LARGE_INTEGER now;

    (void)QueryPerformanceCounter(&now);
    return (os_tick_t)(((now.QuadPart - s_epoch.QuadPart) * 1000LL) / s_frequency.QuadPart);
}

void os_port_idle(os_tick_t max_sleep_ms)
{
    Sleep((DWORD)max_sleep_ms);
}

#else /* POSIX */

static struct timespec s_epoch;

void os_port_init(void)
{
    (void)clock_gettime(CLOCK_MONOTONIC, &s_epoch);
    s_critical_nesting = 0U;
}

os_tick_t os_port_get_tick(void)
{
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);

    const int64_t ms = ((int64_t)(now.tv_sec - s_epoch.tv_sec) * 1000) +
                       ((int64_t)(now.tv_nsec - s_epoch.tv_nsec) / 1000000);
    return (os_tick_t)ms;
}

void os_port_idle(os_tick_t max_sleep_ms)
{
    struct timespec duration;

    duration.tv_sec  = (time_t)(max_sleep_ms / 1000U);
    duration.tv_nsec = (long)(max_sleep_ms % 1000U) * 1000000L;
    (void)nanosleep(&duration, NULL);
}

#endif /* _WIN32 */

void os_port_enter_critical(void)
{
    s_critical_nesting++;
}

void os_port_exit_critical(void)
{
    OS_ASSERT(s_critical_nesting > 0U);
    s_critical_nesting--;
}
