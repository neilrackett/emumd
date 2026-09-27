/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/regs/addressmap.h: XIP_BASE is where the
 * emulated flash is on the host, so `(uintptr_t)p - XIP_BASE` still gives
 * a flash offset. It is not a constant expression here. */
#ifndef MDFW_SHIM_ADDRESSMAP_H
#define MDFW_SHIM_ADDRESSMAP_H
#include "pico.h"
#define XIP_BASE ((uintptr_t)mdfw_flash)
#define XIP_NOCACHE_NOALLOC_BASE XIP_BASE
#endif
