/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: mdfw_app.c
 * Description: The hello firmware for EmuMD: what its main() does
 *              after setting up hardware, and one pass of its main loop.
 */
#include "cart_image.h"
#include "hello.h"
#include "mdfw.h"

/* Stands in for the DMA interrupt that fires on every ROM3 read. */
static void rom3_irq(void) {
  uint16_t sample;
  while (mdfw_rom3_pop(&sample)) hello_sample(sample);
}

static int app_init(void) {
  mdfw_rom4_load(cart_image, cart_image_length);
  hello_init(mdfw_rom4_base(), "emulated");
  mdfw_rom3_set_irq(rom3_irq);
  return 0;
}

const mdfw_app_t mdfw_app = {
    .name = MDFW_NAME,
    .version = MDFW_VERSION,
    .init = app_init,
    .poll = hello_poll,
};
