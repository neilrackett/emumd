/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico.h
 * Description: Host stand-in for the Pico SDK's base header, so firmware
 *              code builds unchanged for md-emulator. Only what firmware
 *              logic commonly uses; hardware set-up belongs outside the
 *              .mdfw build (see docs/GUIDE.md). Add headers of your own in
 *              your project's shims folder for anything missing.
 */
#ifndef MDFW_SHIM_PICO_H
#define MDFW_SHIM_PICO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdfw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int uint;

/* Placement attributes: everything is "in RAM" on the host. */
#define __not_in_flash(group)
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#define __time_critical_func(f) f
#define __in_flash(group)
#define __scratch_x(group)
#define __scratch_y(group)
#define __uninitialized_ram(v) v
#define __attribute_used__ __attribute__((used))
#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __aligned
#define __aligned(n) __attribute__((aligned(n)))
#endif
#ifndef count_of
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Lets core 1's thread be stopped, and the other thread run. */
void mdfw_tight_loop(void);
#define tight_loop_contents() mdfw_tight_loop()
static inline void __dmb(void) { __sync_synchronize(); }
static inline void __dsb(void) { __sync_synchronize(); }
static inline void __isb(void) { __sync_synchronize(); }
static inline void __compiler_memory_barrier(void) {
  __asm__ volatile("" ::: "memory");
}
/* SEV/WFE/WFI as calls: some host compilers treat the names as ARM
 * builtins. */
void mdfw_sev(void);
void mdfw_wfe(void);
void mdfw_wfi(void);
#define __sev() mdfw_sev()
#define __wfe() mdfw_wfe()
#define __wfi() mdfw_wfi()
static inline void __breakpoint(void) { abort(); }

/* Interrupts: ROM3 "interrupts" run synchronously on the emulator's thread,
 * so masking them has nothing to do. */
static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t status) { (void)status; }
typedef void (*irq_handler_t)(void);

/* Which "core" is running: 0 on the emulator's thread, 1 on core 1's. */
uint get_core_num(void);

void panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
#define hard_assert(c) \
  do {                 \
    if (!(c)) panic("assertion failed: %s", #c); \
  } while (0)

/* Time (pico/time.h has the rest). */
typedef uint64_t absolute_time_t;
uint32_t time_us_32(void);
uint64_t time_us_64(void);

/* timer_hw->timerawl / timerawh read the emulated microsecond counter. */
typedef struct {
  volatile uint32_t timerawl;
  volatile uint32_t timerawh;
} mdfw_timer_hw_t;
extern mdfw_timer_hw_t *timer_hw;

#ifdef __cplusplus
}
#endif

#endif /* MDFW_SHIM_PICO_H */
