/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: ff.h
 * Description: Host stand-in for FatFs (ChaN's ff.h, as the SidecarTridge
 *              firmwares use it through no-OS-FatFS). The calls work on
 *              the folder given with --md-sd; paths ignore case, as on
 *              FAT, and any "0:" drive prefix. Types, flags and result
 *              codes match FatFs R0.15 with long file names on.
 */
#ifndef MDFW_SHIM_FF_H
#define MDFW_SHIM_FF_H
#include "pico.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef WORD WCHAR;
typedef char TCHAR;
typedef DWORD LBA_t;
typedef QWORD FSIZE_t;
#define _T(x) x
#define _TEXT(x) x
#define FF_MAX_LFN 255

typedef enum {
  FR_OK = 0,
  FR_DISK_ERR,
  FR_INT_ERR,
  FR_NOT_READY,
  FR_NO_FILE,
  FR_NO_PATH,
  FR_INVALID_NAME,
  FR_DENIED,
  FR_EXIST,
  FR_INVALID_OBJECT,
  FR_WRITE_PROTECTED,
  FR_INVALID_DRIVE,
  FR_NOT_ENABLED,
  FR_NO_FILESYSTEM,
  FR_MKFS_ABORTED,
  FR_TIMEOUT,
  FR_LOCKED,
  FR_NOT_ENOUGH_CORE,
  FR_TOO_MANY_OPEN_FILES,
  FR_INVALID_PARAMETER
} FRESULT;

typedef struct {
  BYTE fs_type; /* non-zero once mounted */
  BYTE pdrv;
  WORD csize;   /* sectors per cluster, for f_getfree users */
  DWORD n_fatent;
  DWORD free_clst;
} FATFS;

typedef struct {
  FATFS *fs;
  WORD id;
  BYTE attr;
  BYTE stat;
  DWORD sclust;
  FSIZE_t objsize;
} FFOBJID;

typedef struct {
  FFOBJID obj;
  BYTE flag;
  BYTE err;
  FSIZE_t fptr;
  void *host; /* FILE * */
} FIL;

typedef struct {
  FFOBJID obj;
  DWORD dptr;
  void *host;          /* DIR * */
  char path[1024];     /* host path of the directory */
  const TCHAR *pat;    /* f_findfirst pattern */
} DIR;

typedef struct {
  FSIZE_t fsize;
  WORD fdate;
  WORD ftime;
  BYTE fattrib;
  TCHAR altname[13];
  TCHAR fname[FF_MAX_LFN + 1];
} FILINFO;

#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW 0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS 0x10
#define FA_OPEN_APPEND 0x30

#define AM_RDO 0x01
#define AM_HID 0x02
#define AM_SYS 0x04
#define AM_DIR 0x10
#define AM_ARC 0x20

#define FS_FAT12 1
#define FS_FAT16 2
#define FS_FAT32 3
#define FS_EXFAT 4

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT f_close(FIL *fp);
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);
FRESULT f_truncate(FIL *fp);
FRESULT f_sync(FIL *fp);
FRESULT f_opendir(DIR *dp, const TCHAR *path);
FRESULT f_closedir(DIR *dp);
FRESULT f_readdir(DIR *dp, FILINFO *fno);
FRESULT f_findfirst(DIR *dp, FILINFO *fno, const TCHAR *path, const TCHAR *pattern);
FRESULT f_findnext(DIR *dp, FILINFO *fno);
FRESULT f_mkdir(const TCHAR *path);
FRESULT f_unlink(const TCHAR *path);
FRESULT f_rename(const TCHAR *path_old, const TCHAR *path_new);
FRESULT f_stat(const TCHAR *path, FILINFO *fno);
FRESULT f_chdir(const TCHAR *path);
FRESULT f_chdrive(const TCHAR *path);
FRESULT f_getcwd(TCHAR *buff, UINT len);
FRESULT f_getfree(const TCHAR *path, DWORD *nclst, FATFS **fatfs);
FRESULT f_getlabel(const TCHAR *path, TCHAR *label, DWORD *vsn);
FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt);
#define f_unmount(path) f_mount(0, path, 0)
int f_putc(TCHAR c, FIL *fp);
int f_puts(const TCHAR *str, FIL *cp);
int f_printf(FIL *fp, const TCHAR *str, ...) __attribute__((format(printf, 2, 3)));
TCHAR *f_gets(TCHAR *buff, int len, FIL *fp);
const char *FRESULT_str(FRESULT i);

#define f_eof(fp) ((int)((fp)->fptr == (fp)->obj.objsize))
#define f_error(fp) ((fp)->err)
#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->obj.objsize)
#define f_rewind(fp) f_lseek((fp), 0)
#define f_rewinddir(dp) f_readdir((dp), 0)
#define f_rmdir(path) f_unlink(path)
#ifdef __cplusplus
}
#endif
#endif
