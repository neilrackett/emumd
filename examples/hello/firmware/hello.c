/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: hello.c
 * Description: The hello firmware's logic (see hello.h). The same file
 *              would build into the real firmware, next to a main.c that
 *              sets up the PIO, DMA and SD card.
 */
#include "hello.h"

#include <stdio.h>
#include <string.h>

#include "debug.h"

static volatile uint8_t *s_rom4;
static volatile bool s_pinged;
static uint16_t s_pongs;

/* The RP keeps ROM4 as little-endian halfwords, and the ST reads each as a
 * big-endian word, so a byte for ST address n goes at RP byte n ^ 1. */
static void st_puts(uint32_t offset, const char *text) {
  size_t i = 0;
  do {
    s_rom4[(offset + i) ^ 1u] = (uint8_t)text[i];
  } while (text[i++]);
}

static void st_put16(uint32_t offset, uint16_t value) {
  *(volatile uint16_t *)(s_rom4 + offset) = value;
}

void hello_init(uintptr_t rom4_base, const char *platform) {
  char text[128];
  s_rom4 = (volatile uint8_t *)rom4_base;
  s_pongs = 0;
  snprintf(text, sizeof(text), "Hello from the Multi-device (%s)!\r\n", platform);
  st_puts(HELLO_TEXT, text);
  st_put16(HELLO_PONGS, 0);
  DPRINTF("hello: greeting in ROM4\n");
}

/* Interrupt context: note it, let the main loop answer. */
void hello_sample(uint16_t sample) {
  if (sample == HELLO_PING) s_pinged = true;
}

bool hello_poll(void) {
  char text[64];
  if (!s_pinged) return false;
  s_pinged = false;
  s_pongs++;
  snprintf(text, sizeof(text), "Pong %u from the RP2040's side.\r\n", s_pongs);
  st_puts(HELLO_PONG_TEXT, text);
  st_put16(HELLO_PONGS, s_pongs); /* last: the ST waits for this */
  DPRINTF("hello: pong %u\n", s_pongs);
  return true;
}
