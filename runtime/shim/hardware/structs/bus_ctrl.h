/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/structs/bus_ctrl.h: the bus fabric's
 * priority register, to be written (and ignored). */
#ifndef MDFW_SHIM_HARDWARE_STRUCTS_BUS_CTRL_H
#define MDFW_SHIM_HARDWARE_STRUCTS_BUS_CTRL_H
#include "pico.h"

#define BUSCTRL_BUS_PRIORITY_PROC0_BITS (1u << 0)
#define BUSCTRL_BUS_PRIORITY_PROC1_BITS (1u << 4)
#define BUSCTRL_BUS_PRIORITY_DMA_R_BITS (1u << 8)
#define BUSCTRL_BUS_PRIORITY_DMA_W_BITS (1u << 12)

typedef struct {
  volatile uint32_t priority, priority_ack;
} bus_ctrl_hw_t;

static bus_ctrl_hw_t mdfw_bus_ctrl_hw __attribute__((unused));
#define bus_ctrl_hw (&mdfw_bus_ctrl_hw)

#endif
