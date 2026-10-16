/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's controller/uni_balance_board.h: the
 * same layout, for uni_controller_t's union. */
#ifndef MDFW_SHIM_UNI_BALANCE_BOARD_H
#define MDFW_SHIM_UNI_BALANCE_BOARD_H
#include <stdint.h>

typedef struct {
  uint16_t tr;
  uint16_t br;
  uint16_t tl;
  uint16_t bl;
  int temperature;
} uni_balance_board_t;
#endif
