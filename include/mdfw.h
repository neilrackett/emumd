/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: mdfw.h
 * Description: The firmware side of EmuMD. A Multi-device firmware
 *              built as a .mdfw plugin is its own portable C sources, a
 *              small glue file that defines `mdfw_app` below, and the
 *              EmuMD runtime, which stands in for the RP2040 (Pico
 *              SDK, flash, FatFs, the cartridge bus). `mdfw build` puts
 *              them together; see docs/GUIDE.md.
 */

#ifndef MDFW_H
#define MDFW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* What the glue file defines                                           */
/* ------------------------------------------------------------------ */

typedef struct {
  const char *name;    /* shown in the emulator's log */
  const char *version; /* e.g. "v1.2.3" */

  /* Power on: what the firmware's main() does once the hardware is set
   * up -- fill ROM4 (mdfw_rom4_load), install the ROM3 handler, mount the
   * SD card... Return 0 on success. */
  int (*init)(void);

  /* One pass of the firmware's main loop. Return true if it did some work;
   * the runtime then calls it again (until it returns false). Runs on the
   * emulator's thread, which it has to give back, so a main loop that
   * blocks (waiting for the ST's VBL, say) goes in `main` instead. */
  bool (*poll)(void);

  /* Optional: after every poll that did work (for test hooks, dumps). */
  void (*after_poll)(void);

  /* Optional: power off. */
  void (*shutdown)(void);

  /* Optional, for a firmware whose main() never returns: the rest of
   * main() after init. It runs as core 0 on a thread of its own,
   * alongside the emulator, from power-on until power-off, when it is
   * stopped at its next wait (a sleep, FIFO, tight_loop_contents() or an
   * empty ROM3 ring). There, sleeping waits for emulated time to pass
   * rather than moving it on. poll, if also given, still runs on the
   * emulator's thread. */
  void (*main)(void);
} mdfw_app_t;

extern const mdfw_app_t mdfw_app;

/* ------------------------------------------------------------------ */
/* The cartridge port                                                   */
/* ------------------------------------------------------------------ */

/* ROM4: the 64 KB the ST reads at $FA0000, laid out as the RP holds it
 * (ROM_IN_RAM): word i is what the ST reads at $FA0000 + 2i. Give the
 * firmware mdfw_rom4_base() wherever it expects that RAM address. */
#define MDFW_ROM4_WORDS 0x8000u
uint16_t *mdfw_rom4(void);
static inline uintptr_t mdfw_rom4_base(void) { return (uintptr_t)mdfw_rom4(); }

/* Copy a cartridge image (e.g. the target_firmware[] array of 16-bit ST
 * words a firmware build embeds) to the start of ROM4. */
void mdfw_rom4_load(const uint16_t *words, size_t count);

/* ROM3: each ST read of $FB0000-$FBFFFF queues a 16-bit sample (the low
 * address bits) in a 4096-entry ring, as the commemul DMA does, then calls
 * the handler at once, as the DMA interrupt would. */
void mdfw_rom3_set_irq(void (*handler)(void));
bool mdfw_rom3_pop(uint16_t *sample);
uint32_t mdfw_rom3_dropped(void); /* samples lost to a full ring */

/* Look at the ring without taking anything from it, for code that
 * watches for one kind of read while the main loop drains the ring (a
 * timer callback, say): the next sample after *cursor, a position in the
 * ring's history that starts at 0 and that this moves on. False if there
 * is none yet. Samples the ring has already written over are skipped. */
bool mdfw_rom3_peek(uint32_t *cursor, uint16_t *sample);

/* ------------------------------------------------------------------ */
/* Storage                                                              */
/* ------------------------------------------------------------------ */

/* The RP's 2 MB flash, mapped at XIP_BASE. Erased (0xFF) at the first
 * power-on, kept across cold resets, and loaded from / saved to a file
 * with --md-option flash=<file>. flash_range_erase/program (Pico SDK)
 * work on it with the real alignment rules. */
#define MDFW_FLASH_BYTES (2u * 1024u * 1024u)
extern uint8_t mdfw_flash[MDFW_FLASH_BYTES];

/* The microSD card: the host folder given with --md-sd (FatFs calls
 * resolve paths inside it, ignoring case like FAT does). */
const char *mdfw_sd_root(void);

/* ------------------------------------------------------------------ */
/* Options, logging, time                                               */
/* ------------------------------------------------------------------ */

/* --md-option key=value; NULL / `def` if not given. */
const char *mdfw_option(const char *key);
int mdfw_option_int(const char *key, int def);

bool mdfw_verbose(void);
/* To the emulator's log, prefixed with the firmware name. */
void mdfw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* The same, only with --md-verbose (what DPRINTF maps to). */
void mdfw_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Emulated microseconds since power-on of the emulator. sleep_ms() and
 * friends move this on, so a firmware waiting on the clock gets there. */
uint64_t mdfw_time_us(void);

#ifdef __cplusplus
}
#endif

#endif /* MDFW_H */
