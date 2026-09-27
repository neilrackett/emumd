/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: hello.h
 * Description: The hello firmware's logic, free of hardware: it puts a
 *              greeting in ROM4 for the ST's boot stub to print, and
 *              answers a "ping" (a read of ROM3) with a reply.
 */
#ifndef HELLO_H
#define HELLO_H

#include <stdbool.h>
#include <stdint.h>

/* ROM4 offsets; cart/cart.s uses the same. */
#define HELLO_TEXT 0x1000      /* greeting, NUL-terminated */
#define HELLO_PONGS 0x1100     /* word: pings answered so far */
#define HELLO_PONG_TEXT 0x1102 /* the latest reply, NUL-terminated */

/* The ROM3 read the boot stub makes: tst.b $FB1234. */
#define HELLO_PING 0x1234

void hello_init(uintptr_t rom4_base, const char *platform);
void hello_sample(uint16_t sample); /* a ROM3 sample, from the interrupt */
bool hello_poll(void);              /* main loop: true if it did work */

#endif
