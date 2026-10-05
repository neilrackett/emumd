/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: net.c
 * Description: The network on the far side of the Wi-Fi chip's radio:
 *              this computer's own, through libslirp, which is a router
 *              with DHCP and DNS that makes the device's connections from
 *              this computer, so nothing needs setting up or permission.
 *              The device gets 10.0.2.15; 10.0.2.2 is this computer
 *              (its localhost) and 10.0.2.3 the DNS server.
 *
 *              --md-option wifi_forward=tcp:8080:80,udp:6969:69 lets this
 *              computer connect to the device (localhost:8080 reaches its
 *              port 80), and wifi_pcap=FILE saves every frame for
 *              Wireshark.
 */

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <libslirp.h>

#include "mdfw.h"
#include "runtime.h"
#include "wifi.h"

#define FRAME_MAX 1536 /* 1500-byte MTU, plus the Ethernet header */
#define RX_SLOTS 128   /* frames waiting for the firmware to poll */
#define MAX_FDS 256

static Slirp *s_slirp;

static struct {
  uint16_t len;
  uint8_t data[FRAME_MAX];
} s_rx[RX_SLOTS];
static unsigned s_rx_head, s_rx_count;

static struct pollfd s_fds[MAX_FDS];
static int s_nfds;

static FILE *s_pcap;

/* When the last frame went either way, in real time (pacing, below). */
static int64_t s_last_frame_us = INT64_MIN / 2;
/* When libslirp last had a turn, in real time: its timers want one now
 * and then, even when nothing comes in. */
static int64_t s_serviced_us;

static int64_t real_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void frame_moved(void) { __atomic_store_n(&s_last_frame_us, real_us(), __ATOMIC_RELAXED); }

/* ------------------------------------------------------------------ */
/* Saving frames for Wireshark                                          */
/* ------------------------------------------------------------------ */

static void pcap_open(const char *path) {
  s_pcap = fopen(path, "wb");
  if (!s_pcap) {
    mdfw_log("Wi-Fi: cannot write %s", path);
    return;
  }
  /* libpcap's format: version 2.4, Ethernet frames. */
  const uint32_t head[6] = {0xa1b2c3d4u, 2u | (4u << 16), 0, 0, 65535u, 1u};
  fwrite(head, sizeof(head), 1, s_pcap);
  fflush(s_pcap);
}

static void pcap_frame(const void *frame, size_t len) {
  if (!s_pcap) return;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  const uint32_t rec[4] = {(uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec, (uint32_t)len,
                           (uint32_t)len};
  fwrite(rec, sizeof(rec), 1, s_pcap);
  fwrite(frame, 1, len, s_pcap);
  fflush(s_pcap);
}

/* ------------------------------------------------------------------ */
/* libslirp's side                                                      */
/* ------------------------------------------------------------------ */

/* A frame for the device: kept until the firmware polls, or dropped if
 * too many are waiting (TCP sends it again). */
static ssize_t send_packet(const void *buf, size_t len, void *opaque) {
  (void)opaque;
  if (len <= FRAME_MAX && s_rx_count < RX_SLOTS) {
    const unsigned slot = (s_rx_head + s_rx_count++) % RX_SLOTS;
    s_rx[slot].len = (uint16_t)len;
    memcpy(s_rx[slot].data, buf, len);
    pcap_frame(buf, len);
  }
  frame_moved();
  return (ssize_t)len;
}

static void guest_error(const char *msg, void *opaque) {
  (void)opaque;
  mdfw_debug("Wi-Fi: %s\n", msg);
}

static int64_t clock_get_ns(void *opaque) {
  (void)opaque;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/* libslirp's only timer is for IPv6, which is off. */
static int s_no_timer;
static void *timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque) {
  (void)cb;
  (void)cb_opaque;
  (void)opaque;
  return &s_no_timer;
}
static void timer_free(void *timer, void *opaque) {
  (void)timer;
  (void)opaque;
}
static void timer_mod(void *timer, int64_t expire_time, void *opaque) {
  (void)timer;
  (void)expire_time;
  (void)opaque;
}
static void notify(void *opaque) { (void)opaque; }
static void poll_socket(int fd, void *opaque) {
  (void)fd;
  (void)opaque;
}

static int add_poll(int fd, int events, void *opaque) {
  (void)opaque;
  if (s_nfds == MAX_FDS) return -1;
  short e = 0;
  if (events & SLIRP_POLL_IN) e |= POLLIN;
  if (events & SLIRP_POLL_OUT) e |= POLLOUT;
  if (events & SLIRP_POLL_PRI) e |= POLLPRI;
  s_fds[s_nfds].fd = fd;
  s_fds[s_nfds].events = e;
  s_fds[s_nfds].revents = 0;
  return s_nfds++;
}

static int get_revents(int idx, void *opaque) {
  (void)opaque;
  if (idx < 0 || idx >= s_nfds) return 0;
  const short r = s_fds[idx].revents;
  int e = 0;
  if (r & POLLIN) e |= SLIRP_POLL_IN;
  if (r & POLLOUT) e |= SLIRP_POLL_OUT;
  if (r & POLLPRI) e |= SLIRP_POLL_PRI;
  if (r & POLLERR) e |= SLIRP_POLL_ERR;
  if (r & POLLHUP) e |= SLIRP_POLL_HUP;
  return e;
}

/* libslirp 4.9 polls sockets; older ones file descriptors (the same on
 * Unix). */
#if SLIRP_CONFIG_VERSION_MAX >= 6
#define CONFIG_VERSION 6
#define fill_fds(timeout) slirp_pollfds_fill_socket(s_slirp, (timeout), add_poll, NULL)
#else
#define CONFIG_VERSION 4
#define fill_fds(timeout) slirp_pollfds_fill(s_slirp, (timeout), add_poll, NULL)
#endif

static const SlirpCb s_callbacks = {
    .send_packet = send_packet,
    .guest_error = guest_error,
    .clock_get_ns = clock_get_ns,
    .timer_new = timer_new,
    .timer_free = timer_free,
    .timer_mod = timer_mod,
#if CONFIG_VERSION >= 6
    .register_poll_socket = poll_socket,
    .unregister_poll_socket = poll_socket,
#else
    .register_poll_fd = poll_socket,
    .unregister_poll_fd = poll_socket,
#endif
    .notify = notify,
};

/* --md-option wifi_forward=[tcp:|udp:]HOSTPORT:DEVICEPORT,... */
static void add_forwards(const char *list) {
  char spec[256];
  snprintf(spec, sizeof(spec), "%s", list);
  struct in_addr host, device;
  inet_pton(AF_INET, "127.0.0.1", &host);
  inet_pton(AF_INET, "10.0.2.15", &device);
  for (char *save = NULL, *f = strtok_r(spec, ", ", &save); f;
       f = strtok_r(NULL, ", ", &save)) {
    int udp = 0;
    if (!strncmp(f, "udp:", 4)) {
      udp = 1;
      f += 4;
    } else if (!strncmp(f, "tcp:", 4)) {
      f += 4;
    }
    int hport, dport;
    if (sscanf(f, "%d:%d", &hport, &dport) != 2) {
      mdfw_log("Wi-Fi: wifi_forward=%s? (e.g. tcp:8080:80)", f);
    } else if (slirp_add_hostfwd(s_slirp, udp, host, hport, device, dport) != 0) {
      mdfw_log("Wi-Fi: cannot forward port %d (in use?)", hport);
    } else {
      mdfw_log("Wi-Fi: localhost:%d (%s) reaches the device's port %d", hport,
               udp ? "UDP" : "TCP", dport);
    }
  }
}

/* ------------------------------------------------------------------ */
/* The chip's side (lock held)                                          */
/* ------------------------------------------------------------------ */

bool mdfw_net_start(void) {
  if (s_slirp) return true;
  SlirpConfig cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.version = CONFIG_VERSION;
  cfg.in_enabled = true;
  inet_pton(AF_INET, "10.0.2.0", &cfg.vnetwork);
  inet_pton(AF_INET, "255.255.255.0", &cfg.vnetmask);
  inet_pton(AF_INET, "10.0.2.2", &cfg.vhost);
  inet_pton(AF_INET, "10.0.2.15", &cfg.vdhcp_start);
  inet_pton(AF_INET, "10.0.2.3", &cfg.vnameserver);
  cfg.vhostname = "emumd";
  cfg.if_mtu = 1500;
  cfg.if_mru = 1500;
  s_slirp = slirp_new(&cfg, &s_callbacks, NULL);
  if (!s_slirp) {
    mdfw_log("Wi-Fi: cannot reach this computer's network (libslirp)");
    return false;
  }
  s_rx_head = s_rx_count = 0;
  const char *fwd = mdfw_option("wifi_forward");
  if (fwd) add_forwards(fwd);
  const char *pcap = mdfw_option("wifi_pcap");
  if (pcap) pcap_open(pcap);
  return true;
}

void mdfw_net_stop(void) {
  if (s_slirp) slirp_cleanup(s_slirp);
  s_slirp = NULL;
  s_rx_count = 0;
  if (s_pcap) fclose(s_pcap);
  s_pcap = NULL;
}

void mdfw_net_send(const void *frame, size_t len) {
  if (!s_slirp) return;
  frame_moved();
  pcap_frame(frame, len);
  slirp_input(s_slirp, frame, (int)len);
}

bool mdfw_net_service(void (*deliver)(const uint8_t *frame, size_t len)) {
  if (!s_slirp) return false;
  uint32_t timeout = 0;
  s_nfds = 0;
  fill_fds(&timeout);
  const int rc = poll(s_fds, (nfds_t)s_nfds, 0);
  slirp_pollfds_poll(s_slirp, rc < 0, get_revents, NULL);
  s_serviced_us = real_us();
  /* Delivering one can bring more (an ARP reply, say). */
  bool any = false;
  uint8_t frame[FRAME_MAX];
  while (s_rx_count) {
    const unsigned len = s_rx[s_rx_head].len;
    memcpy(frame, s_rx[s_rx_head].data, len);
    s_rx_head = (s_rx_head + 1) % RX_SLOTS;
    s_rx_count--;
    deliver(frame, len);
    any = true;
  }
  return any;
}

/* A socket libslirp is waiting on has something. (Not one it asks nothing
 * of, such as a connection already closed, which says so every time.) */
static bool wanted(const struct pollfd *fds, int n) {
  for (int i = 0; i < n; i++) {
    if (fds[i].events && fds[i].revents) return true;
  }
  return false;
}

bool mdfw_net_ready(void) {
  if (!s_slirp) return false;
  if (s_rx_count) return true;
  uint32_t timeout = UINT32_MAX;
  s_nfds = 0;
  fill_fds(&timeout);
  if (real_us() - s_serviced_us >= (int64_t)timeout * 1000) return true;
  return poll(s_fds, (nfds_t)s_nfds, 0) > 0 && wanted(s_fds, s_nfds);
}

/* Without the lock: other threads carry on meanwhile. */
void mdfw_net_wait(unsigned us) {
  struct pollfd fds[MAX_FDS];
  int n = 0;
  const int64_t until = real_us() + us;
  mdfw_wifi_lock();
  if (s_slirp) {
    uint32_t timeout = UINT32_MAX;
    s_nfds = 0;
    fill_fds(&timeout);
    for (int i = 0; i < s_nfds; i++) {
      if (s_fds[i].events) fds[n++] = s_fds[i];
    }
  }
  mdfw_wifi_unlock();
  if (n && poll(fds, (nfds_t)n, (int)((us + 999) / 1000)) > 0 && wanted(fds, n)) return;
  const int64_t left = until - real_us();
  if (left > 0) {
    const struct timespec ts = {0, (long)left * 1000L};
    nanosleep(&ts, NULL);
  }
}

/* ------------------------------------------------------------------ */
/* Pacing (the emulator's thread, once a frame)                         */
/* ------------------------------------------------------------------ */

/* The network runs in real time and the firmware in emulated time, which
 * a fast-forwarding emulator runs far ahead, so the firmware's timeouts
 * would run out before replies could come. While frames are moving (one
 * in the last second), the emulator waits for real time to catch up;
 * when the network is quiet, it runs as fast as it likes. */
static bool s_pacing;
static uint64_t s_pace_emu_us;
static int64_t s_pace_real_us;

void mdfw_runtime_wifi_pace(uint64_t now_us) {
  const int64_t real = real_us();
  if (real - __atomic_load_n(&s_last_frame_us, __ATOMIC_RELAXED) > 1000000) {
    s_pacing = false;
    return;
  }
  int64_t ahead = (int64_t)(now_us - s_pace_emu_us) - (real - s_pace_real_us);
  if (!s_pacing || ahead < -100000) { /* (re)start from here */
    s_pacing = true;
    s_pace_emu_us = now_us;
    s_pace_real_us = real;
    return;
  }
  /* Two frames ahead is the emulator's own pacing at work: leave it be. */
  if (ahead <= 40000) return;
  if (ahead > 100000) ahead = 100000; /* stay responsive */
  const struct timespec ts = {0, (long)ahead * 1000L};
  nanosleep(&ts, NULL);
}
