/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: runtime.h
 * Description: Internal calls between the EmuMD runtime's parts.
 */
#ifndef MDFW_RUNTIME_H
#define MDFW_RUNTIME_H
#include <stdint.h>

#include <stdbool.h>

void mdfw_runtime_sleep(uint64_t us);
void mdfw_runtime_sleep_until(uint64_t target_us);
void mdfw_runtime_reboot(void);

/* The firmware's own threads (main and core 1) mark themselves. */
void mdfw_runtime_enter_thread(void);
bool mdfw_runtime_own_thread(void);

/* Pico SDK timers and alarms, fired on the emulator's thread. */
void mdfw_runtime_run_timers(uint64_t now_us);
void mdfw_runtime_timers_reset(void);

void mdfw_runtime_watchdog_power_on(bool cold);
void mdfw_runtime_flash_power_on(void);
void mdfw_runtime_flash_power_off(void);
void mdfw_runtime_fatfs_reset(void);
void mdfw_runtime_multicore_stop(void);

#include <stddef.h>
void *mdfw_hostdir_open(const char *path);
void mdfw_hostdir_close(void *d);
void mdfw_hostdir_rewind(void *d);
const char *mdfw_hostdir_next(void *d);
bool mdfw_hostdir_find(const char *dir, const char *name, char *found,
                       size_t foundlen);

#endif
