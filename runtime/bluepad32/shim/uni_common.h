/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's uni_common.h: the macros a platform uses. */
#ifndef MDFW_SHIM_UNI_COMMON_H
#define MDFW_SHIM_UNI_COMMON_H
#include <stdint.h>
#ifndef BIT
#define BIT(nr) (1UL << (nr))
#endif
#ifndef BIT64
#define BIT64(nr) (1ULL << (nr))
#endif
#ifndef GENMASK
#define GENMASK(h, l) (((BIT(h) << 1) - 1) ^ (BIT(l) - 1))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif
/* Bluepad32's takes sizeof(x), which clang warns about on an array
 * parameter (a bd_addr_t); this does the same without the warning. */
#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif
#endif
