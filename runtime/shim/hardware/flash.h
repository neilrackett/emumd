/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/flash.h: the flash is mdfw_flash (2 MB),
 * mapped at XIP_BASE. Erase and program insist on the real alignment. */
#ifndef MDFW_SHIM_HARDWARE_FLASH_H
#define MDFW_SHIM_HARDWARE_FLASH_H
#include "pico.h"
#include "hardware/regs/addressmap.h"
#ifdef __cplusplus
extern "C" {
#endif
#define FLASH_PAGE_SIZE (1u << 8)
#define FLASH_SECTOR_SIZE (1u << 12)
#define FLASH_BLOCK_SIZE (1u << 16)
#define FLASH_UNIQUE_ID_SIZE_BYTES 8
void flash_range_erase(uint32_t flash_offs, size_t count);
void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count);
void flash_get_unique_id(uint8_t *id_out);
static inline void flash_flush_cache(void) {}
#ifdef __cplusplus
}
#endif
#endif
