/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: template.c
 * Description: The SidecarTridge microfirmware template's hardware layer,
 *              for a firmware built with `template = sidecartridge`: its
 *              own main() runs, as core 0, while this stands in for what a
 *              host does not have.
 *
 *              - ROM emulation (romemul.c): EmuMD serves ROM4, and ROM3
 *                reads reach the firmware as the PIO and DMA would deliver
 *                them: through commemul, or (the older template) the
 *                lookup DMA channel's register and its interrupt handler.
 *              - The SELECT button (select.c): pressed when
 *                --md-option select=... says, and never otherwise.
 *              - The microSD card (sdcard.c): sdcard.c in this folder.
 *              - The Booster: on blank flash, this does what its first
 *                run would (this app boots, with a settings sector, and a
 *                Wi-Fi network to join), and a jump to it stops the
 *                firmware.
 *
 *              A glue file's mdfw_app, if there is one, replaces this one.
 */

#include "constants.h"
#include "gconfig.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "mdfw.h"
#include "pico/multicore.h"
#include "reset.h"
#include "romemul.h"
#include "runtime.h"
#include "select.h"
#include "settings.h"

#include <stdlib.h>

int main();

/* ------------------------------------------------------------------ */
/* ROM emulation                                                        */
/* ------------------------------------------------------------------ */

#ifndef ROM_BANKS
#define ROM_BANKS 1
#endif
#ifndef FLASH_ROM_LOAD_OFFSET
#define FLASH_ROM_LOAD_OFFSET 0
#endif

/* The ROM images a firmware keeps in flash, into the RAM they are served
 * from, as romemul.c does when asked. */
static void copy_flash_to_ram(void) {
  memcpy(mdfw_rom_in_ram, mdfw_flash + FLASH_ROM_LOAD_OFFSET, 0x10000u * ROM_BANKS);
}

#if EMUMD_ROMEMUL_CALLBACKS
/* The older template: a DMA channel looks up each address the ST puts on
 * the bus, and its interrupt hands the firmware (its responseCallback)
 * the address it read: A16 set for ROM3, the offset in the low bits. */
#define LOOKUP_DMA_CHANNEL 3

static IRQInterceptionCallback s_response;

static void rom3_irq(void) {
  uint16_t sample;
  while (mdfw_rom3_pop(&sample)) {
    dma_hw->ch[LOOKUP_DMA_CHANNEL].al3_read_addr_trig = 0x20010000u | sample;
    dma_hw->ints1 |= 1u << LOOKUP_DMA_CHANNEL;
    if (s_response) s_response();
  }
}

int init_romemul(IRQInterceptionCallback requestCallback,
                 IRQInterceptionCallback responseCallback, bool copyFlashToRAM) {
  (void)requestCallback;
  if (copyFlashToRAM) copy_flash_to_ram();
  s_response = responseCallback;
  mdfw_rom3_set_irq(rom3_irq);
  return 0;
}

void dma_setResponseCB(IRQInterceptionCallback responseCallback) {
  s_response = responseCallback;
}

int romemul_getLookupDataRomDmaChannel(void) { return LOOKUP_DMA_CHANNEL; }

/* The PIO's handlers, which EmuMD has no need of. */
void dma_irqHandlerLookup(void) {}
void dma_irqHandlerAddress(void) {}
#else
/* The template since: ROM3 reads go through commemul, which EmuMD has. */
int init_romemul(bool copyFlashToRAM) {
  if (copyFlashToRAM) copy_flash_to_ram();
  return 0;
}
#endif

/* ------------------------------------------------------------------ */
/* The SELECT button                                                    */
/* ------------------------------------------------------------------ */

/* --md-option select=MS[+HOLD][,MS[+HOLD]...] holds SELECT down at MS
 * milliseconds of emulated time from when Hatari started, for HOLD ms
 * (SELECT_TAP_MS without one). The clock runs on through resets and
 * reboots, so a press that reboots the firmware is not pressed again.
 * Without the option SELECT is never pressed, and the calls below do what
 * they always have here. With it they do what select.c does, timed by
 * the firmware's own select.h, in emulated time. */
#define SELECT_TAP_MS 200
#define SELECT_PRESSES_MAX 64
#ifndef SELECT_LOOP_DELAY
#define SELECT_LOOP_DELAY 10
#endif
#ifndef SELECT_DEBOUNCE_MS
#define SELECT_DEBOUNCE_MS 30
#endif
#ifndef SELECT_LONG_RESET
#define SELECT_LONG_RESET 10000
#endif

static struct {
  uint64_t at_ms, hold_ms;
} s_presses[SELECT_PRESSES_MAX];
static unsigned s_press_count;
static reset_callback_t s_reset_cb;
static reset_callback_t s_reset_long_cb;
static volatile bool s_core1_waiting;
static bool s_latched;
static uint64_t s_latched_at_us;
static bool s_long_seen;

static void select_load_presses(void) {
  s_press_count = 0;
  const char *spec = mdfw_option("select");
  if (!spec) return;
  for (const char *p = spec; *p;) {
    char *end;
    const unsigned long long at = strtoull(p, &end, 10);
    unsigned long long hold = SELECT_TAP_MS;
    if (end == p) goto bad;
    if (*end == '+') {
      p = end + 1;
      hold = strtoull(p, &end, 10);
      if (end == p || hold == 0) goto bad;
    }
    if (*end && *end != ',') goto bad;
    if (s_press_count == SELECT_PRESSES_MAX) {
      mdfw_log("SELECT: only the first %d presses are used", SELECT_PRESSES_MAX);
      break;
    }
    s_presses[s_press_count].at_ms = at;
    s_presses[s_press_count].hold_ms = hold;
    s_press_count++;
    p = *end ? end + 1 : end;
  }
  mdfw_log("SELECT: %u press%s scheduled", s_press_count, s_press_count == 1 ? "" : "es");
  return;
bad:
  mdfw_log("SELECT: cannot read select=%s (MS[+HOLD], comma-separated)", spec);
  s_press_count = 0;
}

/* Whether the schedule holds SELECT down now; its GPIO reads the same, for
 * firmware that reads the pin itself. */
bool select_detectPush() {
  const uint64_t now_ms = mdfw_time_us() / 1000u;
  bool down = false;
  for (unsigned i = 0; i < s_press_count && !down; i++) {
    down = now_ms >= s_presses[i].at_ms &&
           now_ms - s_presses[i].at_ms < s_presses[i].hold_ms;
  }
#ifdef SELECT_GPIO
  if (down) {
    __atomic_fetch_or(&mdfw_gpio_inputs, 1u << SELECT_GPIO, __ATOMIC_RELAXED);
  } else {
    __atomic_fetch_and(&mdfw_gpio_inputs, ~(1u << SELECT_GPIO), __ATOMIC_RELAXED);
  }
#endif
  return down;
}

void select_configure() {}
void select_setResetCallback(reset_callback_t reset) { s_reset_cb = reset; }
void select_setLongResetCallback(reset_callback_t resetLong) { s_reset_long_cb = resetLong; }

/* Whether SELECT stays as given for the debounce time. */
static bool select_stable(bool down) {
  for (uint32_t ms = 0; ms < SELECT_DEBOUNCE_MS; ms += SELECT_LOOP_DELAY) {
    if (select_detectPush() != down) return false;
    sleep_ms(SELECT_LOOP_DELAY);
  }
  return true;
}

static void select_released(bool long_press) {
  reset_callback_t cb = long_press ? s_reset_long_cb : s_reset_cb;
  if (cb) cb();
}

/* Returns at once unless SELECT is held; then waits for it to be let go
 * and calls the short or long press callback. */
void select_waitPush() {
  if (!s_press_count) {
    for (;;) sleep_ms(1000); /* waits for a push that never comes */
  }
  if (!select_stable(true)) return;
  const uint64_t from = mdfw_time_us();
  while (select_detectPush()) sleep_ms(SELECT_LOOP_DELAY);
  while (!select_stable(false)) sleep_ms(SELECT_LOOP_DELAY);
  select_released((mdfw_time_us() - from) / 1000u >= SELECT_LONG_RESET);
}

static void select_core1_wait(void) {
  while (s_core1_waiting && !select_stable(true)) sleep_ms(SELECT_LOOP_DELAY);
  if (!s_core1_waiting) return;
  select_waitPush();
  s_core1_waiting = false;
}

/* Waits on core 1, as select.c does; without a schedule there is nothing
 * to wait for, and core 1 is left alone. */
void select_coreWaitPush(reset_callback_t reset, reset_callback_t resetLong) {
  s_reset_cb = reset;
  s_reset_long_cb = resetLong;
  if (!s_press_count || s_core1_waiting) return;
  s_core1_waiting = true;
  multicore_launch_core1(select_core1_wait);
}

void select_coreWaitPushDisable() {
  if (!s_core1_waiting) return;
  s_core1_waiting = false;
  multicore_reset_core1();
}

/* For a main loop: latches a press, and calls the callback when it ends. */
void select_checkPushReset() {
  if (!s_press_count) return;
  const bool down = select_detectPush();
  if (down && !s_latched) {
    if (!select_stable(true)) return;
    s_latched = true;
    s_latched_at_us = mdfw_time_us();
    s_long_seen = false;
  } else if (down) {
    if ((mdfw_time_us() - s_latched_at_us) / 1000u >= SELECT_LONG_RESET) s_long_seen = true;
  } else if (s_latched) {
    if (!select_stable(false)) return;
    const bool long_press =
        s_long_seen || (mdfw_time_us() - s_latched_at_us) / 1000u >= SELECT_LONG_RESET;
    s_latched = false;
    s_long_seen = false;
    select_released(long_press);
  }
}

/* ------------------------------------------------------------------ */
/* The Booster                                                          */
/* ------------------------------------------------------------------ */

void reset_jump_to_booster(void) {
  mdfw_log("the firmware jumped to the Booster, which EmuMD does not run");
  if (!mdfw_runtime_own_thread()) return;
  for (;;) sleep_ms(1000);
}

/* What the Booster's first run leaves in flash for this app: the global
 * settings say it is the app to boot, and the lookup table gives it the
 * first settings sector. A firmware with Wi-Fi gets a network name too
 * (any will do: see EmuMD's docs/GUIDE.md, Wi-Fi). */
static void booster_first_run(void) {
  if (gconfig_init(CURRENT_APP_UUID_KEY) != GCONFIG_SUCCESS) {
    mdfw_log("first run: the Booster's set-up for %s", CURRENT_APP_UUID_KEY);
    SettingsContext *global = gconfig_getContext();
    settings_put_string(global, PARAM_BOOT_FEATURE, CURRENT_APP_UUID_KEY);
    settings_save(global, true);

    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0, sizeof(page)); /* sector 0, then the end of the table */
    memcpy(page, CURRENT_APP_UUID_KEY, strlen(CURRENT_APP_UUID_KEY));
    const uint32_t lookup = (uint32_t)((uintptr_t)&_global_lookup_flash_start - XIP_BASE);
    flash_range_erase(lookup, FLASH_SECTOR_SIZE);
    flash_range_program(lookup, page, sizeof(page));
  }
#if defined(CYW43_LWIP) && defined(PARAM_WIFI_SSID)
  SettingsContext *global = gconfig_getContext();
  SettingsConfigEntry *ssid = settings_find_entry(global, PARAM_WIFI_SSID);
  if (ssid && !ssid->value[0]) {
    settings_put_string(global, PARAM_WIFI_SSID, "EmuMD");
    settings_save(global, true);
  }
#endif
}

/* ------------------------------------------------------------------ */
/* The app                                                              */
/* ------------------------------------------------------------------ */

static int template_init(void) {
  select_load_presses();
  select_detectPush(); /* SELECT's GPIO: not pressed, or as scheduled */
  return 0;
}

/* Keeps SELECT's GPIO in step with the schedule for firmware that reads
 * the pin without asking select_detectPush(). */
static bool template_poll(void) {
  if (s_press_count) select_detectPush();
  return false;
}

static void template_main(void) {
  booster_first_run();
  main();
  mdfw_log("main() returned");
}

__attribute__((weak)) const mdfw_app_t mdfw_app = {
    .name = MDFW_NAME,
#ifdef MDFW_VERSION
    .version = MDFW_VERSION,
#endif
    .init = template_init,
    .poll = template_poll,
    .main = template_main,
};
