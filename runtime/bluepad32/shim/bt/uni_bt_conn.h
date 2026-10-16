/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's bt/uni_bt_conn.h: a device's Bluetooth
 * connection, and the two BTstack types it is made of (bluetooth.h's
 * bd_addr_t and hci_con_handle_t), so no BTstack header is needed. */
#ifndef MDFW_SHIM_UNI_BT_CONN_H
#define MDFW_SHIM_UNI_BT_CONN_H
#include <stdbool.h>
#include <stdint.h>

#ifndef BD_ADDR_LEN
#define BD_ADDR_LEN 6
typedef uint8_t bd_addr_t[BD_ADDR_LEN];
typedef uint16_t hci_con_handle_t;
#endif

#define UNI_BT_CONN_HANDLE_INVALID 0xffff

typedef enum {
  UNI_BT_CONN_PROTOCOL_NONE,
  UNI_BT_CONN_PROTOCOL_BR_EDR,
  UNI_BT_CONN_PROTOCOL_BLE,
} uni_bt_conn_protocol_t;

typedef enum {
  UNI_BT_CONN_STATE_DEVICE_NONE,
  UNI_BT_CONN_STATE_DEVICE_DISCOVERED,
  UNI_BT_CONN_STATE_REMOTE_NAME_REQUEST,
  UNI_BT_CONN_STATE_REMOTE_NAME_INQUIRED,
  UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED,
  UNI_BT_CONN_STATE_SDP_VENDOR_REQUESTED,
  UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED,
  UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_REQUESTED,
  UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED,
  UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTION_REQUESTED,
  UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTED,
  UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTION_REQUESTED,
  UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED,
  UNI_BT_CONN_STATE_DEVICE_PENDING_READY,
  UNI_BT_CONN_STATE_DEVICE_READY,
} uni_bt_conn_state_t;

typedef struct {
  bd_addr_t btaddr;
  hci_con_handle_t handle;
  uint16_t control_cid;
  uint16_t interrupt_cid;
  uint8_t page_scan_repetition_mode;
  uint16_t clock_offset;
  uint8_t rssi;
  bool incoming;
  bool connected;
  uni_bt_conn_state_t state;
  uni_bt_conn_protocol_t protocol;
} uni_bt_conn_t;

static inline uni_bt_conn_state_t uni_bt_conn_get_state(const uni_bt_conn_t *conn) {
  return conn->state;
}
static inline bool uni_bt_conn_is_connected(const uni_bt_conn_t *conn) {
  return conn->connected;
}
static inline bool uni_bt_conn_is_incoming(const uni_bt_conn_t *conn) {
  return conn->incoming;
}
#endif
