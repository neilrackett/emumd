/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/async_context_threadsafe_background.h
 * Description: An async_context whose work is done in the background:
 *              on the emulator's thread, as an interrupt would on the
 *              RP2040, and while a firmware thread waits for work
 *              (pico/async_context.h).
 */
#ifndef MDFW_SHIM_PICO_ASYNC_CONTEXT_THREADSAFE_BACKGROUND_H
#define MDFW_SHIM_PICO_ASYNC_CONTEXT_THREADSAFE_BACKGROUND_H

#include "pico/async_context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_context_threadsafe_background_config {
  uint8_t low_priority_irq;           /* ignored */
  alarm_pool_t *custom_alarm_pool;    /* ignored */
} async_context_threadsafe_background_config_t;

typedef struct async_context_threadsafe_background {
  async_context_t core;
} async_context_threadsafe_background_t;

async_context_threadsafe_background_config_t
async_context_threadsafe_background_default_config(void);
bool async_context_threadsafe_background_init(
    async_context_threadsafe_background_t *self,
    async_context_threadsafe_background_config_t *config);
static inline bool async_context_threadsafe_background_init_with_defaults(
    async_context_threadsafe_background_t *self) {
  async_context_threadsafe_background_config_t config =
      async_context_threadsafe_background_default_config();
  return async_context_threadsafe_background_init(self, &config);
}

#ifdef __cplusplus
}
#endif

#endif
