/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/time.h: emulated time (mdfw_time_us). Sleeping
 * on the emulator's thread moves emulated time on; on the firmware's own
 * threads (mdfw_app.main, core 1) it waits for the emulator to get there. */
#ifndef MDFW_SHIM_PICO_TIME_H
#define MDFW_SHIM_PICO_TIME_H
#include "pico.h"
#ifdef __cplusplus
extern "C" {
#endif
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

/* Alarms and repeating timers, in emulated time. They fire on the
 * emulator's thread, as the timer interrupt would; every pool is one. */
typedef int32_t alarm_id_t;
typedef int64_t (*alarm_callback_t)(alarm_id_t id, void *user_data);
typedef struct alarm_pool alarm_pool_t;
typedef struct repeating_timer repeating_timer_t;
typedef bool (*repeating_timer_callback_t)(repeating_timer_t *rt);
struct repeating_timer {
  int64_t delay_us;
  alarm_pool_t *pool;
  alarm_id_t alarm_id;
  repeating_timer_callback_t callback;
  void *user_data;
};
alarm_pool_t *alarm_pool_get_default(void);
alarm_pool_t *alarm_pool_create(uint hardware_alarm_num, uint max_timers);
alarm_pool_t *alarm_pool_create_with_unused_hardware_alarm(uint max_timers);
void alarm_pool_destroy(alarm_pool_t *pool);
alarm_id_t alarm_pool_add_alarm_at(alarm_pool_t *pool, absolute_time_t time,
                                   alarm_callback_t callback, void *user_data,
                                   bool fire_if_past);
alarm_id_t alarm_pool_add_alarm_in_us(alarm_pool_t *pool, uint64_t us,
                                      alarm_callback_t callback, void *user_data,
                                      bool fire_if_past);
bool alarm_pool_cancel_alarm(alarm_pool_t *pool, alarm_id_t alarm_id);
bool alarm_pool_add_repeating_timer_us(alarm_pool_t *pool, int64_t delay_us,
                                       repeating_timer_callback_t callback,
                                       void *user_data, repeating_timer_t *out);
bool cancel_repeating_timer(repeating_timer_t *timer);
static inline alarm_id_t alarm_pool_add_alarm_in_ms(alarm_pool_t *pool, uint32_t ms,
                                                    alarm_callback_t callback,
                                                    void *user_data, bool fire_if_past) {
  return alarm_pool_add_alarm_in_us(pool, (uint64_t)ms * 1000u, callback, user_data,
                                    fire_if_past);
}
static inline alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback,
                                      void *user_data, bool fire_if_past) {
  return alarm_pool_add_alarm_at(alarm_pool_get_default(), time, callback, user_data,
                                 fire_if_past);
}
static inline alarm_id_t add_alarm_in_us(uint64_t us, alarm_callback_t callback,
                                         void *user_data, bool fire_if_past) {
  return alarm_pool_add_alarm_in_us(alarm_pool_get_default(), us, callback, user_data,
                                    fire_if_past);
}
static inline alarm_id_t add_alarm_in_ms(uint32_t ms, alarm_callback_t callback,
                                         void *user_data, bool fire_if_past) {
  return add_alarm_in_us((uint64_t)ms * 1000u, callback, user_data, fire_if_past);
}
static inline bool cancel_alarm(alarm_id_t alarm_id) {
  return alarm_pool_cancel_alarm(alarm_pool_get_default(), alarm_id);
}
static inline bool alarm_pool_add_repeating_timer_ms(alarm_pool_t *pool, int32_t delay_ms,
                                                     repeating_timer_callback_t callback,
                                                     void *user_data,
                                                     repeating_timer_t *out) {
  return alarm_pool_add_repeating_timer_us(pool, (int64_t)delay_ms * 1000, callback,
                                           user_data, out);
}
static inline bool add_repeating_timer_us(int64_t delay_us,
                                          repeating_timer_callback_t callback,
                                          void *user_data, repeating_timer_t *out) {
  return alarm_pool_add_repeating_timer_us(alarm_pool_get_default(), delay_us, callback,
                                           user_data, out);
}
static inline bool add_repeating_timer_ms(int32_t delay_ms,
                                          repeating_timer_callback_t callback,
                                          void *user_data, repeating_timer_t *out) {
  return add_repeating_timer_us((int64_t)delay_ms * 1000, callback, user_data, out);
}
#ifdef __cplusplus
}
#endif
#endif
