/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/irq.h: nothing to route on the host. */
#ifndef MDFW_SHIM_HARDWARE_IRQ_H
#define MDFW_SHIM_HARDWARE_IRQ_H
#include "pico.h"
#define PICO_DEFAULT_IRQ_PRIORITY 0x80
#define PICO_HIGHEST_IRQ_PRIORITY 0x00
#define PICO_LOWEST_IRQ_PRIORITY 0xc0
static inline void irq_set_enabled(uint num, bool enabled) { (void)num; (void)enabled; }
static inline void irq_set_priority(uint num, uint8_t p) { (void)num; (void)p; }
static inline void irq_set_exclusive_handler(uint num, irq_handler_t h) { (void)num; (void)h; }
static inline void irq_add_shared_handler(uint num, irq_handler_t h, uint8_t p) { (void)num; (void)h; (void)p; }
static inline void irq_remove_handler(uint num, irq_handler_t h) { (void)num; (void)h; }
static inline void irq_clear(uint num) { (void)num; }
#endif
