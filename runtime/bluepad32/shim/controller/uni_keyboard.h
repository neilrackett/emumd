/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's controller/uni_keyboard.h: the same
 * layout, for uni_controller_t's union. No scripted pad is a keyboard. */
#ifndef MDFW_SHIM_UNI_KEYBOARD_H
#define MDFW_SHIM_UNI_KEYBOARD_H
#include <stdint.h>

#include "uni_common.h"

#define UNI_KEYBOARD_PRESSED_KEYS_MAX 10

enum {
  UNI_KEYBOARD_MODIFIER_LEFT_CONTROL = BIT(0),
  UNI_KEYBOARD_MODIFIER_LEFT_SHIFT = BIT(1),
  UNI_KEYBOARD_MODIFIER_LEFT_ALT = BIT(2),
  UNI_KEYBOARD_MODIFIER_LEFT_GUI = BIT(3),
  UNI_KEYBOARD_MODIFIER_RIGHT_CONTROL = BIT(4),
  UNI_KEYBOARD_MODIFIER_RIGHT_SHIFT = BIT(5),
  UNI_KEYBOARD_MODIFIER_RIGHT_ALT = BIT(6),
  UNI_KEYBOARD_MODIFIER_RIGHT_GUI = BIT(7),
};

typedef struct {
  uint8_t modifiers;
  uint8_t pressed_keys[UNI_KEYBOARD_PRESSED_KEYS_MAX];
  uint8_t reserved[16];
} uni_keyboard_t;
#endif
