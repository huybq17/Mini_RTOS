/**
 * @file    app_logger.c
 * @brief   Event-driven task summarising sensor frames (consumer).
 */
#include "app_logger.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "app_log.h"
#include "app_messages.h"

typedef struct {
    int32_t min;
    int32_t max;
    int32_t avg;
} frame_stats_t;

static frame_stats_t frame_compute_stats(const sensor_frame_t *frame)
{
    frame_stats_t stats = { INT32_MAX, INT32_MIN, 0 };
    int32_t       sum   = 0;

    for (size_t i = 0U; i < SENSOR_FRAME_SAMPLES; ++i) {
        const int32_t sample = frame->samples_centi[i];

        stats.min = (sample < stats.min) ? sample : stats.min;
        stats.max = (sample > stats.max) ? sample : stats.max;
        sum += sample;
    }
    stats.avg = sum / (int32_t)SENSOR_FRAME_SAMPLES;
    return stats;
}

/* Format hundredths of a degree as "-12.34" without floating point. */
static void format_centi(char *buf, size_t len, int32_t centi)
{
    const int32_t magnitude = (centi < 0) ? -centi : centi;

    (void)snprintf(buf, len, "%s%ld.%02ld", (centi < 0) ? "-" : "", (long)(magnitude / 100),
                   (long)(magnitude % 100));
}

void app_logger_task(void *arg)
{
    app_logger_ctx_t *ctx   = (app_logger_ctx_t *)arg;
    sensor_frame_t   *frame = NULL;

    while (os_queue_receive(ctx->in_queue, &frame) == OS_OK) {
        const frame_stats_t stats   = frame_compute_stats(frame);
        const os_tick_t     latency = os_scheduler_get_tick() - frame->timestamp_ms;
        char                min[16];
        char                max[16];
        char                avg[16];

        format_centi(min, sizeof(min), stats.min);
        format_centi(max, sizeof(max), stats.max);
        format_centi(avg, sizeof(avg), stats.avg);

        ctx->received++;
        app_log("LOGGER", "frame #%lu: min %s / avg %s / max %s C (latency %" PRIu32 " ms)",
                (unsigned long)frame->seq, min, avg, max, latency);

        /* The logger owns the frame: return it to the pool. */
        (void)os_mempool_free(ctx->frame_pool, frame);
        frame = NULL;
    }
}
