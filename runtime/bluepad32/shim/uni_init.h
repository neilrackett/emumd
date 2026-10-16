/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's uni_init.h. */
#ifndef MDFW_SHIM_UNI_INIT_H
#define MDFW_SHIM_UNI_INIT_H
/* Starts Bluepad32 on the platform given to uni_platform_set_custom():
 * its init runs now, on_init_complete at the next cyw43_arch_poll(), and
 * the pads script (--md-option pads=FILE) from then on. */
int uni_init(int argc, const char **argv);
#endif
