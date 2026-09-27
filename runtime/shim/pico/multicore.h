/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/multicore.h: core 1 is a host thread, and the
 * inter-core FIFOs (8 deep each way) are real queues between the two. */
#ifndef MDFW_SHIM_PICO_MULTICORE_H
#define MDFW_SHIM_PICO_MULTICORE_H
#include "pico.h"
void multicore_launch_core1(void (*entry)(void));
void multicore_reset_core1(void);
bool multicore_fifo_rvalid(void);
bool multicore_fifo_wready(void);
void multicore_fifo_push_blocking(uint32_t data);
bool multicore_fifo_push_timeout_us(uint32_t data, uint64_t timeout_us);
uint32_t multicore_fifo_pop_blocking(void);
bool multicore_fifo_pop_timeout_us(uint64_t timeout_us, uint32_t *out);
void multicore_fifo_drain(void);
static inline void multicore_fifo_clear_irq(void) {}
static inline uint32_t multicore_fifo_get_status(void) { return 0; }
#endif
