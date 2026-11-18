/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * os_types.h -- OURS: the NimBLE porting layer (NPL) types on Symbian. Every OS object is a small C struct that holds a
 * Symbian handle (RMutex / RSemaphore, process-owned, so any thread of the process may use it) or a pointer to a C++
 * object in npl_symbian.cpp. Ticks are milliseconds. Shape follows NimBLE's own porting/npl/linux/include/nimble/os_types.h.
 */
#ifndef _NPL_OS_TYPES_H
#define _NPL_OS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define OS_TASK_PRI_HIGHEST 0
#define OS_TASK_PRI_LOWEST  255

typedef uint32_t ble_npl_time_t;
typedef int32_t ble_npl_stime_t;
typedef int ble_npl_stack_t;

struct ble_npl_event {
    uint8_t ev_queued;
    ble_npl_event_fn *ev_cb;
    void *ev_arg;
    struct ble_npl_event *ev_next;     /* intrusive queue link, owned by the queue under the port lock */
};

struct ble_npl_eventq {
    struct ble_npl_event *head;
    struct ble_npl_event *tail;
    int sem;                           /* RSemaphore handle: one signal per put */
};

struct ble_npl_callout {
    struct ble_npl_event c_ev;
    struct ble_npl_eventq *c_evq;
    uint32_t c_ticks;                  /* absolute expiry, in ticks (ms) */
    bool c_active;
    struct ble_npl_callout *c_next;    /* the timer thread's active list */
};

struct ble_npl_mutex {
    int handle;                        /* RMutex handle */
};

struct ble_npl_sem {
    int handle;                        /* RSemaphore handle */
    volatile int count;                /* mirror for ble_npl_sem_get_count */
};

struct ble_npl_task {
    int handle;                        /* RThread handle */
    const char *name;
};

typedef void *(*ble_npl_task_func_t)(void *);

int ble_npl_task_init(struct ble_npl_task *t, const char *name,
                      ble_npl_task_func_t func, void *arg, uint8_t prio,
                      ble_npl_time_t sanity_itvl,
                      ble_npl_stack_t *stack_bottom, uint16_t stack_size);
int ble_npl_task_remove(struct ble_npl_task *t);
uint8_t ble_npl_task_count(void);
void ble_npl_task_yield(void);

#endif /* _NPL_OS_TYPES_H */
