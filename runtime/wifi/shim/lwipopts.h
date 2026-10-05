/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: lwipopts.h
 * Description: The firmware's own lwipopts.h (found further along the
 *              include path), with what a 64-bit host needs on top.
 */
#ifndef MDFW_SHIM_LWIPOPTS_H
#define MDFW_SHIM_LWIPOPTS_H

#include_next "lwipopts.h"

#if !NO_SYS
#error "EmuMD runs lwIP without an OS (NO_SYS=1, as pico_cyw43_arch_lwip_poll and _threadsafe_background do)"
#endif

/* The RP2040's 4 would leave pointers in lwIP's pools misaligned here. */
#if MEM_ALIGNMENT < __SIZEOF_POINTER__
#undef MEM_ALIGNMENT
#define MEM_ALIGNMENT __SIZEOF_POINTER__
#endif

#endif
