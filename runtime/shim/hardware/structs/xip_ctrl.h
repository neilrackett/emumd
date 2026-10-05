/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/structs/xip_ctrl.h: the XIP cache's
 * registers, to be read and written; its streaming FIFO is always empty
 * (EmuMD's flash is plain memory). */
#ifndef MDFW_SHIM_HARDWARE_STRUCTS_XIP_CTRL_H
#define MDFW_SHIM_HARDWARE_STRUCTS_XIP_CTRL_H
#include "hardware/regs/addressmap.h"
#include "pico.h"

#define XIP_STAT_FIFO_FULL (1u << 2)
#define XIP_STAT_FIFO_EMPTY (1u << 1)
#define XIP_STAT_FLUSH_READY (1u << 0)

typedef struct {
  volatile uint32_t ctrl, flush, stat, ctr_hit, ctr_acc, stream_addr, stream_ctr,
      stream_fifo;
} xip_ctrl_hw_t;

static xip_ctrl_hw_t mdfw_xip_ctrl_hw __attribute__((unused)) = {
    .stat = XIP_STAT_FIFO_EMPTY | XIP_STAT_FLUSH_READY};
#define xip_ctrl_hw (&mdfw_xip_ctrl_hw)

#endif
