/**
 * @file    app_supervisor.h
 * @brief   Periodic task driving the demo scenario (suspend, resume, delete, stop).
 */
#ifndef APP_SUPERVISOR_H
#define APP_SUPERVISOR_H

#include <stddef.h>

#include "os.h"

typedef struct {
    os_task_handle_t blink_task;
    os_task_handle_t sensor_task;
    os_task_handle_t logger_task;
    size_t           next_step; /**< Index of the next scenario step to execute. */
} app_supervisor_ctx_t;

/** Task callback. @p arg is an app_supervisor_ctx_t. */
void app_supervisor_task(void *arg);

#endif /* APP_SUPERVISOR_H */
