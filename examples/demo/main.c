/**
 * @file    main.c
 * @brief   Demo: LED blink, sensor producer, event-driven logger, supervisor.
 *
 *  task        priority  period     role
 *  ----------  --------  ---------  -------------------------------------------
 *  supervisor  4         250 ms     runs the scenario (suspend/resume/delete)
 *  blink       3         500 ms     toggles the LED
 *  sensor      2         1000 ms    captures a 16-sample frame from the pool
 *  logger      1         event      woken by the queue, summarises and frees it
 *
 * Frames travel zero-copy: the queue carries pointers to memory-pool blocks.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_blink.h"
#include "app_logger.h"
#include "app_messages.h"
#include "app_sensor.h"
#include "app_supervisor.h"
#include "bsp_led.h"
#include "bsp_temp_sensor.h"
#include "os.h"

#define FRAME_POOL_BLOCKS     4U
#define FRAME_QUEUE_CAPACITY  FRAME_POOL_BLOCKS /* Never more frames in flight than blocks. */

static OS_MEMPOOL_DECLARE_STORAGE(s_frame_pool_storage, sizeof(sensor_frame_t),
                                  FRAME_POOL_BLOCKS);
static sensor_frame_t      *s_frame_queue_storage[FRAME_QUEUE_CAPACITY];
static app_sensor_ctx_t     s_sensor_ctx;
static app_logger_ctx_t     s_logger_ctx;
static app_supervisor_ctx_t s_supervisor_ctx;

static const char *state_to_string(os_task_state_t state)
{
    switch (state) {
    case OS_TASK_STATE_READY:
        return "READY";
    case OS_TASK_STATE_RUNNING:
        return "RUNNING";
    case OS_TASK_STATE_WAITING:
        return "WAITING";
    case OS_TASK_STATE_SUSPENDED:
        return "SUSPENDED";
    default:
        return "?";
    }
}

static bool create_task(const char *name, os_task_fn_t fn, void *arg, os_priority_t priority,
                        os_tick_t period_ms, os_task_handle_t *out_handle)
{
    const os_task_config_t config = {
        .name           = name,
        .fn             = fn,
        .arg            = arg,
        .priority       = priority,
        .period_ms      = period_ms,
        .start_delay_ms = 0U,
    };

    const os_status_t status = os_task_create(&config, out_handle);
    if (status != OS_OK) {
        (void)fprintf(stderr, "cannot create task '%s': %s\n", name, os_status_to_string(status));
        return false;
    }
    return true;
}

static void print_task_stats(const char *label, os_task_handle_t task)
{
    os_task_info_t info;

    if (os_task_get_info(task, &info) != OS_OK) {
        (void)printf("  %-10s  (deleted)\n", label);
        return;
    }
    (void)printf("  %-10s  %-9s  prio=%u  period=%5" PRIu32 " ms  runs=%3" PRIu32
                 "  overruns=%" PRIu32 "  max_exec=%" PRIu32 " ms\n",
                 info.name, state_to_string(info.state), (unsigned int)info.priority,
                 info.period_ms, info.run_count, info.overrun_count, info.max_exec_ms);
}

static void print_pool_stats(const char *label, os_mempool_handle_t pool)
{
    os_mempool_info_t info;

    if (os_mempool_get_info(pool, &info) != OS_OK) {
        return;
    }
    (void)printf("  %-10s  %lu blocks x %lu bytes  free=%lu  min_free=%lu\n", label,
                 (unsigned long)info.block_count, (unsigned long)info.block_size,
                 (unsigned long)info.free_count, (unsigned long)info.min_free_count);
}

int main(void)
{
    os_task_handle_t    supervisor_task = NULL;
    os_task_handle_t    blink_task      = NULL;
    os_task_handle_t    sensor_task     = NULL;
    os_task_handle_t    logger_task     = NULL;
    os_queue_handle_t   frame_queue     = NULL;
    os_mempool_handle_t frame_pool      = NULL;

    (void)os_scheduler_init();
    bsp_led_init();
    bsp_temp_sensor_init();

    if ((os_mempool_create(s_frame_pool_storage, sizeof(s_frame_pool_storage),
                           sizeof(sensor_frame_t), &frame_pool) != OS_OK) ||
        (os_queue_create(s_frame_queue_storage, sizeof(sensor_frame_t *), FRAME_QUEUE_CAPACITY,
                         &frame_queue) != OS_OK)) {
        (void)fprintf(stderr, "cannot create frame pool or queue\n");
        return EXIT_FAILURE;
    }

    s_sensor_ctx.frame_pool = frame_pool;
    s_sensor_ctx.out_queue  = frame_queue;
    s_logger_ctx.frame_pool = frame_pool;
    s_logger_ctx.in_queue   = frame_queue;

    const bool created =
        create_task("supervisor", app_supervisor_task, &s_supervisor_ctx, 4U, 250U,
                    &supervisor_task) &&
        create_task("blink", app_blink_task, NULL, 3U, 500U, &blink_task) &&
        create_task("sensor", app_sensor_task, &s_sensor_ctx, 2U, 1000U, &sensor_task) &&
        create_task("logger", app_logger_task, &s_logger_ctx, 1U, OS_TASK_PERIOD_EVENT,
                    &logger_task);
    if (!created) {
        return EXIT_FAILURE;
    }

    s_supervisor_ctx.blink_task  = blink_task;
    s_supervisor_ctx.sensor_task = sensor_task;
    s_supervisor_ctx.logger_task = logger_task;
    (void)os_queue_set_receiver(frame_queue, logger_task);

    (void)printf("Mini RTOS v%d.%d.%d demo - cooperative priority scheduler\n\n",
                 OS_VERSION_MAJOR, OS_VERSION_MINOR, OS_VERSION_PATCH);

    os_scheduler_start();

    (void)printf("\nTask statistics:\n");
    print_task_stats("supervisor", supervisor_task);
    print_task_stats("blink", blink_task);
    print_task_stats("sensor", sensor_task);
    print_task_stats("logger", logger_task);

    (void)printf("\nMemory pools:\n");
    print_pool_stats("frames", frame_pool);

    (void)printf("\nFrames received: %lu, dropped: %lu\n", (unsigned long)s_logger_ctx.received,
                 (unsigned long)s_sensor_ctx.dropped);

    return EXIT_SUCCESS;
}
