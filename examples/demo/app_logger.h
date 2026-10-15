/**
 * @file    app_logger.h
 * @brief   Event-driven task summarising sensor frames (consumer).
 */
#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <stdint.h>

#include "os.h"

typedef struct {
    os_mempool_handle_t frame_pool; /**< Where processed frames are returned. */
    os_queue_handle_t   in_queue;   /**< Carries sensor_frame_t pointers. */
    uint32_t            received;
} app_logger_ctx_t;

/** Task callback. @p arg is an app_logger_ctx_t. Drains the queue on each wake-up. */
void app_logger_task(void *arg);

#endif /* APP_LOGGER_H */
