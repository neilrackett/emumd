/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: uni_hid_device.h
 * Description: Host stand-in for Bluepad32's uni_hid_device.h: a
 *              connected controller, as the platform callbacks receive
 *              it. The fields a platform reads (the name, the controller
 *              data, the report parser whose LED and rumble calls it
 *              makes, the connection, its own platform_data) are here
 *              under their real names; BTstack's timers, the HID
 *              descriptor and the parsers' private data are not. The
 *              table is runtime/bluepad32/bluepad32.c's, so a device's
 *              index is its place in it, as in Bluepad32.
 */
#ifndef MDFW_SHIM_UNI_HID_DEVICE_H
#define MDFW_SHIM_UNI_HID_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "parser/uni_hid_parser.h"
#include "uni_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HID_MAX_NAME_LEN 240
#define HID_DEVICE_MAX_PLATFORM_DATA 256

struct uni_hid_device_s {
  uint32_t cod; /* Class of Device: UNI_BT_COD_* */
  uint16_t vendor_id;
  uint16_t product_id;
  char name[HID_MAX_NAME_LEN];
  uint32_t flags;

  uni_controller_type_t controller_type;
  uni_controller_subtype_t controller_subtype;
  uni_controller_t controller; /* what on_controller_data is given */

  /* What the parser can do for this controller: set_player_leds and
   * play_dual_rumble log what they are asked; NULL where the pad has no
   * such thing (the script's `norumble`). */
  uni_report_parser_t report_parser;

  uint8_t platform_data[HID_DEVICE_MAX_PLATFORM_DATA]; /* the platform's own */

  uni_bt_conn_t conn;

  struct uni_hid_device_s *parent; /* a virtual device's; never here */
  struct uni_hid_device_s *child;
};
typedef struct uni_hid_device_s uni_hid_device_t;

/* The device at an index (0 to CONFIG_BLUEPAD32_MAX_DEVICES - 1, else
 * NULL), connected or not, and the index of a device. */
uni_hid_device_t *uni_hid_device_get_instance_for_idx(int idx);
int uni_hid_device_get_idx_for_instance(const uni_hid_device_t *d);

bool uni_hid_device_is_gamepad(const uni_hid_device_t *d);
bool uni_hid_device_is_mouse(const uni_hid_device_t *d);
bool uni_hid_device_is_keyboard(const uni_hid_device_t *d);
bool uni_hid_device_is_virtual_device(const uni_hid_device_t *d);
bool uni_hid_device_has_name(const uni_hid_device_t *d);
uint16_t uni_hid_device_get_vendor_id(const uni_hid_device_t *d);
uint16_t uni_hid_device_get_product_id(const uni_hid_device_t *d);

#ifdef __cplusplus
}
#endif

#endif
