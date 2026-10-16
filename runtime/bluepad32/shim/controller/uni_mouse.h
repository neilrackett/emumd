/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's controller/uni_mouse.h: the same layout,
 * for uni_controller_t's union. No scripted pad is a mouse. */
#ifndef MDFW_SHIM_UNI_MOUSE_H
#define MDFW_SHIM_UNI_MOUSE_H
#include <stdint.h>

#include "uni_common.h"

enum {
  UNI_MOUSE_BUTTON_LEFT = BIT(0),
  UNI_MOUSE_BUTTON_RIGHT = BIT(1),
  UNI_MOUSE_BUTTON_MIDDLE = BIT(2),
  UNI_MOUSE_BUTTON_AUX_0 = BIT(3),
  UNI_MOUSE_BUTTON_AUX_1 = BIT(4),
  UNI_MOUSE_BUTTON_AUX_2 = BIT(5),
  UNI_MOUSE_BUTTON_AUX_3 = BIT(6),
  UNI_MOUSE_BUTTON_AUX_4 = BIT(7),
  UNI_MOUSE_BUTTON_AUX_5 = BIT(8),
};

typedef struct {
  int32_t delta_x;
  int32_t delta_y;
  uint16_t buttons;
  int8_t scroll_wheel;
  uint8_t misc_buttons;
} uni_mouse_t;
#endif
