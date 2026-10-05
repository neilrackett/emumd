/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/cyw43_arch.h
 * Description: Host stand-in for the Pico SDK's cyw43_arch: Wi-Fi on
 *              the Pico W, here through this computer's own network
 *              (runtime/wifi). Built as pico_cyw43_arch_lwip_poll, or
 *              _threadsafe_background with `arch = background` in
 *              mdfw.ini's [wifi].
 */
#ifndef MDFW_SHIM_PICO_CYW43_ARCH_H
#define MDFW_SHIM_PICO_CYW43_ARCH_H

#include "cyw43.h"
#include "cyw43_country.h"
#include "pico.h"
#include "pico/async_context.h"
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
#include "pico/async_context_threadsafe_background.h"
#else
#include "pico/async_context_poll.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE
#define PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE CYW43_COUNTRY_WORLDWIDE
#endif

int cyw43_arch_init(void);
int cyw43_arch_init_with_country(uint32_t country);
void cyw43_arch_deinit(void);
uint32_t cyw43_arch_get_country_code(void);

async_context_t *cyw43_arch_async_context(void);
void cyw43_arch_set_async_context(async_context_t *context);
async_context_t *cyw43_arch_init_default_async_context(void);

void cyw43_arch_enable_sta_mode(void);
void cyw43_arch_disable_sta_mode(void);
void cyw43_arch_enable_ap_mode(const char *ssid, const char *password, uint32_t auth);
void cyw43_arch_disable_ap_mode(void);

int cyw43_arch_wifi_connect_async(const char *ssid, const char *pw, uint32_t auth);
int cyw43_arch_wifi_connect_bssid_async(const char *ssid, const uint8_t *bssid,
                                        const char *pw, uint32_t auth);
int cyw43_arch_wifi_connect_blocking(const char *ssid, const char *pw, uint32_t auth);
int cyw43_arch_wifi_connect_bssid_blocking(const char *ssid, const uint8_t *bssid,
                                           const char *pw, uint32_t auth);
int cyw43_arch_wifi_connect_timeout_ms(const char *ssid, const char *pw, uint32_t auth,
                                       uint32_t timeout_ms);
int cyw43_arch_wifi_connect_bssid_timeout_ms(const char *ssid, const uint8_t *bssid,
                                             const char *pw, uint32_t auth,
                                             uint32_t timeout_ms);

void cyw43_arch_poll(void);
void cyw43_arch_wait_for_work_until(absolute_time_t until);

/* lwIP calls go between these (the async_context's lock). */
void cyw43_arch_lwip_begin(void);
void cyw43_arch_lwip_end(void);
int cyw43_arch_lwip_protect(int (*func)(void *param), void *param);
static inline void cyw43_arch_lwip_check(void) {}

/* The chip's own pins: the LED (CYW43_WL_GPIO_LED_PIN) goes to the log
 * with --md-verbose on; VBUS reads as off (no USB). */
void cyw43_arch_gpio_put(uint wl_gpio, bool value);
bool cyw43_arch_gpio_get(uint wl_gpio);

#ifdef __cplusplus
}
#endif

#endif
