/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: commemul.c
 * Description: The SidecarTridge template's ROM3 API (commemul.h, a DMA
 *              ring of ROM3 address samples) on the runtime's ring, plus
 *              the interrupt hook MD/ROTT added and MD/DOOM's
 *              commemul_scan. Firmwares built on the template call these
 *              unchanged; leave their own commemul.c (PIO and DMA set-up)
 *              out of the .mdfw build.
 */

#include "pico.h"

typedef void (*CommEmulSampleCallback)(uint16_t sample);

int commemul_init(void) { return 0; }

/* An empty ring is usually a firmware waiting for the ST: let the other
 * threads run, and a firmware thread be stopped. */
void commemul_poll(CommEmulSampleCallback callback) {
  uint16_t sample;
  bool any = false;
  while (mdfw_rom3_pop(&sample)) {
    callback(sample);
    any = true;
  }
  if (!any) tight_loop_contents();
}

void commemul_set_irq_handler(irq_handler_t handler) { mdfw_rom3_set_irq(handler); }

void commemul_irq_ack(void) {}

/* MD/DOOM's addition, since in other firmwares: has a read of
 * `window_hi` (the high byte of its low 16 address bits) arrived since
 * *cursor, without taking anything from the ring? For a timer callback
 * watching for one kind of read while the main loop drains the ring. */
bool commemul_scan(uint32_t *cursor, uint16_t window_hi) {
  uint16_t sample;
  bool seen = false;
  while (mdfw_rom3_peek(cursor, &sample)) {
    if ((sample & 0xFF00u) == window_hi) seen = true;
  }
  return seen;
}
