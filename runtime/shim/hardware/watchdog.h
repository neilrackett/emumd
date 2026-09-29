/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/watchdog.h: a reboot request is logged and
 * the firmware is powered off and on again. The scratch registers keep
 * their values across that, and are cleared by a power cycle (the ST's
 * cold reset), as on the RP2040. */
#ifndef MDFW_SHIM_HARDWARE_WATCHDOG_H
#define MDFW_SHIM_HARDWARE_WATCHDOG_H
#include "pico.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  volatile uint32_t ctrl;
  volatile uint32_t load;
  volatile uint32_t reason;
  volatile uint32_t scratch[8];
  volatile uint32_t tick;
} watchdog_hw_t;
extern watchdog_hw_t *watchdog_hw;
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
static inline void watchdog_enable(uint32_t delay_ms, bool pause_on_debug) {
  (void)delay_ms;
  (void)pause_on_debug;
}
static inline void watchdog_update(void) {}
bool watchdog_caused_reboot(void);
#ifdef __cplusplus
}
#endif
#endif
