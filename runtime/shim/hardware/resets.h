/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/resets.h: there are no peripheral blocks to
 * hold in reset on the host. */
#ifndef MDFW_SHIM_HARDWARE_RESETS_H
#define MDFW_SHIM_HARDWARE_RESETS_H
#include "pico.h"
static inline void reset_block(uint32_t bits) { (void)bits; }
static inline void unreset_block(uint32_t bits) { (void)bits; }
static inline void unreset_block_wait(uint32_t bits) { (void)bits; }
#endif
