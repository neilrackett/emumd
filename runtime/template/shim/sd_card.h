/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* The SD card driver's sd_card.h, which the SidecarTridge template's
 * sdcard.h includes: EmuMD has no SPI card (runtime/template/sdcard.c
 * works on its microSD folder), so its types are only names. */
#ifndef EMUMD_SD_CARD_H
#define EMUMD_SD_CARD_H
#include "ff.h"
typedef struct sd_card_t sd_card_t;
typedef struct spi_t spi_t;
#endif
