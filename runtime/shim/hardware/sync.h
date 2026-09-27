/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/sync.h: barriers and interrupt masking are in
 * pico.h; the 32 hardware spin locks are host mutexes. */
#ifndef MDFW_SHIM_HARDWARE_SYNC_H
#define MDFW_SHIM_HARDWARE_SYNC_H
#include "pico.h"
typedef volatile uint32_t spin_lock_t;
spin_lock_t *spin_lock_instance(uint lock_num);
uint spin_lock_get_num(spin_lock_t *lock);
spin_lock_t *spin_lock_init(uint lock_num);
int spin_lock_claim_unused(bool required);
void spin_lock_claim(uint lock_num);
void spin_lock_unclaim(uint lock_num);
uint32_t spin_lock_blocking(spin_lock_t *lock);
void spin_unlock(spin_lock_t *lock, uint32_t saved_irq);
void spin_lock_unsafe_blocking(spin_lock_t *lock);
void spin_unlock_unsafe(spin_lock_t *lock);
static inline uint next_striped_spin_lock_num(void) { return 16; }
#endif
