/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico.c
 * Description: Host stand-ins for the Pico SDK calls declared in shim/:
 *              time and sleeping (emulated time), clocks, reboots.
 */

#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "pico.h"
#include "pico/time.h"
#include "runtime.h"

uint32_t time_us_32(void) { return (uint32_t)mdfw_time_us(); }
uint64_t time_us_64(void) { return mdfw_time_us(); }

/* Sleeping moves emulated time on, so a firmware that waits for a moment
 * gets there without the emulator stalling. */
void sleep_us(uint64_t us) { mdfw_runtime_sleep(us); }
void sleep_ms(uint32_t ms) { mdfw_runtime_sleep((uint64_t)ms * 1000u); }
void sleep_until(absolute_time_t t) {
  const uint64_t now = mdfw_time_us();
  if (t > now && t != at_the_end_of_time) mdfw_runtime_sleep(t - now);
}
void busy_wait_us(uint64_t us) { sleep_us(us); }
void busy_wait_us_32(uint32_t us) { sleep_us(us); }
void busy_wait_ms(uint32_t ms) { sleep_ms(ms); }
void busy_wait_until(absolute_time_t t) { sleep_until(t); }

static uint32_t s_sys_hz = 125000000u;

bool set_sys_clock_khz(uint32_t freq_khz, bool required) {
  (void)required;
  s_sys_hz = freq_khz * 1000u;
  return true;
}

uint32_t clock_get_hz(enum clock_index clk_index) {
  switch (clk_index) {
    case clk_sys:
      return s_sys_hz;
    case clk_peri:
      return s_sys_hz;
    case clk_usb:
    case clk_adc:
      return 48000000u;
    case clk_ref:
      return 12000000u;
    case clk_rtc:
      return 46875u;
    default:
      return 0;
  }
}

void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) {
  (void)pc;
  (void)sp;
  (void)delay_ms;
  mdfw_runtime_reboot();
}
