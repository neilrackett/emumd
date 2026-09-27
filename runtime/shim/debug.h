/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for the SidecarTridge templates' debug.h: DPRINTF output
 * goes to the emulator's log with --md-verbose. */
#ifndef MDFW_SHIM_DEBUG_H
#define MDFW_SHIM_DEBUG_H
#include "pico.h"
#define DPRINTF(fmt, ...) mdfw_debug(fmt, ##__VA_ARGS__)
#define DPRINTFRAW(fmt, ...) mdfw_debug(fmt, ##__VA_ARGS__)
#endif
