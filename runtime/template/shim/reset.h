/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: reset.h
 * Description: The SidecarTridge template's reset.h for EmuMD (the same
 *              include guard), without the ARM code that jumps to the
 *              Booster: runtime/template/template.c has a stand-in. The
 *              template's own reset.c (reset_device and friends) builds
 *              as it is.
 */
#ifndef RESET_H
#define RESET_H

#include "constants.h"
#include "debug.h"
#include "gconfig.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "settings.h"

#define RESET_WATCHDOG_TIMEOUT 20 /* ms */

/* EmuMD does not run the Booster: the firmware stops here, and says so. */
void reset_jump_to_booster(void);
void reset_device();
void reset_deviceAndEraseFlash();

#endif
