/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: cyw43.h
 * Description: Host stand-in for the Pico W's Wi-Fi chip driver
 *              (runtime/wifi/cyw43.c): the same names and values, so
 *              firmware code builds unchanged, over lwIP and this
 *              computer's own network. Written for EmuMD; none of the
 *              chip's driver is used.
 */
#ifndef MDFW_SHIM_CYW43_H
#define MDFW_SHIM_CYW43_H

#include "cyw43_country.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CYW43_LWIP
#define CYW43_LWIP 1
#endif
#ifndef CYW43_VERBOSE_DEBUG
#define CYW43_VERBOSE_DEBUG 0
#endif
#ifndef CYW43_HOST_NAME
#define CYW43_HOST_NAME "PicoW"
#endif

/* Interfaces */
enum {
  CYW43_ITF_STA, /* a client of an access point */
  CYW43_ITF_AP,  /* an access point (not emulated) */
};

/* Link status (cyw43_wifi_link_status, cyw43_tcpip_link_status) */
#define CYW43_LINK_DOWN (0)
#define CYW43_LINK_JOIN (1)
#define CYW43_LINK_NOIP (2)
#define CYW43_LINK_UP (3)
#define CYW43_LINK_FAIL (-1)
#define CYW43_LINK_NONET (-2)
#define CYW43_LINK_BADAUTH (-3)

/* Authorisation */
#define CYW43_AUTH_OPEN (0)
#define CYW43_AUTH_WPA_TKIP_PSK (0x00200002)
#define CYW43_AUTH_WPA2_AES_PSK (0x00400004)
#define CYW43_AUTH_WPA2_MIXED_PSK (0x00400006)
#define CYW43_AUTH_WPA3_SAE_AES_PSK (0x01000004)
#define CYW43_AUTH_WPA3_WPA2_AES_PSK (0x01400004)

#define CYW43_CHANNEL_NONE (0xffffffff)

/* Power management (accepted, and remembered for cyw43_wifi_get_pm) */
#define CYW43_NO_POWERSAVE_MODE (0)
#define CYW43_PM1_POWERSAVE_MODE (1)
#define CYW43_PM2_POWERSAVE_MODE (2)
static inline uint32_t cyw43_pm_value(uint8_t pm_mode, uint16_t pm2_sleep_ret_ms,
                                      uint8_t li_beacon_period, uint8_t li_dtim_period,
                                      uint8_t li_assoc) {
  return (uint32_t)li_assoc << 20 | (uint32_t)li_dtim_period << 16 |
         (uint32_t)li_beacon_period << 12 | (uint32_t)(pm2_sleep_ret_ms / 10) << 4 |
         pm_mode;
}
#define CYW43_PERFORMANCE_PM (cyw43_pm_value(CYW43_PM2_POWERSAVE_MODE, 200, 1, 1, 10))
#define CYW43_DEFAULT_PM (CYW43_PERFORMANCE_PM)
#define CYW43_NONE_PM (cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 10, 0, 0, 0))
#define CYW43_AGGRESSIVE_PM (cyw43_pm_value(CYW43_PM1_POWERSAVE_MODE, 10, 0, 0, 0))

/* A network found by a scan. auth_mode: 0 open, | 1 WEP, | 2 WPA, | 4 WPA2. */
typedef struct _cyw43_ev_scan_result_t {
  uint8_t bssid[6];
  uint8_t ssid_len;
  uint8_t ssid[32];
  uint16_t channel;
  uint8_t auth_mode;
  int16_t rssi;
} cyw43_ev_scan_result_t;

typedef struct _cyw43_wifi_scan_options_t {
  uint32_t version;
  uint16_t action;
  uint32_t ssid_len; /* 0: every network */
  uint8_t ssid[32];
  uint8_t bssid[6];
  int8_t bss_type;
  int8_t scan_type; /* 0 active, 1 passive */
  int32_t nprobes;
  int32_t active_time;
  int32_t passive_time;
  int32_t home_time;
  int32_t channel_num;
  uint16_t channel_list[1];
} cyw43_wifi_scan_options_t;

typedef struct _cyw43_t {
  struct netif netif[2]; /* CYW43_ITF_STA, CYW43_ITF_AP */
} cyw43_t;

extern cyw43_t cyw43_state;

bool cyw43_is_initialized(cyw43_t *self);
int cyw43_wifi_set_up(cyw43_t *self, int itf, bool up, uint32_t country);
int cyw43_wifi_join(cyw43_t *self, size_t ssid_len, const uint8_t *ssid, size_t key_len,
                    const uint8_t *key, uint32_t auth_type, const uint8_t *bssid,
                    uint32_t channel);
int cyw43_wifi_leave(cyw43_t *self, int itf);
int cyw43_wifi_link_status(cyw43_t *self, int itf);
int cyw43_tcpip_link_status(cyw43_t *self, int itf);
int cyw43_wifi_scan(cyw43_t *self, cyw43_wifi_scan_options_t *opts, void *env,
                    int (*result_cb)(void *, const cyw43_ev_scan_result_t *));
bool cyw43_wifi_scan_active(cyw43_t *self);
int cyw43_wifi_get_mac(cyw43_t *self, int itf, uint8_t mac[6]);
int cyw43_wifi_get_bssid(cyw43_t *self, uint8_t bssid[6]);
int cyw43_wifi_get_rssi(cyw43_t *self, int32_t *rssi);
int cyw43_wifi_pm(cyw43_t *self, uint32_t pm);
int cyw43_wifi_get_pm(cyw43_t *self, uint32_t *pm);
/* A whole Ethernet frame (`buf` is a struct pbuf * when is_pbuf). */
int cyw43_send_ethernet(cyw43_t *self, int itf, size_t len, const void *buf, bool is_pbuf);
int cyw43_gpio_set(cyw43_t *self, int gpio, bool val);
int cyw43_gpio_get(cyw43_t *self, int gpio, bool *val);

#ifdef __cplusplus
}
#endif

#endif
