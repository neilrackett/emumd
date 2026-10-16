/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's uni_property.h: the types a platform's
 * get_property callback deals in. There is no property store: a platform
 * that reads or writes properties will not build, which is the gap to
 * fill if one ever does. */
#ifndef MDFW_SHIM_UNI_PROPERTY_H
#define MDFW_SHIM_UNI_PROPERTY_H
#include <stdbool.h>
#include <stdint.h>

#include "uni_common.h"

typedef enum {
  UNI_PROPERTY_IDX_ALLOWLIST_ENABLED,
  UNI_PROPERTY_IDX_ALLOWLIST_LIST,
  UNI_PROPERTY_IDX_BLE_ENABLED,
  UNI_PROPERTY_IDX_GAP_INQ_LEN,
  UNI_PROPERTY_IDX_GAP_LEVEL,
  UNI_PROPERTY_IDX_GAP_MAX_PERIODIC_LEN,
  UNI_PROPERTY_IDX_GAP_MIN_PERIODIC_LEN,
  UNI_PROPERTY_IDX_MOUSE_SCALE,
  UNI_PROPERTY_IDX_VERSION,
  UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED,
  UNI_PROPERTY_IDX_LAST,
  /* Unijoysticle's own */
  UNI_PROPERTY_IDX_UNI_AUTOFIRE_CPS = UNI_PROPERTY_IDX_LAST,
  UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD,
  UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD,
  UNI_PROPERTY_IDX_UNI_C64_POT_MODE,
  UNI_PROPERTY_IDX_UNI_MODEL,
  UNI_PROPERTY_IDX_UNI_MOUSE_EMULATION,
  UNI_PROPERTY_IDX_UNI_SERIAL_NUMBER,
  UNI_PROPERTY_IDX_UNI_VENDOR,
  UNI_PROPERTY_IDX_UNI_LAST,
  UNI_PROPERTY_IDX_COUNT = UNI_PROPERTY_IDX_UNI_LAST
} uni_property_idx_t;

typedef enum {
  UNI_PROPERTY_TYPE_BOOL,
  UNI_PROPERTY_TYPE_U8,
  UNI_PROPERTY_TYPE_U32,
  UNI_PROPERTY_TYPE_FLOAT,
  UNI_PROPERTY_TYPE_STRING,
} uni_property_type_t;

typedef union {
  bool boolean;
  uint8_t u8;
  uint32_t u32;
  float f32;
  const char *str;
} uni_property_value_t;

typedef enum {
  UNI_PROPERTY_FLAG_READ_ONLY = BIT(0),
} uni_property_flag_t;

typedef struct {
  uni_property_idx_t idx;
  const char *name;
  uni_property_type_t type;
  uni_property_value_t default_value;
  uni_property_flag_t flags;
} uni_property_t;
#endif
