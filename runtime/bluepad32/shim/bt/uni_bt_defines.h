/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's bt/uni_bt_defines.h: the Class of Device
 * and appearance values a platform's on_device_discovered looks at. */
#ifndef MDFW_SHIM_UNI_BT_DEFINES_H
#define MDFW_SHIM_UNI_BT_DEFINES_H
#include "uni_common.h"

#define UNI_BT_COD_MAJOR_MASK 0x1f00
#define UNI_BT_COD_MAJOR_PERIPHERAL 0x0500
#define UNI_BT_COD_MAJOR_AUDIO_VIDEO 0x0400

#define UNI_BT_COD_MINOR_MASK 0xfc
#define UNI_BT_COD_MINOR_KEYBOARD_AND_MICE 0xc0
#define UNI_BT_COD_MINOR_MICE 0x80
#define UNI_BT_COD_MINOR_KEYBOARD 0x40
#define UNI_BT_COD_MINOR_SENSING_DEVICE 0x10
#define UNI_BT_COD_MINOR_REMOTE_CONTROL 0x0c
#define UNI_BT_COD_MINOR_GAMEPAD 0x08
#define UNI_BT_COD_MINOR_JOYSTICK 0x04

#define UNI_BT_HID_APPEARANCE_GENERIC 0x3c0
#define UNI_BT_HID_APPEARANCE_KEYBOARD 0x3c1
#define UNI_BT_HID_APPEARANCE_MOUSE 0x3c2
#define UNI_BT_HID_APPEARANCE_JOYSTICK 0x3c3
#define UNI_BT_HID_APPEARANCE_GAMEPAD 0x3c4
#endif
