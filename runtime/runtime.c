/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: runtime.c
 * Description: The plugin side of EmuMD: the emumd_plugin_v1 entry
 *              point (emumd_plugin.h) wrapped around a firmware's
 *              mdfw_app (mdfw.h). Holds the ROM4 window and the ROM3
 *              capture ring, and runs the firmware's main loop whenever
 *              the ST touches the cartridge or a frame passes.
 */

#include <pthread.h>
#include <stdarg.h>

#include "emumd_plugin.h"
#include "mdfw.h"
#include "pico.h"
#include "runtime.h"

/* ------------------------------------------------------------------ */
/* State                                                                */
/* ------------------------------------------------------------------ */

static const emumd_host_t *s_host;
static bool s_on;

static uint16_t s_rom4[MDFW_ROM4_WORDS] __attribute__((aligned(8)));

#define RING_SIZE 4096u
static uint16_t s_ring[RING_SIZE];
static uint32_t s_ring_head, s_ring_tail;
static uint32_t s_ring_dropped;
static void (*s_rom3_irq)(void);

static char s_options[4096];
static char s_sd_root[1024];

static uint64_t s_host_us;  /* what the emulator says the time is */
static uint64_t s_slept_us; /* what sleeps added on top */
static mdfw_timer_hw_t s_timer_hw;
mdfw_timer_hw_t *timer_hw = &s_timer_hw;

/* A main loop that always has work must not hang the emulator. */
#define MAX_POLLS_PER_TURN 4096

/* ------------------------------------------------------------------ */
/* Time                                                                 */
/* ------------------------------------------------------------------ */

static void publish_time(void) {
  const uint64_t t = s_host_us + s_slept_us;
  s_timer_hw.timerawl = (uint32_t)t;
  s_timer_hw.timerawh = (uint32_t)(t >> 32);
}

static void set_host_time(uint64_t now_us) {
  if (now_us > s_host_us) s_host_us = now_us;
  publish_time();
}

uint64_t mdfw_time_us(void) { return s_host_us + s_slept_us; }

void mdfw_runtime_sleep(uint64_t us) {
  s_slept_us += us;
  publish_time();
}

/* ------------------------------------------------------------------ */
/* Logging                                                              */
/* ------------------------------------------------------------------ */

static pthread_mutex_t s_log_lock = PTHREAD_MUTEX_INITIALIZER;
static char s_line[1024];
static size_t s_line_len;

/* Collect text into whole lines: DPRINTF output often arrives in pieces. */
static void log_text(const char *text) {
  pthread_mutex_lock(&s_log_lock);
  for (const char *p = text; *p; p++) {
    if (*p == '\n' || s_line_len == sizeof(s_line) - 1) {
      s_line[s_line_len] = 0;
      if (s_host && s_host->log) {
        char out[1100];
        snprintf(out, sizeof(out), "%s: %s", mdfw_app.name ? mdfw_app.name : "MD",
                 s_line);
        s_host->log(out);
      } else {
        fprintf(stderr, "%s\n", s_line);
      }
      s_line_len = 0;
      if (*p == '\n') continue;
    }
    if (*p != '\r') s_line[s_line_len++] = *p;
  }
  pthread_mutex_unlock(&s_log_lock);
}

static void log_v(const char *fmt, va_list ap) {
  char buf[2048];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  log_text(buf);
}

void mdfw_log(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_v(fmt, ap);
  va_end(ap);
  /* mdfw_log is for whole messages; end the line if the caller did not. */
  if (fmt[0] && fmt[strlen(fmt) - 1] != '\n') log_text("\n");
}

bool mdfw_verbose(void) { return s_host && s_host->verbose; }

void mdfw_debug(const char *fmt, ...) {
  if (!mdfw_verbose()) return;
  va_list ap;
  va_start(ap, fmt);
  log_v(fmt, ap);
  va_end(ap);
}

void panic(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_text("panic: ");
  log_v(fmt, ap);
  log_text("\n");
  va_end(ap);
  abort();
}

/* ------------------------------------------------------------------ */
/* Options                                                              */
/* ------------------------------------------------------------------ */

const char *mdfw_option(const char *key) {
  static char value[1024];
  const size_t klen = strlen(key);
  const char *p = s_options;
  /* The last one given wins; a bare "key" means "1". */
  const char *found = NULL;
  size_t found_len = 0;
  while (*p) {
    const char *eol = strchr(p, '\n');
    const size_t len = eol ? (size_t)(eol - p) : strlen(p);
    if (len > klen && p[klen] == '=' && strncmp(p, key, klen) == 0) {
      found = p + klen + 1;
      found_len = len - klen - 1;
    } else if (len == klen && strncmp(p, key, klen) == 0) {
      found = "1";
      found_len = 1;
    }
    p += len + (eol ? 1 : 0);
  }
  if (!found) return NULL;
  if (found_len >= sizeof(value)) found_len = sizeof(value) - 1;
  memcpy(value, found, found_len);
  value[found_len] = 0;
  return value;
}

int mdfw_option_int(const char *key, int def) {
  const char *v = mdfw_option(key);
  if (!v || !*v) return def;
  char *end = NULL;
  const long n = strtol(v, &end, 0);
  return (end && *end == 0) ? (int)n : def;
}

const char *mdfw_sd_root(void) { return s_sd_root; }

/* ------------------------------------------------------------------ */
/* The cartridge port                                                   */
/* ------------------------------------------------------------------ */

uint16_t *mdfw_rom4(void) { return s_rom4; }

void mdfw_rom4_load(const uint16_t *words, size_t count) {
  if (count > MDFW_ROM4_WORDS) count = MDFW_ROM4_WORDS;
  memcpy(s_rom4, words, count * sizeof(uint16_t));
}

void mdfw_rom3_set_irq(void (*handler)(void)) { s_rom3_irq = handler; }

bool mdfw_rom3_pop(uint16_t *sample) {
  if (s_ring_tail == s_ring_head) return false;
  *sample = s_ring[s_ring_tail % RING_SIZE];
  s_ring_tail++;
  return true;
}

uint32_t mdfw_rom3_dropped(void) { return s_ring_dropped; }

uint32_t mdfw_runtime_ring_head(void) { return s_ring_head; }
const uint16_t *mdfw_runtime_ring(void) { return s_ring; }

/* ------------------------------------------------------------------ */
/* The main loop                                                        */
/* ------------------------------------------------------------------ */

static bool s_in_loop;
static bool s_reboot_pending;
static void reboot_now(void);

static void run_main_loop(void) {
  if (!s_on || s_in_loop || !mdfw_app.poll) return;
  s_in_loop = true;
  for (int i = 0; i < MAX_POLLS_PER_TURN && !s_reboot_pending && mdfw_app.poll();
       i++) {
    if (mdfw_app.after_poll) mdfw_app.after_poll();
  }
  s_in_loop = false;
  if (s_reboot_pending) reboot_now();
}

/* ------------------------------------------------------------------ */
/* The plugin interface                                                 */
/* ------------------------------------------------------------------ */

static int plugin_power_on(const emumd_host_t *host, uint64_t now_us) {
  if (!host || host->abi != EMUMD_PLUGIN_ABI) return -1;
  s_host = host;
  snprintf(s_options, sizeof(s_options), "%s", host->options ? host->options : "");
  snprintf(s_sd_root, sizeof(s_sd_root), "%s", host->sd_dir ? host->sd_dir : ".");
  /* No trailing slash: firmware paths start with one. */
  size_t n = strlen(s_sd_root);
  while (n > 1 && s_sd_root[n - 1] == '/') s_sd_root[--n] = 0;

  set_host_time(now_us);
  memset(s_rom4, 0, sizeof(s_rom4));
  s_ring_head = s_ring_tail = s_ring_dropped = 0;
  s_rom3_irq = NULL;
  mdfw_runtime_flash_power_on();
  mdfw_runtime_fatfs_reset();

  s_on = true;
  const int rc = mdfw_app.init ? mdfw_app.init() : 0;
  if (rc != 0) {
    mdfw_log("the firmware did not start (%d)", rc);
    s_on = false;
    return rc;
  }
  run_main_loop();
  return 0;
}

static void plugin_power_off(void) {
  if (!s_on) return;
  if (mdfw_app.shutdown) mdfw_app.shutdown();
  s_on = false;
  mdfw_runtime_multicore_stop();
  mdfw_runtime_fatfs_reset();
  mdfw_runtime_flash_power_off();
}

static uint16_t plugin_rom4_read(uint32_t offset, uint64_t now_us) {
  static unsigned reads;
  set_host_time(now_us);
  /* The real main loop never stops; here it also gets a turn while the
   * ST polls ROM4 (waiting for the firmware, say). */
  if ((++reads & 255u) == 0) run_main_loop();
  return s_rom4[(offset & 0xFFFFu) >> 1];
}

static void plugin_rom3_read(uint32_t offset, uint64_t now_us) {
  set_host_time(now_us);
  if (s_ring_head - s_ring_tail >= RING_SIZE) {
    s_ring_dropped++;
  } else {
    s_ring[s_ring_head % RING_SIZE] = (uint16_t)offset;
    s_ring_head++;
  }
  if (s_on && s_rom3_irq) s_rom3_irq();
  run_main_loop();
}

static void plugin_tick(uint64_t now_us) {
  set_host_time(now_us);
  run_main_loop();
}

/* watchdog_reboot() and the like: power-cycle once the current turn of the
 * main loop is over. */
void mdfw_runtime_reboot(void) {
  mdfw_log("the firmware asked for a reboot");
  s_reboot_pending = true;
}

static void reboot_now(void) {
  const emumd_host_t *host = s_host;
  s_reboot_pending = false;
  plugin_power_off();
  plugin_power_on(host, s_host_us);
}

__attribute__((visibility("default"))) const emumd_plugin_t *emumd_plugin_v1(
    void) {
  static emumd_plugin_t plugin = {
      .abi = EMUMD_PLUGIN_ABI,
      .power_on = plugin_power_on,
      .power_off = plugin_power_off,
      .rom4_read = plugin_rom4_read,
      .rom3_read = plugin_rom3_read,
      .tick = plugin_tick,
  };
  plugin.name = mdfw_app.name ? mdfw_app.name : "unnamed firmware";
  plugin.version = mdfw_app.version ? mdfw_app.version : "";
  return &plugin;
}
