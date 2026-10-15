/**
 * @file    os_mempool_internal.h
 * @brief   Memory pool services used by other kernel modules (kernel private).
 */
#ifndef OS_MEMPOOL_INTERNAL_H
#define OS_MEMPOOL_INTERNAL_H

#include "os_mempool.h"

/** Reset the memory pool control blocks. Called by os_scheduler_init(). */
void os_mempool_module_init(void);

#endif /* OS_MEMPOOL_INTERNAL_H */
