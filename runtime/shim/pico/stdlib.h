/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/stdlib.h. stdio goes to the host's stdout. */
#ifndef MDFW_SHIM_PICO_STDLIB_H
#define MDFW_SHIM_PICO_STDLIB_H
#include "pico.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
static inline bool stdio_init_all(void) { return true; }
static inline void setup_default_uart(void) {}
#endif
