/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's uni_error.h: the same values. */
#ifndef MDFW_SHIM_UNI_ERROR_H
#define MDFW_SHIM_UNI_ERROR_H
typedef enum {
  UNI_ERROR_SUCCESS = 0,
  UNI_ERROR_INVALID_DEVICE = 1,
  UNI_ERROR_INVALID_CONTROLLER = 2,
  UNI_ERROR_NO_SLOTS = 3,
  UNI_ERROR_INIT_FAILED = 4,
  UNI_ERROR_IGNORE_DEVICE = 5,
} uni_error_t;
#endif
