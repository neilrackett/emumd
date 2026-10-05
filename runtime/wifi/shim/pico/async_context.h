/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/async_context.h
 * Description: Host stand-in for the Pico SDK's async_context, which
 *              cyw43_arch and lwIP run on: a lock, workers that run when
 *              they have work or at a time, polling and waiting for work.
 *              One implementation (runtime/wifi/async_context.c) serves
 *              both the poll and threadsafe_background kinds.
 */
#ifndef MDFW_SHIM_PICO_ASYNC_CONTEXT_H
#define MDFW_SHIM_PICO_ASYNC_CONTEXT_H

#include "pico.h"
#include "pico/time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_context async_context_t;

typedef struct async_work_on_timeout {
  struct async_work_on_timeout *next; /* the context's */
  /* Called once next_time has passed, after the worker is taken out of
   * the context; add it again from here to repeat. */
  void (*do_work)(async_context_t *context, struct async_work_on_timeout *timeout);
  absolute_time_t next_time;
  void *user_data;
} async_at_time_worker_t;

typedef struct async_when_pending_worker {
  struct async_when_pending_worker *next; /* the context's */
  /* Called when work_pending is set (async_context_set_work_pending). */
  void (*do_work)(async_context_t *context, struct async_when_pending_worker *worker);
  bool work_pending;
  void *user_data;
} async_when_pending_worker_t;

#define ASYNC_CONTEXT_FLAG_CALLBACK_FROM_NON_IRQ 0x1
#define ASYNC_CONTEXT_FLAG_CALLBACK_FROM_IRQ 0x2
#define ASYNC_CONTEXT_FLAG_POLLED 0x4

struct async_context {
  async_when_pending_worker_t *when_pending_list;
  async_at_time_worker_t *at_time_list; /* soonest first */
  absolute_time_t next_time;
  uint16_t flags;
  uint8_t core_num;
};

void async_context_acquire_lock_blocking(async_context_t *context);
void async_context_release_lock(async_context_t *context);
void async_context_lock_check(async_context_t *context);
uint32_t async_context_execute_sync(async_context_t *context,
                                    uint32_t (*func)(void *param), void *param);

bool async_context_add_at_time_worker(async_context_t *context,
                                      async_at_time_worker_t *worker);
static inline bool async_context_add_at_time_worker_at(async_context_t *context,
                                                       async_at_time_worker_t *worker,
                                                       absolute_time_t at) {
  worker->next_time = at;
  return async_context_add_at_time_worker(context, worker);
}
static inline bool async_context_add_at_time_worker_in_ms(async_context_t *context,
                                                          async_at_time_worker_t *worker,
                                                          uint32_t ms) {
  return async_context_add_at_time_worker_at(context, worker, make_timeout_time_ms(ms));
}
bool async_context_remove_at_time_worker(async_context_t *context,
                                         async_at_time_worker_t *worker);

bool async_context_add_when_pending_worker(async_context_t *context,
                                           async_when_pending_worker_t *worker);
bool async_context_remove_when_pending_worker(async_context_t *context,
                                              async_when_pending_worker_t *worker);
void async_context_set_work_pending(async_context_t *context,
                                    async_when_pending_worker_t *worker);

/* Do the work that is due. */
void async_context_poll(async_context_t *context);
void async_context_wait_until(async_context_t *context, absolute_time_t until);
/* Until there is work to do, or `until`. On a firmware thread this waits
 * (really) for the network too; on the emulator's thread, as sleeping
 * there does, it moves emulated time on. */
void async_context_wait_for_work_until(async_context_t *context, absolute_time_t until);
static inline void async_context_wait_for_work_ms(async_context_t *context, uint32_t ms) {
  async_context_wait_for_work_until(context, make_timeout_time_ms(ms));
}
static inline uint async_context_core_num(const async_context_t *context) {
  return context->core_num;
}
void async_context_deinit(async_context_t *context);

#ifdef __cplusplus
}
#endif

#endif
