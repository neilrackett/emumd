/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: arch/cc.h
 * Description: lwIP's port to EmuMD, in place of the Pico SDK's: the
 *              compiler, assertions (a panic, as on the RP2040),
 *              random numbers and debug output (DPRINTF's way, with
 *              --md-verbose on).
 */
#ifndef MDFW_SHIM_ARCH_CC_H
#define MDFW_SHIM_ARCH_CC_H

#include <sys/time.h>

#include "pico.h"
#include "pico/rand.h"

typedef int sys_prot_t;

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#define LWIP_PLATFORM_ASSERT(x) panic("lwIP: %s", x)
#define LWIP_PLATFORM_DIAG(x) \
  do {                        \
    mdfw_debug x;             \
  } while (0)
#define LWIP_RAND() get_rand_32()

/* This computer's headers have htons() and the like already. */
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS

#endif
