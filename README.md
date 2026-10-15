# Mini RTOS — Cooperative Event-Driven Scheduler in C

A small, non-preemptive real-time kernel written in C11. Tasks are
run-to-completion callbacks scheduled by **priority**, either **periodically**
or **on events**, and they talk to each other through **message queues**. The
API is modelled on FreeRTOS, so the project works as a hands-on introduction
to how an RTOS kernel works inside.

- No dynamic allocation: every kernel object comes from a static pool with an O(1) in-place free list.
- Zero-copy messaging through memory pools: only pointers travel through queues, as in lwIP and CMSIS-RTOS `osMemoryPool`.
- Opaque handles (`os_task_handle_t` / `TaskHandle_t`) hide the kernel's data structures from applications.
- A portable core sits on a tiny port layer: a host port for Linux, macOS and Windows, plus a simulated port for deterministic tests.
- 47 unit tests run on a virtual clock (no sleeps, no flakiness), and the code builds with `-Wall -Wextra -Wpedantic -Wconversion -Werror`.

## Features

| Feature | API |
|---|---|
| Periodic tasks (e.g. blink LED every 500 ms) | `os_task_create()` with `period_ms > 0` |
| Event-driven tasks (run only when woken) | `period_ms = OS_TASK_PERIOD_EVENT`, `os_task_notify()` |
| Priority scheduling, FIFO among equal priorities | `os_task_config_t.priority` |
| Suspend / Resume / Delete (also on the calling task) | `os_task_suspend()`, `os_task_resume()`, `os_task_delete()` |
| Message queues with copy semantics | `os_queue_send()`, `os_queue_receive()`, `os_queue_peek()` |
| Queue → task wake-up (producer/consumer) | `os_queue_set_receiver()` |
| Fixed-size memory pools: O(1) alloc/free, double-free detection, usage watermark | `os_mempool_create()`, `os_mempool_alloc()`, `os_mempool_free()` |
| Runtime statistics: run count, overruns, max execution time | `os_task_get_info()` |
| Drift-free periods and overrun detection | built into the scheduler |
| Wrap-safe 32-bit tick arithmetic | built into the scheduler |

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ Application  (examples/demo)                                  │
│   app_supervisor  app_blink  app_sensor ──queue──▶ app_logger  │
│                                  (frame pointers, zero-copy)  │
│   bsp_led  bsp_temp_sensor   (simulated hardware)              │
├──────────────────────────────────────────────────────────────┤
│ Public API   (include/)                                       │
│   os.h → os_task.h  os_queue.h  os_mempool.h  os_scheduler.h  │
├──────────────────────────────────────────────────────────────┤
│ Kernel core  (src/core/)  — platform independent              │
│   os_scheduler.c   ready list (by priority) + delayed list    │
│   os_task.c        TCB pool, task state machine               │
│   os_queue.c       ring-buffer message queues                 │
│   os_mempool.c     public fixed-size memory pools             │
│   os_block_pool.c  O(1) in-place free-list allocator          │
│   os_list.c        intrusive doubly-linked list               │
├──────────────────────────────────────────────────────────────┤
│ Port layer   (include/os_port.h)                              │
│   src/port/host  real clock (POSIX clock_gettime / Win32 QPC)  │
│   src/port/sim   virtual clock for unit tests                 │
└──────────────────────────────────────────────────────────────┘
```

Dependencies only point downwards. The core never includes `<stdio.h>`, never
calls `malloc`, and only reaches the hardware through `os_port.h`. Porting to a
microcontroller means writing one file (SysTick for the tick, interrupt masking
for critical sections, `WFI` for idle).

### Task state machine

```
              create
                │
                ▼
   ┌──────▶ WAITING ──── period elapsed / notify ───▶ READY
   │            ▲                                       │
   │            │ callback returns                      │ highest priority
   │            └───────────── RUNNING ◀────────────────┘
   │                             │
   │ resume          suspend (from any state)
   └────────── SUSPENDED ◀───────┘          delete (from any state) → slot freed
```

### Scheduling loop

```c
while (running) {
    release_due_tasks(now);           // delayed list → ready list
    task = pop_highest_priority();    // ready list is kept sorted
    if (task) run_to_completion(task);
    else      idle_until_next_release();
}
```

## Project layout

```
freertos_mock/
├── Makefile
├── include/            public API (the only headers applications see)
├── src/
│   ├── core/           kernel implementation + private headers
│   └── port/
│       ├── host/       desktop port (Linux / macOS / Windows)
│       └── sim/        virtual-clock port for tests
├── examples/demo/      layered demo application (bsp → app → main)
├── tests/              unit tests + minimal test framework
└── docs/               design notes
```

## Build and run

Requirements: GCC or Clang, GNU Make, and a POSIX shell (Linux, macOS, or on
Windows: MSYS2, Git Bash with MinGW, or [w64devkit](https://github.com/skeeto/w64devkit)).

```sh
make              # kernel library + demo (debug build)
make run          # run the demo (about 8 seconds)
make test         # run the unit tests
make BUILD=release test
make SANITIZE=1 test        # ASan + UBSan (Linux/macOS)
make DEFINES=-DOS_CFG_MAX_TASKS=32
make check        # cppcheck
make format       # clang-format
```

### Demo output (excerpt)

The supervisor suspends the logger at 2 s. Frames pile up until the 4-block
pool runs dry, the sensor drops frame #6, and the logger drains the backlog
when it is resumed at 6.5 s:

```
[     0 ms] BLINK      LED ON
[     0 ms] SENSOR     frame #0 sent (40 bytes, zero-copy)
[     0 ms] LOGGER     frame #0: min 25.00 / avg 25.56 / max 26.00 C (latency 0 ms)
...
[  2001 ms] SUPERVISOR suspend logger task -> OS_OK
[  2001 ms] SENSOR     frame #2 sent (40 bytes, zero-copy)
[  3001 ms] SUPERVISOR suspend blink task -> OS_OK
[  3001 ms] SENSOR     frame #3 sent (40 bytes, zero-copy)
[  4001 ms] SENSOR     frame #4 sent (40 bytes, zero-copy)
[  5000 ms] SUPERVISOR resume blink task -> OS_OK
[  5000 ms] SENSOR     frame #5 sent (40 bytes, zero-copy)
[  6002 ms] SENSOR     frame #6 dropped (pool exhausted)
[  6501 ms] SUPERVISOR resume logger task -> OS_OK
[  6501 ms] LOGGER     frame #2: min 25.00 / avg 25.40 / max 25.80 C (latency 4500 ms)
[  6501 ms] LOGGER     frame #3: min 25.00 / avg 25.48 / max 26.00 C (latency 3500 ms)
[  6501 ms] LOGGER     frame #4: min 25.10 / avg 25.58 / max 26.00 C (latency 2500 ms)
[  6501 ms] LOGGER     frame #5: min 25.00 / avg 25.56 / max 26.00 C (latency 1501 ms)
[  7001 ms] SUPERVISOR delete sensor task -> OS_OK
[  8000 ms] SUPERVISOR stop scheduler

Task statistics:
  supervisor  WAITING    prio=4  period=  250 ms  runs= 33  overruns=0  max_exec=0 ms
  blink       READY      prio=3  period=  500 ms  runs= 12  overruns=0  max_exec=0 ms
  sensor      (deleted)
  logger      WAITING    prio=1  period=    0 ms  runs=  3  overruns=0  max_exec=0 ms

Memory pools:
  frames      4 blocks x 48 bytes  free=4  min_free=0

Frames received: 6, dropped: 1
```

## Usage

```c
#include "os.h"

static void blink_task(void *arg) { (void)arg; led_toggle(); }

int main(void)
{
    os_task_handle_t blink;
    const os_task_config_t config = {
        .name = "blink", .fn = blink_task, .arg = NULL,
        .priority = 3U, .period_ms = 500U, .start_delay_ms = 0U,
    };

    os_scheduler_init();
    os_task_create(&config, &blink);
    os_scheduler_start();             /* returns after os_scheduler_stop() */
}
```

Zero-copy producer/consumer with a memory pool and a queue of pointers:

```c
static OS_MEMPOOL_DECLARE_STORAGE(frame_storage, sizeof(frame_t), 4U);
static frame_t *queue_storage[4];
os_mempool_handle_t frames;
os_queue_handle_t   queue;

os_mempool_create(frame_storage, sizeof(frame_storage), sizeof(frame_t), &frames);
os_queue_create(queue_storage, sizeof(frame_t *), 4U, &queue);
os_queue_set_receiver(queue, logger_task);       /* logger is an event task */

/* producer */
frame_t *frame = os_mempool_alloc(frames);       /* O(1), NULL when exhausted */
fill(frame);
os_queue_send(queue, &frame);                    /* copies one pointer, wakes logger */

/* consumer */
os_queue_receive(queue, &frame);
process(frame);
os_mempool_free(frames, frame);                  /* O(1), detects double free */
```

## Design decisions

| Decision | Why |
|---|---|
| **Cooperative, run-to-completion** | No context switch and no per-task stack, so the kernel needs no assembly. Shared data needs no mutex because a task is never interrupted by another task. The cost: one long task delays all the others. |
| **Static pools instead of `malloc`** | Memory use is bounded and known at link time, there is no fragmentation, and nothing can fail at runtime after initialisation. This is the usual rule in safety-oriented embedded code (MISRA, for example). |
| **In-place free list** (`os_block_pool`) | A free block stores the link to the next free block in its own memory, so the allocator needs no bookkeeping RAM and both alloc and free are O(1). TCBs, queues, memory-pool control blocks and the public memory pools all share this one engine. |
| **Double-free detection without a bitmap** | Each free block also carries a per-pool marker. A free without the marker takes the O(1) fast path. Only when the marker is present is the free list walked, to tell a real double free from user data that happens to match. |
| **`memcpy` for the free-list node** | The allocator never reads the caller's objects through an unrelated pointer type, so there is no strict-aliasing undefined behaviour at `-O2`. Compilers turn the 16-byte `memcpy` into two plain moves. |
| **Zero-copy messaging** | A frame is written once into a pool block and only its pointer is queued, so the cost of a send does not grow with the payload size. Ownership moves with the pointer: the receiver frees the block. |
| **Opaque handles** | `struct os_task` is only defined in `src/core/os_task_internal.h`. Applications cannot read or corrupt the TCB, and the kernel can change it without breaking the API. Handles are also checked against the pool, so stale or foreign pointers are rejected. |
| **Intrusive linked list** | The list node is embedded in the TCB (`OS_CONTAINER_OF`), so moving a task between lists never allocates, and removing one is O(1). This is the same technique as Linux `list_head` and FreeRTOS `ListItem_t`. |
| **Sorted ready list** | Insertion is O(n) and picking the next task is O(1). For a handful of tasks this is the fastest option. FreeRTOS instead keeps one list per priority level plus a bitmap (O(1) insertion), which is the natural next step. |
| **Drift-free periods** | The next release is computed as `previous_release + period`, not `now + period`, so execution time and jitter do not accumulate. |
| **Overrun policy** | If a job finishes after one or more of its next releases, those releases are skipped and counted in `overrun_count`. A burst of catch-up runs would starve lower-priority tasks. |
| **Wrap-safe ticks** | `(int32_t)(now - deadline) >= 0` stays correct when the 32-bit millisecond counter wraps after 49.7 days. The wrap is covered by a unit test. |
| **Port layer + simulated clock** | The same kernel binary is tested against a virtual clock: tests are instant and deterministic, and can create edge cases such as a tick wrap or a 250 ms overrun on demand. |

## Limitations and possible extensions

- Non-preemptive: a long-running task delays higher-priority tasks until it returns. A preemptive kernel would need per-task stacks and a context switch (PendSV on Cortex-M).
- A stale handle whose slot has been reused by a new task is not detected. A generation counter encoded in the handle would fix this.
- Possible next steps: O(1) bitmap priority lists, one-shot software timers, a sleep/tickless mode, an ISR-safe send with real interrupt masking on an MCU port, and a CI pipeline (GitHub Actions running `make test SANITIZE=1`).
