/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/watchdog.h: a reboot request is logged and
 * the firmware is powered off and on again. */
#ifndef MDFW_SHIM_HARDWARE_WATCHDOG_H
#define MDFW_SHIM_HARDWARE_WATCHDOG_H
#include "pico.h"
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);
static inline void watchdog_enable(uint32_t delay_ms, bool pause_on_debug) {
  (void)delay_ms;
  (void)pause_on_debug;
}
static inline void watchdog_update(void) {}
static inline bool watchdog_caused_reboot(void) { return false; }
#endif
