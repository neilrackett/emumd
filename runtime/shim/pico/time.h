/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/time.h: emulated time (mdfw_time_us). Sleeping
 * moves emulated time on rather than waiting. */
#ifndef MDFW_SHIM_PICO_TIME_H
#define MDFW_SHIM_PICO_TIME_H
#include "pico.h"
static inline absolute_time_t get_absolute_time(void) { return time_us_64(); }
static inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
static inline uint32_t to_ms_since_boot(absolute_time_t t) {
  return (uint32_t)(t / 1000u);
}
static inline absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) {
  return t + us;
}
static inline absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms) {
  return t + (uint64_t)ms * 1000u;
}
static inline absolute_time_t make_timeout_time_us(uint64_t us) {
  return time_us_64() + us;
}
static inline absolute_time_t make_timeout_time_ms(uint32_t ms) {
  return time_us_64() + (uint64_t)ms * 1000u;
}
static inline int64_t absolute_time_diff_us(absolute_time_t from,
                                            absolute_time_t to) {
  return (int64_t)(to - from);
}
static inline bool time_reached(absolute_time_t t) { return time_us_64() >= t; }
#define at_the_end_of_time ((absolute_time_t)UINT64_MAX)
#define nil_time ((absolute_time_t)0)
void sleep_us(uint64_t us);
void sleep_ms(uint32_t ms);
void sleep_until(absolute_time_t t);
void busy_wait_us(uint64_t us);
void busy_wait_us_32(uint32_t us);
void busy_wait_ms(uint32_t ms);
void busy_wait_until(absolute_time_t t);
#endif
