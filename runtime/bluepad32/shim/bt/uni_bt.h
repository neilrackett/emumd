/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's bt/uni_bt.h: what a platform asks of the
 * Bluetooth side. The _safe calls, meant for another task, are done at
 * the next cyw43_arch_poll(), in order; the _unsafe ones, meant for
 * BTstack's own callbacks, at once. There is no radio behind them:
 * scanning and forgotten bonds are logged, and a disconnect drops the
 * scripted pad in that slot. */
#ifndef MDFW_SHIM_UNI_BT_H
#define MDFW_SHIM_UNI_BT_H
#include <stdbool.h>

#include "bt/uni_bt_conn.h"

void uni_bt_del_keys_safe(void);
void uni_bt_del_keys_unsafe(void);
void uni_bt_start_scanning_and_autoconnect_safe(void);
void uni_bt_start_scanning_and_autoconnect_unsafe(void);
void uni_bt_stop_scanning_safe(void);
void uni_bt_stop_scanning_unsafe(void);
bool uni_bt_is_scanning(void);
void uni_bt_allow_incoming_connections(bool allow);
bool uni_bt_incoming_connections_is_allowed(void);
void uni_bt_disconnect_device_safe(int device_idx);
void uni_bt_get_local_bd_addr_safe(bd_addr_t addr);
#endif
