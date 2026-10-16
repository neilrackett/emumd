/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: uni.h
 * Description: Host stand-in for Bluepad32's one-stop header, for a
 *              firmware built with [bluetooth] in mdfw.ini: the same
 *              names, types and layouts as Bluepad32 4.2's include/, so
 *              a custom platform (platform/uni_platform.h) builds
 *              unchanged, over runtime/bluepad32/bluepad32.c, which plays
 *              scripted controllers in place of the radio. Written for
 *              EmuMD; none of Bluepad32 or BTstack is used. What is here
 *              is what a platform uses; the rest of Bluepad32's API (its
 *              console, properties, the parsers) is not provided, so a
 *              firmware calling it will not build, rather than not link.
 */
#ifndef MDFW_SHIM_UNI_H
#define MDFW_SHIM_UNI_H

#ifdef __cplusplus
extern "C" {
#endif

/* The firmware's Bluepad32 configuration (CONFIG_BLUEPAD32_MAX_DEVICES,
 * CONFIG_BLUEPAD32_PLATFORM_CUSTOM), from an include folder, as Bluepad32
 * reads it; without one, four devices and a custom platform. */
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#ifndef CONFIG_BLUEPAD32_MAX_DEVICES
#define CONFIG_BLUEPAD32_MAX_DEVICES 4
#endif
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#define CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#endif

#include "bt/uni_bt.h"
#include "bt/uni_bt_conn.h"
#include "bt/uni_bt_defines.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "controller/uni_mouse.h"
#include "parser/uni_hid_parser.h"
#include "platform/uni_platform.h"
#include "uni_common.h"
#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_init.h"
#include "uni_log.h"
#include "uni_property.h"

#ifdef __cplusplus
}
#endif

#endif
