/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/error.h
 * Description: The Pico SDK's error codes (pico.h includes this, as the
 *              SDK's does).
 */
#ifndef MDFW_SHIM_PICO_ERROR_H
#define MDFW_SHIM_PICO_ERROR_H

enum pico_error_codes {
  PICO_OK = 0,
  PICO_ERROR_NONE = 0,
  PICO_ERROR_GENERIC = -1,
  PICO_ERROR_TIMEOUT = -2,
  PICO_ERROR_NO_DATA = -3,
  PICO_ERROR_NOT_PERMITTED = -4,
  PICO_ERROR_INVALID_ARG = -5,
  PICO_ERROR_IO = -6,
  PICO_ERROR_BADAUTH = -7,
  PICO_ERROR_CONNECT_FAILED = -8,
  PICO_ERROR_INSUFFICIENT_RESOURCES = -9,
  PICO_ERROR_INVALID_ADDRESS = -10,
  PICO_ERROR_BAD_ALIGNMENT = -11,
  PICO_ERROR_INVALID_STATE = -12,
  PICO_ERROR_BUFFER_TOO_SMALL = -13,
  PICO_ERROR_PRECONDITION_NOT_MET = -14,
  PICO_ERROR_MODIFIED_DATA = -15,
};

#endif
