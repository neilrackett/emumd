/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's controller/uni_controller.h: what a
 * controller reports, by class. The scripted pads are all gamepads. */
#ifndef MDFW_SHIM_UNI_CONTROLLER_H
#define MDFW_SHIM_UNI_CONTROLLER_H
#include <stdint.h>

#include "controller/uni_balance_board.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "controller/uni_mouse.h"
#include "uni_common.h"

typedef enum {
  UNI_CONTROLLER_CLASS_NONE = 0,
  UNI_CONTROLLER_CLASS_GAMEPAD,
  UNI_CONTROLLER_CLASS_MOUSE,
  UNI_CONTROLLER_CLASS_KEYBOARD,
  UNI_CONTROLLER_CLASS_BALANCE_BOARD,
  UNI_CONTROLLER_CLASS_COUNT,
} uni_controller_class_t;

typedef enum {
  CONTROLLER_SUBTYPE_NONE = 0,
  CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL,
  CONTROLLER_SUBTYPE_WIIMOTE_VERTICAL,
  CONTROLLER_SUBTYPE_WIIMOTE_ACCEL,
  CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK,
  CONTROLLER_SUBTYPE_UNUSED_00,
  CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK_ACCEL,
  CONTROLLER_SUBTYPE_WII_CLASSIC,
  CONTROLLER_SUBTYPE_WIIUPRO,
  CONTROLLER_SUBTYPE_WII_BALANCE_BOARD,
  CONTROLLER_SUBTYPE_WIIMOTE_UDRAW_TABLET,
  CONTROLLER_SUBTYPE_EXAMPLE_OF_NEW_GAMEPAD = 20,
} uni_controller_subtype_t;

enum {
  UNI_CONTROLLER_BATTERY_NOT_AVAILABLE = 0,
  UNI_CONTROLLER_BATTERY_EMPTY = 1,
  UNI_CONTROLLER_BATTERY_FULL = 255,
};

typedef struct {
  uni_controller_class_t klass;
  union {
    uni_gamepad_t gamepad;
    uni_mouse_t mouse;
    uni_balance_board_t balance_board;
    uni_keyboard_t keyboard;
  };
  uint8_t battery; /* UNI_CONTROLLER_BATTERY_NOT_AVAILABLE here */
} uni_controller_t;
#endif
