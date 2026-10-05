/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: wifi_demo.h
 * Description: The Wi-Fi example's logic: it joins a network, then
 *              fetches a web page whenever the ST asks (a read of ROM3)
 *              and puts what came back in ROM4 for the ST to print. It
 *              serves a page of its own on port 80, too.
 */
#ifndef WIFI_DEMO_H
#define WIFI_DEMO_H

#include <stdbool.h>
#include <stdint.h>

/* ROM4 offsets; cart/cart.s uses the same. */
#define DEMO_STATUS 0x1000      /* word: 0 joining, 1 joined, 2 failed */
#define DEMO_STATUS_TEXT 0x1002 /* how joining went, NUL-terminated */
#define DEMO_ANSWERS 0x1100     /* word: fetches done so far */
#define DEMO_ANSWER_TEXT 0x1102 /* the latest one, NUL-terminated */
#define DEMO_ANSWER_MAX 0x0F00

/* The ROM3 read the ST makes to ask for a fetch: tst.b $FB1234. */
#define DEMO_FETCH 0x1234

void wifi_demo_init(uintptr_t rom4_base, const char *url);
void wifi_demo_sample(uint16_t sample); /* a ROM3 sample, from the interrupt */
void wifi_demo_main(void);              /* never returns */

#endif
