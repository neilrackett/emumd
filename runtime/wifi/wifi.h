/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: wifi.h
 * Description: Calls between the parts of EmuMD's Wi-Fi: the async_context
 *              lock and work (async_context.c), the chip (cyw43.c) and
 *              the network on the far side of its radio (net.c), which is
 *              this computer's own, through libslirp: a virtual router
 *              (10.0.2.2, which is also this computer), DHCP (10.0.2.15
 *              for the device) and DNS (10.0.2.3).
 */
#ifndef MDFW_WIFI_H
#define MDFW_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pico/async_context.h"

/* The one lock for every async_context, and so for lwIP and the network.
 * Recursive. A firmware thread is never stopped while it holds it. */
void mdfw_wifi_lock(void);
void mdfw_wifi_unlock(void);
bool mdfw_wifi_trylock(void);

/* cyw43.c, for async_context.c (lock held): mark the chip's worker
 * pending if it has something to do, and when lwIP next wants a turn. */
void mdfw_wifi_check(async_context_t *context);
absolute_time_t mdfw_wifi_next_time(async_context_t *context);

/* net.c. All but mdfw_net_wait are called with the lock held. */
bool mdfw_net_start(void);
void mdfw_net_stop(void);
/* A frame from the device to the network. */
void mdfw_net_send(const void *frame, size_t len);
/* Service this computer's side and hand any frames for the device to
 * `deliver`; never waits. True if there were any. */
bool mdfw_net_service(void (*deliver)(const uint8_t *frame, size_t len));
/* Is there anything for mdfw_net_service to do? */
bool mdfw_net_ready(void);
/* Wait (really) up to `us` microseconds for the network. */
void mdfw_net_wait(unsigned us);

#endif
