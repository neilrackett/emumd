/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: emumd_template.h
 * Description: Included ahead of every source of a firmware built on the
 *              SidecarTridge microfirmware template (template =
 *              sidecartridge in mdfw.ini). The template's debug.h, which
 *              its headers include from their own folder, is skipped (the
 *              same include guard) in favour of EmuMD's, whose DPRINTF
 *              goes to the log, and what it brought in comes from here.
 */
#ifndef EMUMD_TEMPLATE_H
#define EMUMD_TEMPLATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* uint & co, as newlib's headers bring */

#include "debug.h" /* EmuMD's */
#include "pico/stdlib.h"
#define DEBUG_H

#ifndef __cplusplus
#if __has_include("constants.h")
#include "constants.h"
#endif
#endif

/* The older template's debug.h has this, for romemul.h. */
typedef void (*IRQInterceptionCallback)();

#endif
