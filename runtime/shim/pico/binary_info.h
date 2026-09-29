/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/binary_info.h: there is no binary for picotool
 * to read, so the declarations come to nothing. */
#ifndef MDFW_SHIM_PICO_BINARY_INFO_H
#define MDFW_SHIM_PICO_BINARY_INFO_H
#include "pico.h"
#define bi_decl(...)
#define bi_decl_if_func_used(...)
#define bi_program_name(...)
#define bi_program_description(...)
#define bi_program_version_string(...)
#define bi_program_url(...)
#define bi_1pin_with_name(...)
#define bi_2pins_with_names(...)
#define bi_4pins_with_names(...)
#define bi_pin_mask_with_name(...)
#define bi_block_device(...)
#endif
