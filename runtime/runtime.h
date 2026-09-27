/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: runtime.h
 * Description: Internal calls between the md-emulator runtime's parts.
 */
#ifndef MDFW_RUNTIME_H
#define MDFW_RUNTIME_H
#include <stdint.h>

void mdfw_runtime_sleep(uint64_t us);
void mdfw_runtime_reboot(void);
uint32_t mdfw_runtime_ring_head(void);
const uint16_t *mdfw_runtime_ring(void);

void mdfw_runtime_flash_power_on(void);
void mdfw_runtime_flash_power_off(void);
void mdfw_runtime_fatfs_reset(void);
void mdfw_runtime_multicore_stop(void);

#include <stdbool.h>
#include <stddef.h>
void *mdfw_hostdir_open(const char *path);
void mdfw_hostdir_close(void *d);
void mdfw_hostdir_rewind(void *d);
const char *mdfw_hostdir_next(void *d);
bool mdfw_hostdir_find(const char *dir, const char *name, char *found,
                       size_t foundlen);

#endif
