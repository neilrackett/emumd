/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/sync.h: critical sections and mutexes are host
 * mutexes (they matter once core 1 runs as a thread). */
#ifndef MDFW_SHIM_PICO_SYNC_H
#define MDFW_SHIM_PICO_SYNC_H
#include "pico.h"
#include "hardware/sync.h"
typedef struct {
  void *impl;
} critical_section_t;
void critical_section_init(critical_section_t *cs);
void critical_section_init_with_lock_num(critical_section_t *cs, uint lock_num);
void critical_section_enter_blocking(critical_section_t *cs);
void critical_section_exit(critical_section_t *cs);
void critical_section_deinit(critical_section_t *cs);
typedef struct {
  void *impl;
} mutex_t;
void mutex_init(mutex_t *m);
void mutex_enter_blocking(mutex_t *m);
bool mutex_try_enter(mutex_t *m, uint32_t *owner_out);
void mutex_exit(mutex_t *m);
typedef mutex_t recursive_mutex_t;
#define recursive_mutex_init mutex_init
#define recursive_mutex_enter_blocking mutex_enter_blocking
#define recursive_mutex_exit mutex_exit
#endif
