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
 *              - The SELECT button (select.c), which is not pressed.
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
#include "reset.h"
#include "romemul.h"
#include "runtime.h"
#include "select.h"
#include "settings.h"

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
/* The SELECT button, never pressed                                     */
/* ------------------------------------------------------------------ */

void select_configure() {}
bool select_detectPush() { return false; }
void select_coreWaitPush(reset_callback_t reset, reset_callback_t resetLong) {
  (void)reset;
  (void)resetLong;
}
void select_coreWaitPushDisable() {}
void select_checkPushReset() {}
void select_setResetCallback(reset_callback_t reset) { (void)reset; }
void select_setLongResetCallback(reset_callback_t resetLong) { (void)resetLong; }

/* Waits for a push that never comes. */
void select_waitPush() {
  for (;;) sleep_ms(1000);
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
#ifdef SELECT_GPIO
  mdfw_gpio_inputs &= ~(1u << SELECT_GPIO); /* not pressed */
#endif
  return 0;
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
    .main = template_main,
};
