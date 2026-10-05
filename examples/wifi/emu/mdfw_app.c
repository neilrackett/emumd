/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: mdfw_app.c
 * Description: The Wi-Fi example for EmuMD: what its main() does after
 *              setting up hardware, then its main loop, which never
 *              returns (so waiting for the network really waits).
 *              --md-option url= is the page it fetches; with
 *              wifi_forward=tcp:8080:80, http://localhost:8080 is its own.
 */
#include <stdio.h>

#include "cart_image.h"
#include "mdfw.h"
#include "wifi_demo.h"

/* Stands in for the DMA interrupt that fires on every ROM3 read. */
static void rom3_irq(void) {
  uint16_t sample;
  while (mdfw_rom3_pop(&sample)) wifi_demo_sample(sample);
}

static int app_init(void) {
  const char *url = mdfw_option("url");
  static char saved[256];
  snprintf(saved, sizeof(saved), "%s", url ? url : "http://detectportal.firefox.com/success.txt");
  mdfw_rom4_load(cart_image, cart_image_length);
  wifi_demo_init(mdfw_rom4_base(), saved);
  mdfw_rom3_set_irq(rom3_irq);
  return 0;
}

const mdfw_app_t mdfw_app = {
    .name = MDFW_NAME,
    .version = MDFW_VERSION,
    .init = app_init,
    .main = wifi_demo_main,
};
