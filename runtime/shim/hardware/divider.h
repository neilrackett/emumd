/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/divider.h: the SIO divider's results, from C
 * division. Dividing by zero gives what the RP2040's divider does. */
#ifndef MDFW_SHIM_HARDWARE_DIVIDER_H
#define MDFW_SHIM_HARDWARE_DIVIDER_H
#include "pico.h"
static inline uint32_t hw_divider_u32_quotient(uint32_t a, uint32_t b) {
  return b ? a / b : 0xffffffffu;
}
static inline uint32_t hw_divider_u32_remainder(uint32_t a, uint32_t b) {
  return b ? a % b : a;
}
static inline int32_t hw_divider_s32_quotient(int32_t a, int32_t b) {
  if (!b) return a < 0 ? 1 : -1;
  if (a == INT32_MIN && b == -1) return INT32_MIN;
  return a / b;
}
static inline int32_t hw_divider_s32_remainder(int32_t a, int32_t b) {
  if (!b || (a == INT32_MIN && b == -1)) return b ? 0 : a;
  return a % b;
}
#define hw_divider_u32_quotient_inlined hw_divider_u32_quotient
#define hw_divider_u32_remainder_inlined hw_divider_u32_remainder
#define hw_divider_s32_quotient_inlined hw_divider_s32_quotient
#define hw_divider_s32_remainder_inlined hw_divider_s32_remainder
#endif
