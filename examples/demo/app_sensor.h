/**
 * @file    app_sensor.h
 * @brief   Periodic task capturing sensor bursts into pooled frames (producer).
 */
#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include <stdint.h>

#include "os.h"

typedef struct {
    os_mempool_handle_t frame_pool; /**< Source of the frames. */
    os_queue_handle_t   out_queue;  /**< Carries sensor_frame_t pointers. */
    uint32_t            next_seq;
    uint32_t            dropped;    /**< Frames lost: pool exhausted or queue full. */
} app_sensor_ctx_t;

/** Task callback. @p arg is an app_sensor_ctx_t. */
void app_sensor_task(void *arg);

#endif /* APP_SENSOR_H */
