/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/async_context_poll.h
 * Description: An async_context whose work is done when the firmware
 *              polls it (pico/async_context.h).
 */
#ifndef MDFW_SHIM_PICO_ASYNC_CONTEXT_POLL_H
#define MDFW_SHIM_PICO_ASYNC_CONTEXT_POLL_H

#include "pico/async_context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_context_poll {
  async_context_t core;
} async_context_poll_t;

bool async_context_poll_init_with_defaults(async_context_poll_t *self);

#ifdef __cplusplus
}
#endif

#endif
