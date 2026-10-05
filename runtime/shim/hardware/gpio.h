/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/gpio.h: pins read what mdfw_gpio_inputs
 * says (high, inactive, unless EmuMD says otherwise) and writes go
 * nowhere. The cartridge bus is not pins on the host (mdfw.h). */
#ifndef MDFW_SHIM_HARDWARE_GPIO_H
#define MDFW_SHIM_HARDWARE_GPIO_H
#include "pico.h"
extern uint32_t mdfw_gpio_inputs; /* bit n: what pin n reads (pico.c) */
#define GPIO_IN false
#define GPIO_OUT true
enum gpio_function { GPIO_FUNC_XIP, GPIO_FUNC_SPI, GPIO_FUNC_UART, GPIO_FUNC_I2C,
  GPIO_FUNC_PWM, GPIO_FUNC_SIO, GPIO_FUNC_PIO0, GPIO_FUNC_PIO1, GPIO_FUNC_GPCK,
  GPIO_FUNC_USB, GPIO_FUNC_NULL = 0x1f };
enum gpio_drive_strength { GPIO_DRIVE_STRENGTH_2MA, GPIO_DRIVE_STRENGTH_4MA,
  GPIO_DRIVE_STRENGTH_8MA, GPIO_DRIVE_STRENGTH_12MA };
enum gpio_slew_rate { GPIO_SLEW_RATE_SLOW, GPIO_SLEW_RATE_FAST };
enum gpio_irq_level { GPIO_IRQ_LEVEL_LOW = 1, GPIO_IRQ_LEVEL_HIGH = 2,
  GPIO_IRQ_EDGE_FALL = 4, GPIO_IRQ_EDGE_RISE = 8 };
static inline void gpio_init(uint g) { (void)g; }
static inline void gpio_init_mask(uint32_t m) { (void)m; }
static inline void gpio_deinit(uint g) { (void)g; }
static inline void gpio_set_dir(uint g, bool out) { (void)g; (void)out; }
static inline void gpio_set_function(uint g, enum gpio_function f) { (void)g; (void)f; }
static inline void gpio_pull_up(uint g) { (void)g; }
static inline void gpio_pull_down(uint g) { (void)g; }
static inline void gpio_disable_pulls(uint g) { (void)g; }
static inline void gpio_set_pulls(uint g, bool up, bool down) { (void)g; (void)up; (void)down; }
static inline void gpio_set_drive_strength(uint g, enum gpio_drive_strength d) { (void)g; (void)d; }
static inline void gpio_set_slew_rate(uint g, enum gpio_slew_rate s) { (void)g; (void)s; }
static inline void gpio_set_input_enabled(uint g, bool e) { (void)g; (void)e; }
static inline void gpio_put(uint g, bool v) { (void)g; (void)v; }
static inline void gpio_put_masked(uint32_t m, uint32_t v) { (void)m; (void)v; }
static inline void gpio_set_mask(uint32_t m) { (void)m; }
static inline void gpio_clr_mask(uint32_t m) { (void)m; }
static inline bool gpio_get(uint g) { return (mdfw_gpio_inputs >> (g & 31u)) & 1u; }
static inline uint32_t gpio_get_all(void) { return mdfw_gpio_inputs; }
static inline void gpio_set_irq_enabled(uint g, uint32_t e, bool on) { (void)g; (void)e; (void)on; }
/* The SDK's debug pins, which a firmware may leave in: nothing to toggle. */
#define CU_REGISTER_DEBUG_PINS(...)
#define CU_SELECT_DEBUG_PINS(x)
#define DEBUG_PINS_ENABLED(p) false
#define DEBUG_PINS_SET(p, v) ((void)0)
#define DEBUG_PINS_CLR(p, v) ((void)0)
#define DEBUG_PINS_XOR(p, v) ((void)0)
#endif
