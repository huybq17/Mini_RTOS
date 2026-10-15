/**
 * @file    os_status.c
 * @brief   Status code helpers.
 */
#include "os_types.h"

const char *os_status_to_string(os_status_t status)
{
    switch (status) {
    case OS_OK:
        return "OS_OK";
    case OS_ERR_INVALID_ARG:
        return "OS_ERR_INVALID_ARG";
    case OS_ERR_NO_MEMORY:
        return "OS_ERR_NO_MEMORY";
    case OS_ERR_INVALID_STATE:
        return "OS_ERR_INVALID_STATE";
    case OS_ERR_QUEUE_FULL:
        return "OS_ERR_QUEUE_FULL";
    case OS_ERR_QUEUE_EMPTY:
        return "OS_ERR_QUEUE_EMPTY";
    default:
        return "OS_ERR_UNKNOWN";
    }
}
