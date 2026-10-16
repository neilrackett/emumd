/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's platform/uni_platform.h: the callbacks a
 * custom platform gives Bluepad32, every member of the real struct, so a
 * designated initialiser builds unchanged. The scripted pads
 * (runtime/bluepad32/bluepad32.c) call them inside cyw43_arch_poll(). */
#ifndef MDFW_SHIM_UNI_PLATFORM_H
#define MDFW_SHIM_UNI_PLATFORM_H
#include <stdint.h>

#include "bt/uni_bt_conn.h"
#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_property.h"

typedef enum {
  UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON,
  UNI_PLATFORM_OOB_BLUETOOTH_ENABLED,
} uni_platform_oob_event_t;

struct uni_platform {
  const char *name;
  /* Once, from uni_init(). */
  void (*init)(int argc, const char **argv);
  /* At the first cyw43_arch_poll() after uni_init(). */
  void (*on_init_complete)(void);
  /* A pad the script connects is offered here first, as a scan would;
   * anything but UNI_ERROR_SUCCESS and it is not connected. */
  uni_error_t (*on_device_discovered)(bd_addr_t addr, const char *name, uint16_t cod,
                                      uint8_t rssi);
  void (*on_device_connected)(uni_hid_device_t *d);
  void (*on_device_disconnected)(uni_hid_device_t *d);
  /* Anything but UNI_ERROR_SUCCESS and the pad is dropped again. */
  uni_error_t (*on_device_ready)(uni_hid_device_t *d);
  void (*on_gamepad_data)(uni_hid_device_t *d, uni_gamepad_t *gp); /* deprecated */
  void (*on_controller_data)(uni_hid_device_t *d, uni_controller_t *ctl);
  const uni_property_t *(*get_property)(uni_property_idx_t idx);
  void (*on_oob_event)(uni_platform_oob_event_t event, void *data);
  void (*device_dump)(uni_hid_device_t *d);
  void (*register_console_cmds)(void);
};

/* Before uni_init(). */
void uni_platform_set_custom(struct uni_platform *platform);
struct uni_platform *uni_get_platform(void);
#endif
