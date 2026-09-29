/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/sem.h: counting semaphores between the firmware's
 * threads. Timeouts are in emulated time, as on the RP2040's timer. */
#ifndef MDFW_SHIM_PICO_SEM_H
#define MDFW_SHIM_PICO_SEM_H
#include "pico.h"
#include "hardware/sync.h" /* as the SDK's lock_core.h brings in */
#include "pico/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  void *impl;
  volatile int16_t permits;
  int16_t max_permits;
} semaphore_t;
void sem_init(semaphore_t *sem, int16_t initial_permits, int16_t max_permits);
int sem_available(semaphore_t *sem);
bool sem_release(semaphore_t *sem);
void sem_reset(semaphore_t *sem, int16_t permits);
void sem_acquire_blocking(semaphore_t *sem);
bool sem_acquire_block_until(semaphore_t *sem, absolute_time_t until);
bool sem_try_acquire(semaphore_t *sem);
static inline bool sem_acquire_timeout_us(semaphore_t *sem, uint32_t timeout_us) {
  return sem_acquire_block_until(sem, make_timeout_time_us(timeout_us));
}
static inline bool sem_acquire_timeout_ms(semaphore_t *sem, uint32_t timeout_ms) {
  return sem_acquire_block_until(sem, make_timeout_time_ms(timeout_ms));
}
#ifdef __cplusplus
}
#endif
#endif
