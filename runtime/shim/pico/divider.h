/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for pico/divider.h: the SDK's division helpers, on
 * hardware/divider.h's stand-in. */
#ifndef MDFW_SHIM_PICO_DIVIDER_H
#define MDFW_SHIM_PICO_DIVIDER_H
#include "hardware/divider.h"
static inline int32_t div_s32s32(int32_t a, int32_t b) { return hw_divider_s32_quotient(a, b); }
static inline uint32_t div_u32u32(uint32_t a, uint32_t b) { return hw_divider_u32_quotient(a, b); }
static inline int32_t mod_s32s32(int32_t a, int32_t b) { return hw_divider_s32_remainder(a, b); }
static inline uint32_t mod_u32u32(uint32_t a, uint32_t b) { return hw_divider_u32_remainder(a, b); }
static inline int32_t divmod_s32s32_rem(int32_t a, int32_t b, int32_t *rem) {
  *rem = hw_divider_s32_remainder(a, b);
  return hw_divider_s32_quotient(a, b);
}
static inline uint32_t divmod_u32u32_rem(uint32_t a, uint32_t b, uint32_t *rem) {
  *rem = hw_divider_u32_remainder(a, b);
  return hw_divider_u32_quotient(a, b);
}
#endif
