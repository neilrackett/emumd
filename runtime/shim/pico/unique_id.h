/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/unique_id.h
 * Description: Host stand-in for the Pico SDK's board ID: E6614103E74D4401,
 *              or the 16 hex digits of --md-option board_id=.
 */
#ifndef MDFW_SHIM_PICO_UNIQUE_ID_H
#define MDFW_SHIM_PICO_UNIQUE_ID_H

#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8

typedef struct {
  uint8_t id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
} pico_unique_board_id_t;

void pico_get_unique_board_id(pico_unique_board_id_t *id_out);
/* Upper-case hex, cut to fit `len` bytes with its NUL. */
void pico_get_unique_board_id_string(char *id_out, uint len);

#ifdef __cplusplus
}
#endif

#endif
