/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: commemul.c
 * Description: The SidecarTridge template's ROM3 API (commemul.h, a DMA
 *              ring of ROM3 address samples) on the runtime's ring, plus
 *              the interrupt hook MD/ROTT added. Firmwares built on the
 *              template call these unchanged; leave their own commemul.c
 *              (PIO and DMA set-up) out of the .mdfw build.
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
