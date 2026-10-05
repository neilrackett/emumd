/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: pico/rand.h
 * Description: Host stand-in for the Pico SDK's random numbers: the host's
 *              own (getentropy).
 */
#ifndef MDFW_SHIM_PICO_RAND_H
#define MDFW_SHIM_PICO_RAND_H

#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rng_128 {
  uint64_t r[2];
} rng_128_t;

uint32_t get_rand_32(void);
uint64_t get_rand_64(void);
void get_rand_128(rng_128_t *rand128);

#ifdef __cplusplus
}
#endif

#endif
