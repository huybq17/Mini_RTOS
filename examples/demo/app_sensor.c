/**
 * @file    app_sensor.c
 * @brief   Periodic task capturing sensor bursts into pooled frames (producer).
 */
#include "app_sensor.h"

#include "app_log.h"
#include "app_messages.h"
#include "bsp_temp_sensor.h"

void app_sensor_task(void *arg)
{
    app_sensor_ctx_t *ctx = (app_sensor_ctx_t *)arg;
    const uint32_t    seq = ctx->next_seq++;

    sensor_frame_t *frame = os_mempool_alloc(ctx->frame_pool);
    if (frame == NULL) {
        ctx->dropped++;
        app_log("SENSOR", "frame #%lu dropped (pool exhausted)", (unsigned long)seq);
        return;
    }

    frame->seq          = seq;
    frame->timestamp_ms = os_scheduler_get_tick();
    for (size_t i = 0U; i < SENSOR_FRAME_SAMPLES; ++i) {
        frame->samples_centi[i] = bsp_temp_sensor_read_centi();
    }

    /* Only the pointer is copied into the queue; the logger now owns the frame. */
    const os_status_t status = os_queue_send(ctx->out_queue, &frame);
    if (status == OS_OK) {
        app_log("SENSOR", "frame #%lu sent (%u bytes, zero-copy)", (unsigned long)seq,
                (unsigned int)sizeof(*frame));
    } else {
        (void)os_mempool_free(ctx->frame_pool, frame);
        ctx->dropped++;
        app_log("SENSOR", "frame #%lu dropped (%s)", (unsigned long)seq,
                os_status_to_string(status));
    }
}
