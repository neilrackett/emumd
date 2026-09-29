/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: main_test.c
 * Description: Checks a firmware whose main loop never returns
 *              (mdfw_app.main): its own thread, sleeps that wait for
 *              emulated time, the ROM3 ring and peek across threads,
 *              timers and alarms, semaphores, watchdog scratch across a
 *              reboot, and stopping the threads at power-off.
 *
 *   main_test
 */
#include <sched.h>
#include <time.h>

#include "emumd_plugin.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdlib.h"

static int s_fails;
#define CHECK(c)                                                     \
  do {                                                               \
    if (!(c)) {                                                      \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);   \
      s_fails++;                                                     \
    }                                                                \
  } while (0)

/* Wait (really) up to a second for the firmware's thread to get there. */
#define EVENTUALLY(c)                                                     \
  do {                                                                    \
    for (int n_ = 0; n_ < 1000 && !(c); n_++) {                           \
      const struct timespec ts_ = {0, 1000000};                           \
      nanosleep(&ts_, NULL);                                              \
    }                                                                     \
    CHECK(c);                                                             \
  } while (0)

/* ---- the firmware ---------------------------------------------------- */

static volatile int s_step;          /* how far main() has got */
static volatile uint s_main_core = 99;
static volatile uint32_t s_slept_at;
static volatile unsigned s_samples, s_boots;
static volatile uint16_t s_last;
static semaphore_t s_sem;
static repeating_timer_t s_timer;
static volatile unsigned s_ticks, s_alarms;

static bool on_timer(repeating_timer_t *rt) {
  (void)rt;
  s_ticks++;
  return true;
}

static int64_t on_alarm(alarm_id_t id, void *data) {
  (void)id;
  (void)data;
  s_alarms++;
  return 0;
}

static void got_sample(uint16_t v) {
  s_samples++;
  s_last = v;
}

extern void commemul_poll(void (*cb)(uint16_t));

static int app_init(void) {
  s_boots++;
  s_step = 0;
  return 0;
}

static void app_main(void) {
  s_main_core = get_core_num();
  s_step = 1;
  if (watchdog_hw->scratch[3] == 0x1234u) { /* back from the reboot */
    s_step = 100;
    for (;;) sleep_ms(1000);
  }
  sleep_until(11000); /* waits for the emulator */
  s_slept_at = time_us_32();
  s_step = 2;
  while (s_samples < 3) commemul_poll(got_sample);
  s_step = 3;
  sem_acquire_blocking(&s_sem); /* released by the test */
  s_step = 4;
  watchdog_hw->scratch[3] = 0x1234u;
  watchdog_reboot(0, 0, 0);
  for (;;) tight_loop_contents();
}

const mdfw_app_t mdfw_app = {.name = "main", .version = "v0",
                             .init = app_init, .main = app_main};

static void host_log(const char *line) { (void)line; }

int main(void) {
  const emumd_plugin_t *p = emumd_plugin_v1();
  const emumd_host_t host = {EMUMD_PLUGIN_ABI, "/tmp", "", 0, host_log};
  sem_init(&s_sem, 0, 1);

  CHECK(p->power_on(&host, 1000) == 0);
  EVENTUALLY(s_step == 1);
  CHECK(s_main_core == 0);
  CHECK(get_core_num() == 0);

  /* Sleeping on the firmware's thread waits for emulated time. */
  p->tick(5000);
  const struct timespec ms20 = {0, 20000000};
  nanosleep(&ms20, NULL);
  CHECK(s_step == 1);
  p->tick(12000);
  EVENTUALLY(s_step == 2);
  CHECK(s_slept_at >= 11000 && s_slept_at <= 12000);

  /* ROM3 samples reach the firmware's thread; a peek leaves them there. */
  uint32_t cursor = 0;
  uint16_t v;
  p->rom3_read(0x8401, 14000);
  CHECK(mdfw_rom3_peek(&cursor, &v) && v == 0x8401);
  CHECK(!mdfw_rom3_peek(&cursor, &v));
  p->rom3_read(0x8402, 14000);
  p->rom3_read(0x8403, 14000);
  EVENTUALLY(s_step == 3);
  CHECK(s_samples == 3 && s_last == 0x8403);
  CHECK(mdfw_rom3_peek(&cursor, &v) && v == 0x8402);
  CHECK(mdfw_rom3_peek(&cursor, &v) && v == 0x8403);
  CHECK(!mdfw_rom3_peek(&cursor, &v));

  /* Timers and alarms fire on the emulator's thread in emulated time. */
  CHECK(add_repeating_timer_us(-1000, on_timer, NULL, &s_timer));
  CHECK(add_alarm_in_us(2500, on_alarm, NULL, true) > 0);
  p->tick(14500);
  CHECK(s_ticks == 0 && s_alarms == 0);
  p->tick(15000);
  CHECK(s_ticks == 1);
  p->tick(16000);
  CHECK(s_ticks == 2);
  p->tick(16600);
  CHECK(s_alarms == 1);
  p->tick(30000); /* late: once, not a burst */
  CHECK(s_ticks == 3 && s_alarms == 1);
  CHECK(cancel_repeating_timer(&s_timer));
  p->tick(40000);
  CHECK(s_ticks == 3);

  /* A semaphore released here; then the firmware reboots itself, which
   * happens on the emulator's thread, and watchdog scratch survives. */
  CHECK(sem_release(&s_sem));
  EVENTUALLY(s_step == 4);
  for (int i = 0; i < 1000 && s_boots < 2; i++) {
    p->tick(41000 + (uint64_t)i);
    nanosleep(&(struct timespec){0, 1000000}, NULL);
  }
  CHECK(s_boots == 2);
  EVENTUALLY(s_step == 100);

  /* A cold power-on clears it; power-off stops a thread that is asleep. */
  p->power_off();
  CHECK(p->power_on(&host, 50000) == 0);
  EVENTUALLY(s_step == 3); /* in sem_acquire_blocking */
  CHECK(watchdog_hw->scratch[3] == 0);
  p->power_off();

  /* Sleeping on the emulator's thread moves emulated time on. */
  const uint64_t t = time_us_64();
  sleep_ms(1);
  CHECK(time_us_64() == t + 1000);

  printf("main_test: %s\n", s_fails ? "FAIL" : "OK");
  return s_fails ? 1 : 0;
}
