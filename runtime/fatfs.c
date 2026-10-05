/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: fatfs.c
 * Description: FatFs (shim/ff.h) on a host folder: the microSD card given
 *              with --md-sd. Paths ignore case, as on FAT, so a firmware
 *              opening /ROTT/HUNTBGIN.WAD finds rott/huntbgin.wad; a
 *              leading "0:" is dropped; relative paths use f_chdir's
 *              folder.
 */

#include "ff.h"

#include <errno.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pico.h"
#include "runtime.h"

#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif

static char s_cwd[1024] = "/";

void mdfw_runtime_fatfs_reset(void) { strcpy(s_cwd, "/"); }

/* ------------------------------------------------------------------ */
/* Paths                                                                */
/* ------------------------------------------------------------------ */

/* Normalise a FatFs path to an absolute, slash-separated card path. */
static FRESULT card_path(const TCHAR *path, char *out, size_t outlen) {
  char tmp[2048];
  if (!path) return FR_INVALID_NAME;
  if (path[0] && path[1] == ':') path += 2; /* "0:/foo" */
  if (path[0] == '/' || path[0] == '\\') {
    snprintf(tmp, sizeof(tmp), "%s", path);
  } else {
    snprintf(tmp, sizeof(tmp), "%s/%s", s_cwd, path);
  }
  for (char *p = tmp; *p; p++) {
    if (*p == '\\') *p = '/';
  }
  /* Resolve "." and "..". */
  char *parts[128];
  int n = 0;
  for (char *tok = strtok(tmp, "/"); tok; tok = strtok(NULL, "/")) {
    if (strcmp(tok, ".") == 0 || tok[0] == 0) continue;
    if (strcmp(tok, "..") == 0) {
      if (n > 0) n--;
      continue;
    }
    if (n == 128) return FR_INVALID_NAME;
    parts[n++] = tok;
  }
  size_t len = 0;
  out[0] = 0;
  for (int i = 0; i < n; i++) {
    const int w = snprintf(out + len, outlen - len, "/%s", parts[i]);
    if (w < 0 || (size_t)w >= outlen - len) return FR_INVALID_NAME;
    len += (size_t)w;
  }
  if (n == 0) snprintf(out, outlen, "/");
  return FR_OK;
}

/* Map a FatFs path to a host path. With `may_be_new`, the last part need
 * not exist (it is created with the name as given). */
static FRESULT host_path(const TCHAR *path, char *out, size_t outlen,
                         bool may_be_new) {
  char card[1024];
  FRESULT fr = card_path(path, card, sizeof(card));
  if (fr != FR_OK) return fr;
  snprintf(out, outlen, "%s", mdfw_sd_root());
  if (strcmp(card, "/") == 0) return FR_OK;
  char *rest = card + 1;
  while (rest && *rest) {
    char *slash = strchr(rest, '/');
    if (slash) *slash = 0;
    char found[512];
    const size_t used = strlen(out);
    if (mdfw_hostdir_find(out, rest, found, sizeof(found))) {
      snprintf(out + used, outlen - used, "/%s", found);
    } else if (!slash && may_be_new) {
      snprintf(out + used, outlen - used, "/%s", rest);
    } else {
      return slash ? FR_NO_PATH : FR_NO_FILE;
    }
    rest = slash ? slash + 1 : NULL;
  }
  return FR_OK;
}

static FRESULT errno_result(void) {
  switch (errno) {
    case ENOENT:
      return FR_NO_FILE;
    case ENOTDIR:
      return FR_NO_PATH;
    case EEXIST:
    case ENOTEMPTY:
      return errno == EEXIST ? FR_EXIST : FR_DENIED;
    case EACCES:
    case EPERM:
    case EISDIR:
      return FR_DENIED;
    case EROFS:
      return FR_WRITE_PROTECTED;
    default:
      return FR_DISK_ERR;
  }
}

/* ------------------------------------------------------------------ */
/* Files                                                                */
/* ------------------------------------------------------------------ */

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode) {
  char host[2048];
  struct stat st;
  memset(fp, 0, sizeof(*fp));
  const bool create = mode & (FA_CREATE_NEW | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS);
  FRESULT fr = host_path(path, host, sizeof(host), create);
  if (fr != FR_OK) return fr;
  const bool exists = stat(host, &st) == 0;
  if (exists && S_ISDIR(st.st_mode)) return FR_NO_FILE;
  if ((mode & FA_CREATE_NEW) && exists) return FR_EXIST;
  if (!exists && !create) return FR_NO_FILE;

  const char *how;
  if (!(mode & FA_WRITE)) {
    how = "rb";
  } else if ((mode & FA_CREATE_ALWAYS) || !exists) {
    how = "w+b";
  } else {
    how = "r+b";
  }
  FILE *f = fopen(host, how);
  if (!f) return errno_result();
  fseek(f, 0, SEEK_END);
  fp->obj.objsize = (FSIZE_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  fp->host = f;
  fp->flag = mode;
  fp->obj.attr = 0;
  if ((mode & FA_OPEN_APPEND) == FA_OPEN_APPEND) {
    fseek(f, 0, SEEK_END);
    fp->fptr = fp->obj.objsize;
  }
  return FR_OK;
}

FRESULT f_close(FIL *fp) {
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  fclose((FILE *)fp->host);
  fp->host = NULL;
  return FR_OK;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br) {
  if (br) *br = 0;
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  if (!(fp->flag & FA_READ)) return FR_DENIED;
  fseek((FILE *)fp->host, (long)fp->fptr, SEEK_SET);
  const size_t n = fread(buff, 1, btr, (FILE *)fp->host);
  fp->fptr += n;
  if (br) *br = (UINT)n;
  return FR_OK;
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw) {
  if (bw) *bw = 0;
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  if (!(fp->flag & FA_WRITE)) return FR_DENIED;
  fseek((FILE *)fp->host, (long)fp->fptr, SEEK_SET);
  const size_t n = fwrite(buff, 1, btw, (FILE *)fp->host);
  fp->fptr += n;
  if (fp->fptr > fp->obj.objsize) fp->obj.objsize = fp->fptr;
  if (bw) *bw = (UINT)n;
  return n == btw ? FR_OK : FR_DISK_ERR;
}

FRESULT f_lseek(FIL *fp, FSIZE_t ofs) {
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  /* Read-only files stop at the end; writable ones may grow, as in FatFs. */
  if (ofs > fp->obj.objsize) {
    if (!(fp->flag & FA_WRITE)) {
      ofs = fp->obj.objsize;
    } else {
      fseek((FILE *)fp->host, (long)ofs - 1, SEEK_SET);
      fputc(0, (FILE *)fp->host);
      fp->obj.objsize = ofs;
    }
  }
  fp->fptr = ofs;
  return fseek((FILE *)fp->host, (long)ofs, SEEK_SET) ? FR_DISK_ERR : FR_OK;
}

FRESULT f_truncate(FIL *fp) {
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  fflush((FILE *)fp->host);
  if (ftruncate(fileno((FILE *)fp->host), (off_t)fp->fptr) != 0) return FR_DISK_ERR;
  fp->obj.objsize = fp->fptr;
  return FR_OK;
}

FRESULT f_sync(FIL *fp) {
  if (!fp || !fp->host) return FR_INVALID_OBJECT;
  return fflush((FILE *)fp->host) ? FR_DISK_ERR : FR_OK;
}

int f_putc(TCHAR c, FIL *fp) {
  UINT bw;
  return (f_write(fp, &c, 1, &bw) == FR_OK && bw == 1) ? 1 : -1;
}

int f_puts(const TCHAR *str, FIL *fp) {
  UINT bw;
  const UINT n = (UINT)strlen(str);
  return (f_write(fp, str, n, &bw) == FR_OK && bw == n) ? (int)n : -1;
}

int f_printf(FIL *fp, const TCHAR *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return f_puts(buf, fp);
}

TCHAR *f_gets(TCHAR *buff, int len, FIL *fp) {
  int i = 0;
  while (i < len - 1) {
    char c;
    UINT br;
    if (f_read(fp, &c, 1, &br) != FR_OK || br == 0) break;
    buff[i++] = c;
    if (c == '\n') break;
  }
  buff[i] = 0;
  return i ? buff : NULL;
}

/* ------------------------------------------------------------------ */
/* Folders and names                                                    */
/* ------------------------------------------------------------------ */

static void fill_info(const char *hostpath, const char *name, FILINFO *fno) {
  struct stat st;
  memset(fno, 0, sizeof(*fno));
  snprintf(fno->fname, sizeof(fno->fname), "%s", name);
  if (stat(hostpath, &st) == 0) {
    fno->fsize = S_ISDIR(st.st_mode) ? 0 : (FSIZE_t)st.st_size;
    fno->fattrib = S_ISDIR(st.st_mode) ? AM_DIR : AM_ARC;
    if (access(hostpath, W_OK) != 0) fno->fattrib |= AM_RDO;
    struct tm tm;
    localtime_r(&st.st_mtime, &tm);
    fno->fdate = (WORD)(((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
    fno->ftime = (WORD)((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
  }
  if (name[0] == '.') fno->fattrib |= AM_HID;
  /* An 8.3 alias when the name fits one. */
  const char *dot = strrchr(name, '.');
  const size_t base = dot ? (size_t)(dot - name) : strlen(name);
  const size_t ext = dot ? strlen(dot + 1) : 0;
  if (base >= 1 && base <= 8 && ext <= 3) {
    size_t j = 0;
    for (size_t i = 0; name[i] && j < 12; i++) {
      fno->altname[j++] = (TCHAR)((name[i] >= 'a' && name[i] <= 'z') ? name[i] - 32 : name[i]);
    }
    fno->altname[j] = 0;
  }
}

FRESULT f_opendir(DIR *dp, const TCHAR *path) {
  char host[2048];
  memset(dp, 0, sizeof(*dp));
  FRESULT fr = host_path(path, host, sizeof(host), false);
  if (fr != FR_OK) return fr == FR_NO_FILE ? FR_NO_PATH : fr;
  void *d = mdfw_hostdir_open(host);
  if (!d) return FR_NO_PATH;
  dp->host = d;
  snprintf(dp->path, sizeof(dp->path), "%s", host);
  return FR_OK;
}

FRESULT f_closedir(DIR *dp) {
  if (!dp || !dp->host) return FR_INVALID_OBJECT;
  mdfw_hostdir_close(dp->host);
  dp->host = NULL;
  return FR_OK;
}

FRESULT f_readdir(DIR *dp, FILINFO *fno) {
  if (!dp || !dp->host) return FR_INVALID_OBJECT;
  if (!fno) {
    mdfw_hostdir_rewind(dp->host);
    return FR_OK;
  }
  for (;;) {
    const char *name = mdfw_hostdir_next(dp->host);
    if (!name) {
      fno->fname[0] = 0;
      return FR_OK;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
    if (dp->pat && fnmatch(dp->pat, name, FNM_CASEFOLD) != 0) continue;
    char full[2048];
    snprintf(full, sizeof(full), "%s/%s", dp->path, name);
    fill_info(full, name, fno);
    return FR_OK;
  }
}

FRESULT f_findfirst(DIR *dp, FILINFO *fno, const TCHAR *path,
                    const TCHAR *pattern) {
  FRESULT fr = f_opendir(dp, path);
  if (fr != FR_OK) return fr;
  dp->pat = pattern;
  return f_readdir(dp, fno);
}

FRESULT f_findnext(DIR *dp, FILINFO *fno) { return f_readdir(dp, fno); }

FRESULT f_stat(const TCHAR *path, FILINFO *fno) {
  char host[2048];
  FRESULT fr = host_path(path, host, sizeof(host), false);
  if (fr != FR_OK) return fr;
  struct stat st;
  if (stat(host, &st) != 0) return FR_NO_FILE;
  const char *name = strrchr(host, '/');
  if (fno) fill_info(host, name ? name + 1 : host, fno);
  return FR_OK;
}

FRESULT f_chmod(const TCHAR *path, BYTE attr, BYTE mask) {
  char host[2048];
  struct stat st;
  FRESULT fr = host_path(path, host, sizeof(host), false);
  if (fr != FR_OK) return fr;
  if (stat(host, &st) != 0) return FR_NO_FILE;
  if (!(mask & AM_RDO)) return FR_OK;
  const mode_t mode = (attr & AM_RDO) ? (st.st_mode & ~(mode_t)0222) : (st.st_mode | S_IWUSR);
  return chmod(host, mode & 07777) == 0 ? FR_OK : errno_result();
}

FRESULT f_mkdir(const TCHAR *path) {
  char host[2048];
  FRESULT fr = host_path(path, host, sizeof(host), true);
  if (fr != FR_OK) return fr;
  return mkdir(host, 0777) == 0 ? FR_OK : errno_result();
}

FRESULT f_unlink(const TCHAR *path) {
  char host[2048];
  struct stat st;
  FRESULT fr = host_path(path, host, sizeof(host), false);
  if (fr != FR_OK) return fr;
  if (stat(host, &st) != 0) return FR_NO_FILE;
  const int rc = S_ISDIR(st.st_mode) ? rmdir(host) : unlink(host);
  return rc == 0 ? FR_OK : errno_result();
}

FRESULT f_rename(const TCHAR *path_old, const TCHAR *path_new) {
  char from[2048], to[2048];
  FRESULT fr = host_path(path_old, from, sizeof(from), false);
  if (fr != FR_OK) return fr;
  fr = host_path(path_new, to, sizeof(to), true);
  if (fr != FR_OK) return fr;
  struct stat st;
  if (stat(to, &st) == 0) return FR_EXIST;
  return rename(from, to) == 0 ? FR_OK : errno_result();
}

FRESULT f_chdir(const TCHAR *path) {
  char card[1024], host[2048];
  FRESULT fr = card_path(path, card, sizeof(card));
  if (fr != FR_OK) return fr;
  fr = host_path(card, host, sizeof(host), false);
  if (fr != FR_OK) return FR_NO_PATH;
  struct stat st;
  if (stat(host, &st) != 0 || !S_ISDIR(st.st_mode)) return FR_NO_PATH;
  snprintf(s_cwd, sizeof(s_cwd), "%s", card);
  return FR_OK;
}

FRESULT f_chdrive(const TCHAR *path) {
  (void)path;
  return FR_OK;
}

FRESULT f_getcwd(TCHAR *buff, UINT len) {
  snprintf(buff, len, "0:%s", s_cwd);
  return FR_OK;
}

/* A pretend 32 GB card, half free. */
FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt) {
  (void)path;
  (void)opt;
  if (fs) {
    memset(fs, 0, sizeof(*fs));
    fs->fs_type = FS_FAT32;
    fs->csize = 64;
    fs->n_fatent = 1u << 20;
    fs->free_clst = 1u << 19;
  }
  return FR_OK;
}

FRESULT f_getfree(const TCHAR *path, DWORD *nclst, FATFS **fatfs) {
  static FATFS fs;
  f_mount(&fs, path, 0);
  if (nclst) *nclst = fs.free_clst;
  if (fatfs) *fatfs = &fs;
  return FR_OK;
}

FRESULT f_getlabel(const TCHAR *path, TCHAR *label, DWORD *vsn) {
  (void)path;
  if (label) strcpy(label, "EMUMD");
  if (vsn) *vsn = 0x4d44454du;
  return FR_OK;
}

const char *FRESULT_str(FRESULT i) {
  static const char *const names[] = {
      "Succeeded", "A hard error occurred in the low level disk I/O layer",
      "Assertion failed", "The physical drive cannot work", "Could not find the file",
      "Could not find the path", "The path name format is invalid",
      "Access denied due to prohibited access or directory full",
      "Access denied due to prohibited access", "The file/directory object is invalid",
      "The physical drive is write protected", "The logical drive number is invalid",
      "The volume has no work area", "There is no valid FAT volume",
      "The f_mkfs() aborted due to any problem",
      "Could not get a grant to access the volume within defined period",
      "The operation is rejected according to the file sharing policy",
      "LFN working buffer could not be allocated", "Too many open files",
      "Given parameter is invalid"};
  return (unsigned)i < count_of(names) ? names[i] : "Unknown";
}
