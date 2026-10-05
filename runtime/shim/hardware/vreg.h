/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/vreg.h: the core voltage, accepted and
 * ignored. */
#ifndef MDFW_SHIM_HARDWARE_VREG_H
#define MDFW_SHIM_HARDWARE_VREG_H
#include "pico.h"
enum vreg_voltage {
  VREG_VOLTAGE_0_85 = 6, VREG_VOLTAGE_0_90, VREG_VOLTAGE_0_95, VREG_VOLTAGE_1_00,
  VREG_VOLTAGE_1_05, VREG_VOLTAGE_1_10, VREG_VOLTAGE_1_15, VREG_VOLTAGE_1_20,
  VREG_VOLTAGE_1_25, VREG_VOLTAGE_1_30,
  VREG_VOLTAGE_MIN = VREG_VOLTAGE_0_85, VREG_VOLTAGE_DEFAULT = VREG_VOLTAGE_1_10,
  VREG_VOLTAGE_MAX = VREG_VOLTAGE_1_30 };
static inline void vreg_set_voltage(enum vreg_voltage v) { (void)v; }
#endif
