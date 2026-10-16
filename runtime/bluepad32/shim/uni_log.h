/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's uni_log.h: errors go to the log, the
 * rest with --md-verbose on, as DPRINTF does. */
#ifndef MDFW_SHIM_UNI_LOG_H
#define MDFW_SHIM_UNI_LOG_H
#include "mdfw.h"
#define loge(...) mdfw_log(__VA_ARGS__)
#define logi(...) mdfw_debug(__VA_ARGS__)
#define logd(...) mdfw_debug(__VA_ARGS__)
#endif
