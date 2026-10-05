/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for hardware/dma.h (runtime/dma.c): unpaced transfers
 * (memory to memory, DREQ_FORCE) happen at once, byte swapping and all;
 * transfers paced by the PIO or another peripheral do not happen, as the
 * hardware they would serve is not part of a .mdfw build. A caller that
 * can do without a channel (dma_claim_unused_channel(false)) is told none
 * is free, so it takes its CPU path. dma_hw's registers are there to be
 * read and written. */
#ifndef MDFW_SHIM_HARDWARE_DMA_H
#define MDFW_SHIM_HARDWARE_DMA_H
#include "pico.h"

#define NUM_DMA_CHANNELS 12u
#define DREQ_FORCE 0x3f
#define DREQ_XIP_STREAM 37

enum dma_channel_transfer_size { DMA_SIZE_8 = 0, DMA_SIZE_16 = 1, DMA_SIZE_32 = 2 };

typedef struct {
  uint8_t size; /* enum dma_channel_transfer_size */
  bool read_increment, write_increment, bswap, enable, irq_quiet;
  uint8_t dreq, chain_to;
} dma_channel_config;

typedef struct {
  volatile uint32_t read_addr, write_addr, transfer_count, ctrl_trig;
  volatile uint32_t al1_ctrl, al1_read_addr, al1_write_addr, al1_transfer_count_trig;
  volatile uint32_t al2_ctrl, al2_transfer_count, al2_read_addr, al2_write_addr_trig;
  volatile uint32_t al3_ctrl, al3_write_addr, al3_transfer_count, al3_read_addr_trig;
} dma_channel_hw_t;

typedef struct {
  dma_channel_hw_t ch[NUM_DMA_CHANNELS];
  volatile uint32_t intr, inte0, intf0, ints0, inte1, intf1, ints1;
  volatile uint32_t multi_channel_trigger, abort, n_channels;
} dma_hw_t;
extern dma_hw_t *dma_hw;

static inline dma_channel_config dma_channel_get_default_config(uint channel) {
  dma_channel_config c = {DMA_SIZE_32, true, false, false, true, false, DREQ_FORCE,
                          (uint8_t)channel};
  return c;
}
static inline void channel_config_set_transfer_data_size(dma_channel_config *c,
                                                         enum dma_channel_transfer_size s) {
  c->size = (uint8_t)s;
}
static inline void channel_config_set_read_increment(dma_channel_config *c, bool on) {
  c->read_increment = on;
}
static inline void channel_config_set_write_increment(dma_channel_config *c, bool on) {
  c->write_increment = on;
}
static inline void channel_config_set_dreq(dma_channel_config *c, uint dreq) {
  c->dreq = (uint8_t)dreq;
}
static inline void channel_config_set_chain_to(dma_channel_config *c, uint channel) {
  c->chain_to = (uint8_t)channel;
}
static inline void channel_config_set_bswap(dma_channel_config *c, bool on) { c->bswap = on; }
static inline void channel_config_set_enable(dma_channel_config *c, bool on) { c->enable = on; }
static inline void channel_config_set_irq_quiet(dma_channel_config *c, bool on) {
  c->irq_quiet = on;
}
static inline void channel_config_set_ring(dma_channel_config *c, bool write, uint bits) {
  (void)c;
  (void)write;
  (void)bits;
}
static inline void channel_config_set_sniff_enable(dma_channel_config *c, bool on) {
  (void)c;
  (void)on;
}
static inline void channel_config_set_high_priority(dma_channel_config *c, bool on) {
  (void)c;
  (void)on;
}

int dma_claim_unused_channel(bool required);
void dma_channel_claim(uint channel);
void dma_channel_unclaim(uint channel);
bool dma_channel_is_claimed(uint channel);
void dma_channel_set_config(uint channel, const dma_channel_config *config, bool trigger);
void dma_channel_set_read_addr(uint channel, const volatile void *read_addr, bool trigger);
void dma_channel_set_write_addr(uint channel, volatile void *write_addr, bool trigger);
void dma_channel_set_trans_count(uint channel, uint32_t count, bool trigger);
void dma_channel_configure(uint channel, const dma_channel_config *config,
                           volatile void *write_addr, const volatile void *read_addr,
                           uint transfer_count, bool trigger);
void dma_channel_start(uint channel);
void dma_start_channel_mask(uint32_t mask);
static inline void dma_channel_abort(uint channel) { (void)channel; }
static inline bool dma_channel_is_busy(uint channel) {
  (void)channel;
  return false;
}
static inline void dma_channel_wait_for_finish_blocking(uint channel) { (void)channel; }
static inline void dma_channel_set_irq0_enabled(uint channel, bool on) {
  (void)channel;
  (void)on;
}
static inline void dma_channel_set_irq1_enabled(uint channel, bool on) {
  (void)channel;
  (void)on;
}
static inline void dma_channel_acknowledge_irq0(uint channel) {
  dma_hw->ints0 = 1u << channel;
}
static inline void dma_channel_acknowledge_irq1(uint channel) {
  dma_hw->ints1 = 1u << channel;
}

#endif
