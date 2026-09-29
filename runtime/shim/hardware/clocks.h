/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/clocks.h: clock changes are accepted and
 * remembered, nothing more. */
#ifndef MDFW_SHIM_HARDWARE_CLOCKS_H
#define MDFW_SHIM_HARDWARE_CLOCKS_H
#include "pico.h"
#ifdef __cplusplus
extern "C" {
#endif
enum clock_index { clk_gpout0, clk_gpout1, clk_gpout2, clk_gpout3, clk_ref,
  clk_sys, clk_peri, clk_usb, clk_adc, clk_rtc, CLK_COUNT };
bool set_sys_clock_khz(uint32_t freq_khz, bool required);
uint32_t clock_get_hz(enum clock_index clk_index);
static inline uint32_t frequency_count_khz(uint src) { (void)src; return clock_get_hz(clk_sys) / 1000u; }
#ifdef __cplusplus
}
#endif
#endif
