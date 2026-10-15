/**
 * @file    app_supervisor.c
 * @brief   Periodic task driving the demo scenario (suspend, resume, delete, stop).
 */
#include "app_supervisor.h"

#include "app_log.h"

typedef enum {
    SUPERVISOR_ACTION_SUSPEND_LOGGER,
    SUPERVISOR_ACTION_SUSPEND_BLINK,
    SUPERVISOR_ACTION_RESUME_BLINK,
    SUPERVISOR_ACTION_RESUME_LOGGER,
    SUPERVISOR_ACTION_DELETE_SENSOR,
    SUPERVISOR_ACTION_STOP_SCHEDULER
} supervisor_action_t;

typedef struct {
    os_tick_t           at_ms;
    supervisor_action_t action;
} supervisor_step_t;

/*
 * Scenario, ordered by time. While the logger is suspended, frames pile up in
 * the queue until the memory pool runs dry and the sensor starts dropping.
 */
static const supervisor_step_t k_scenario[] = {
    { 2000U, SUPERVISOR_ACTION_SUSPEND_LOGGER },
    { 3000U, SUPERVISOR_ACTION_SUSPEND_BLINK },
    { 5000U, SUPERVISOR_ACTION_RESUME_BLINK },
    { 6500U, SUPERVISOR_ACTION_RESUME_LOGGER },
    { 7000U, SUPERVISOR_ACTION_DELETE_SENSOR },
    { 8000U, SUPERVISOR_ACTION_STOP_SCHEDULER },
};

#define SCENARIO_LENGTH (sizeof(k_scenario) / sizeof(k_scenario[0]))

static void supervisor_execute(app_supervisor_ctx_t *ctx, supervisor_action_t action)
{
    os_status_t status = OS_OK;

    switch (action) {
    case SUPERVISOR_ACTION_SUSPEND_LOGGER:
        status = os_task_suspend(ctx->logger_task);
        app_log("SUPERVISOR", "suspend logger task -> %s", os_status_to_string(status));
        break;
    case SUPERVISOR_ACTION_RESUME_LOGGER:
        status = os_task_resume(ctx->logger_task);
        app_log("SUPERVISOR", "resume logger task -> %s", os_status_to_string(status));
        break;
    case SUPERVISOR_ACTION_SUSPEND_BLINK:
        status = os_task_suspend(ctx->blink_task);
        app_log("SUPERVISOR", "suspend blink task -> %s", os_status_to_string(status));
        break;
    case SUPERVISOR_ACTION_RESUME_BLINK:
        status = os_task_resume(ctx->blink_task);
        app_log("SUPERVISOR", "resume blink task -> %s", os_status_to_string(status));
        break;
    case SUPERVISOR_ACTION_DELETE_SENSOR:
        status = os_task_delete(ctx->sensor_task);
        ctx->sensor_task = NULL; /* The handle is stale from now on. */
        app_log("SUPERVISOR", "delete sensor task -> %s", os_status_to_string(status));
        break;
    case SUPERVISOR_ACTION_STOP_SCHEDULER:
        app_log("SUPERVISOR", "stop scheduler");
        os_scheduler_stop();
        break;
    default:
        break;
    }
}

void app_supervisor_task(void *arg)
{
    app_supervisor_ctx_t *ctx = (app_supervisor_ctx_t *)arg;
    const os_tick_t       now = os_scheduler_get_tick();

    while ((ctx->next_step < SCENARIO_LENGTH) && (now >= k_scenario[ctx->next_step].at_ms)) {
        supervisor_execute(ctx, k_scenario[ctx->next_step].action);
        ctx->next_step++;
    }
}
