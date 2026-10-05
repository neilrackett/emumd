/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: sdcard.c
 * Description: The SidecarTridge template's sdcard.c (its sdcard.h API) on
 *              EmuMD's microSD card, the --md-sd folder through FatFs:
 *              mounting, folders and sizes. The SPI speed settings are
 *              accepted and ignored.
 */

#if __has_include("sdcard.h")

#include "ff.h"
#include "mdfw.h"
#include "sdcard.h"

static bool s_mounted;

FRESULT sdcard_mountFilesystem(FATFS *fsys, const char *drive) {
  const FRESULT fr = f_mount(fsys, drive, 1);
  s_mounted = fr == FR_OK;
  return fr;
}

bool sdcard_dirExist(const char *dir) {
  FILINFO info;
  return f_stat(dir, &info) == FR_OK && (info.fattrib & AM_DIR);
}

sdcard_status_t sdcard_ensureFolder(const char *folderName) {
  if (sdcard_dirExist(folderName)) return SDCARD_INIT_OK;
  return f_mkdir(folderName) == FR_OK ? SDCARD_INIT_OK : SDCARD_CREATE_FOLDER_ERROR;
}

sdcard_status_t sdcard_initFilesystem(FATFS *fsPtr, const char *folderName) {
  if (sdcard_mountFilesystem(fsPtr, "0:") != FR_OK) return SDCARD_MOUNT_ERROR;
  return sdcard_ensureFolder(folderName);
}

void sdcard_changeSpiSpeed(int baudRateKbits) { (void)baudRateKbits; }
void sdcard_setSpiSpeedSettings() {}

void sdcard_getInfo(FATFS *fsPtr, uint32_t *totalSizeMb, uint32_t *freeSpaceMb) {
  (void)fsPtr;
  DWORD free_clusters = 0;
  FATFS *fs = NULL;
  f_getfree("0:", &free_clusters, &fs);
  const uint64_t cluster = (uint64_t)fs->csize * NUM_BYTES_PER_SECTOR;
  if (totalSizeMb) *totalSizeMb = (uint32_t)((fs->n_fatent - 2u) * cluster / SDCARD_MEGABYTE);
  if (freeSpaceMb) *freeSpaceMb = (uint32_t)(free_clusters * cluster / SDCARD_MEGABYTE);
}

bool sdcard_isMounted(void) { return s_mounted; }

bool sdcard_getMountedInfo(uint32_t *totalSizeMb, uint32_t *freeSpaceMb) {
  if (!s_mounted) return false;
  sdcard_getInfo(NULL, totalSizeMb, freeSpaceMb);
  return true;
}

#endif
