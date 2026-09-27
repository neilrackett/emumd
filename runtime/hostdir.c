/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: hostdir.c
 * Description: Host folder listing for fatfs.c, kept apart because FatFs
 *              and POSIX both define a type called DIR.
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "runtime.h"

void *mdfw_hostdir_open(const char *path) { return opendir(path); }
void mdfw_hostdir_close(void *d) { closedir((DIR *)d); }
void mdfw_hostdir_rewind(void *d) { rewinddir((DIR *)d); }

const char *mdfw_hostdir_next(void *d) {
  struct dirent *e = readdir((DIR *)d);
  return e ? e->d_name : NULL;
}

bool mdfw_hostdir_find(const char *dir, const char *name, char *found,
                       size_t foundlen) {
  char probe[2048];
  struct stat st;
  snprintf(probe, sizeof(probe), "%s/%s", dir, name);
  if (stat(probe, &st) == 0) {
    snprintf(found, foundlen, "%s", name);
    return true;
  }
  DIR *d = opendir(dir);
  if (!d) return false;
  bool ok = false;
  for (struct dirent *e; (e = readdir(d));) {
    if (strcasecmp(e->d_name, name) == 0) {
      snprintf(found, foundlen, "%s", e->d_name);
      ok = true;
      break;
    }
  }
  closedir(d);
  return ok;
}
