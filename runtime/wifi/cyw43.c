/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: cyw43.c
 * Description: The Pico W's Wi-Fi chip and the Pico SDK's cyw43_arch, for
 *              a firmware built with [wifi] in mdfw.ini. Joining works
 *              whatever the network's name and password (they are not
 *              checked), and the chip's Ethernet frames go to this
 *              computer's network (net.c). The firmware's own lwIP runs
 *              on top, as on the Pico W, under cyw43_arch's async_context.
 *
 *              --md-option wifi=badauth, nonet or fail makes joining fail
 *              that way (wifi=off: no networks at all), wifi_join_ms= is
 *              how long joining takes (250), wifi_rssi= the signal (-45).
 */

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif
#if LWIP_TCP
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"
#endif
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "runtime.h"
#include "wifi.h"

#ifndef CYW43_WL_GPIO_LED_PIN
#define CYW43_WL_GPIO_LED_PIN 0
#endif

#define SCAN_MS 500

cyw43_t cyw43_state;

static async_context_t *s_context; /* cyw43_arch's */
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
static async_context_threadsafe_background_t s_default_context;
#else
static async_context_poll_t s_default_context;
#endif
static bool s_default_in_use;

static bool s_powered;
static bool s_lwip_ready;
static uint32_t s_country = PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE;
static bool s_itf_up[2];
static uint32_t s_pm;
static bool s_led;

/* Joining, on the STA interface: s_join is what cyw43_wifi_link_status
 * says (CYW43_LINK_JOIN once joined). */
static int s_join = CYW43_LINK_DOWN;
static int s_join_logged = CYW43_LINK_DOWN;
static bool s_join_pending;
static absolute_time_t s_join_at;
static char s_ssid[33];

static bool s_scan_active;
static absolute_time_t s_scan_at;
static char s_scan_ssid[33];
static int (*s_scan_cb)(void *, const cyw43_ev_scan_result_t *);
static void *s_scan_env;

static void chip_work(async_context_t *c, async_when_pending_worker_t *w);
static async_when_pending_worker_t s_chip = {.do_work = chip_work};

/* The access point's MAC: libslirp's router, made from 10.0.2.2. */
static const uint8_t ROUTER_MAC[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

/* ------------------------------------------------------------------ */
/* lwIP's port                                                          */
/* ------------------------------------------------------------------ */

u32_t sys_now(void) { return (u32_t)(mdfw_time_us() / 1000u); }
u32_t sys_jiffies(void) { return time_us_32(); }
/* Without an OS, the async_context's lock protects lwIP. */
sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t pval) { (void)pval; }

/* ------------------------------------------------------------------ */
/* The network interfaces                                               */
/* ------------------------------------------------------------------ */

/* --md-option wifi= */
static int join_outcome(void) {
  const char *how = mdfw_option("wifi");
  if (!how || !strcmp(how, "on")) return CYW43_LINK_JOIN;
  if (!strcmp(how, "badauth")) return CYW43_LINK_BADAUTH;
  if (!strcmp(how, "nonet") || !strcmp(how, "off")) return CYW43_LINK_NONET;
  if (!strcmp(how, "fail")) return CYW43_LINK_FAIL;
  return CYW43_LINK_JOIN;
}

static err_t link_output(struct netif *n, struct pbuf *p) {
  return cyw43_send_ethernet(&cyw43_state, n->name[1] - '0', p->tot_len, p, true) == 0
             ? ERR_OK
             : ERR_IF;
}

static err_t netif_setup(struct netif *n) {
  n->linkoutput = link_output;
  n->output = etharp_output;
#if LWIP_IPV6
  n->output_ip6 = ethip6_output;
#endif
  n->mtu = 1500;
  n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET |
             NETIF_FLAG_IGMP;
#if LWIP_IPV6
  n->flags |= NETIF_FLAG_MLD6;
#endif
  n->hwaddr_len = 6;
  cyw43_wifi_get_mac(&cyw43_state, n->name[1] - '0', n->hwaddr);
  return ERR_OK;
}

static void itf_down(int itf) {
  if (!s_itf_up[itf]) return;
  struct netif *n = &cyw43_state.netif[itf];
#if LWIP_DHCP
  if (itf == CYW43_ITF_STA) {
    dhcp_stop(n);
    dhcp_cleanup(n);
  }
#endif
  netif_remove(n);
  s_itf_up[itf] = false;
}

/* As the Pico SDK sets an interface up: DHCP for the client (once it has
 * joined), a fixed address for an access point. */
static void itf_up(int itf) {
  struct netif *n = &cyw43_state.netif[itf];
  itf_down(itf);
  ip4_addr_t ip, mask, gw;
  IP4_ADDR(&mask, 255, 255, 255, 0);
  if (itf == CYW43_ITF_STA) {
    IP4_ADDR(&ip, 0, 0, 0, 0);
    IP4_ADDR(&gw, 192, 168, 0, 1);
  } else {
    IP4_ADDR(&ip, 192, 168, 4, 1);
    gw = ip;
  }
  n->name[0] = 'w';
  n->name[1] = (char)('0' + itf);
  netif_add(n, &ip, &mask, &gw, &cyw43_state, netif_setup, ethernet_input);
#if LWIP_NETIF_HOSTNAME
  netif_set_hostname(n, CYW43_HOST_NAME);
#endif
  netif_set_default(n);
  netif_set_up(n);
  if (itf == CYW43_ITF_STA) {
#if LWIP_DNS
    ip_addr_t dns;
    IP_ADDR4(&dns, 8, 8, 8, 8);
    dns_setserver(0, &dns);
#endif
#if LWIP_DHCP
    dhcp_start(n);
#endif
  }
  s_itf_up[itf] = true;
}

static void set_link(bool up) {
  if (!s_itf_up[CYW43_ITF_STA]) return;
  struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
  if (up) {
    netif_set_link_up(n);
  } else {
    netif_set_link_down(n);
  }
}

/* A power cut: every TCP connection goes, without a word to the firmware
 * (whose callbacks would find it powered off). */
static void forget_connections(void) {
#if LWIP_TCP
  struct tcp_pcb *pcb;
  while ((pcb = tcp_active_pcbs) != NULL || (pcb = tcp_bound_pcbs) != NULL) {
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_err(pcb, NULL);
    tcp_abort(pcb);
  }
  while ((pcb = tcp_tw_pcbs) != NULL) tcp_abort(pcb);
  while (tcp_listen_pcbs.pcbs != NULL) tcp_close(tcp_listen_pcbs.pcbs);
#endif
}

/* ------------------------------------------------------------------ */
/* The chip's work (lock held)                                          */
/* ------------------------------------------------------------------ */

static void deliver(const uint8_t *frame, size_t len) {
  struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
  if (!s_itf_up[CYW43_ITF_STA] || !netif_is_link_up(n)) return;
  struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
  if (!p) return; /* out of buffers: lost, as on the chip */
  pbuf_take(p, frame, (u16_t)len);
  if (n->input(p, n) != ERR_OK) pbuf_free(p);
}

static void finish_join(void) {
  s_join_pending = false;
  s_join = join_outcome();
  if (s_join == CYW43_LINK_JOIN) {
    set_link(true);
    mdfw_log("Wi-Fi: joined \"%s\" (this computer's network)", s_ssid);
  } else if (s_join != s_join_logged) {
    mdfw_log("Wi-Fi: could not join \"%s\" (--md-option wifi=%s)", s_ssid,
             mdfw_option("wifi"));
  }
  s_join_logged = s_join;
}

static void finish_scan(void) {
  s_scan_active = false;
  const char *ssid = s_ssid[0] ? s_ssid : "EmuMD";
  if (!s_scan_cb || join_outcome() == CYW43_LINK_NONET) return;
  if (s_scan_ssid[0] && strcmp(s_scan_ssid, ssid)) return;
  cyw43_ev_scan_result_t r;
  memset(&r, 0, sizeof(r));
  memcpy(r.bssid, ROUTER_MAC, sizeof(r.bssid));
  r.ssid_len = (uint8_t)strlen(ssid);
  memcpy(r.ssid, ssid, r.ssid_len);
  r.channel = 6;
  r.auth_mode = 4; /* WPA2 */
  r.rssi = (int16_t)mdfw_option_int("wifi_rssi", -45);
  s_scan_cb(s_scan_env, &r);
}

static void chip_work(async_context_t *c, async_when_pending_worker_t *w) {
  (void)c;
  (void)w;
  if (s_join_pending && time_reached(s_join_at)) finish_join();
  if (s_scan_active && time_reached(s_scan_at)) finish_scan();
  mdfw_net_service(deliver);
  sys_check_timeouts();
}

void mdfw_wifi_check(async_context_t *c) {
  if (c != s_context || !s_powered) return;
  if ((s_join_pending && time_reached(s_join_at)) ||
      (s_scan_active && time_reached(s_scan_at)) || mdfw_net_ready() ||
      sys_timeouts_sleeptime() == 0) {
    s_chip.work_pending = true;
  }
}

absolute_time_t mdfw_wifi_next_time(async_context_t *c) {
  if (c != s_context || !s_powered) return at_the_end_of_time;
  absolute_time_t next = at_the_end_of_time;
  const u32_t ms = sys_timeouts_sleeptime();
  if (ms != SYS_TIMEOUTS_SLEEPTIME_INFINITE) next = make_timeout_time_ms(ms);
  if (s_join_pending && s_join_at < next) next = s_join_at;
  if (s_scan_active && s_scan_at < next) next = s_scan_at;
  return next;
}

/* ------------------------------------------------------------------ */
/* Power                                                                */
/* ------------------------------------------------------------------ */

async_context_t *cyw43_arch_init_default_async_context(void) {
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
  if (!async_context_threadsafe_background_init_with_defaults(&s_default_context))
    return NULL;
#else
  if (!async_context_poll_init_with_defaults(&s_default_context)) return NULL;
#endif
  return &s_default_context.core;
}

void cyw43_arch_set_async_context(async_context_t *context) { s_context = context; }
async_context_t *cyw43_arch_async_context(void) { return s_context; }

int cyw43_arch_init_with_country(uint32_t country) {
  if (!s_context) {
    s_context = cyw43_arch_init_default_async_context();
    if (!s_context) return PICO_ERROR_GENERIC;
    s_default_in_use = true;
  }
  const char *how = mdfw_option("wifi");
  if (how && strcmp(how, "on") && strcmp(how, "off") && strcmp(how, "nonet") &&
      strcmp(how, "badauth") && strcmp(how, "fail")) {
    mdfw_log("Wi-Fi: --md-option wifi=%s? (on, off, nonet, badauth or fail)", how);
  }
  mdfw_wifi_lock();
  if (!s_lwip_ready) { /* once, as the Pico SDK does */
    lwip_init();
    s_lwip_ready = true;
  }
  s_country = country;
  s_pm = CYW43_DEFAULT_PM;
  mdfw_net_start();
  async_context_add_when_pending_worker(s_context, &s_chip);
  s_powered = true;
  mdfw_wifi_unlock();
  return PICO_OK;
}

int cyw43_arch_init(void) {
  return cyw43_arch_init_with_country(PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE);
}

uint32_t cyw43_arch_get_country_code(void) { return s_country; }

static void power_down(void) {
  if (!s_powered) return;
  itf_down(CYW43_ITF_STA);
  itf_down(CYW43_ITF_AP);
  mdfw_net_stop();
  async_context_remove_when_pending_worker(s_context, &s_chip);
  s_powered = false;
  s_join = s_join_logged = CYW43_LINK_DOWN;
  s_join_pending = s_scan_active = false;
  s_scan_cb = NULL;
  s_ssid[0] = 0;
  s_led = false;
}

void cyw43_arch_deinit(void) {
  if (!s_context) return;
  mdfw_wifi_lock();
  power_down();
  mdfw_wifi_unlock();
  if (s_default_in_use) {
    async_context_deinit(s_context);
    s_context = NULL;
    s_default_in_use = false;
  }
}

/* The Multi-device is switched off (runtime.c, once the firmware's threads
 * have stopped). */
void mdfw_runtime_wifi_power_off(void) {
  mdfw_wifi_lock();
  if (s_powered) forget_connections();
  mdfw_wifi_unlock();
  cyw43_arch_deinit();
  s_context = NULL;
  s_default_in_use = false;
}

bool cyw43_is_initialized(cyw43_t *self) {
  (void)self;
  return s_powered;
}

/* ------------------------------------------------------------------ */
/* Station and access point                                             */
/* ------------------------------------------------------------------ */

int cyw43_wifi_set_up(cyw43_t *self, int itf, bool up, uint32_t country) {
  (void)self;
  if (itf != CYW43_ITF_STA && itf != CYW43_ITF_AP) return -1;
  mdfw_wifi_lock();
  s_country = country;
  if (up) {
    itf_up(itf);
  } else {
    if (itf == CYW43_ITF_STA) {
      s_join = CYW43_LINK_DOWN;
      s_join_pending = false;
    }
    itf_down(itf);
  }
  mdfw_wifi_unlock();
  return 0;
}

void cyw43_arch_enable_sta_mode(void) {
  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, s_country);
}

void cyw43_arch_disable_sta_mode(void) {
  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, false, s_country);
}

void cyw43_arch_enable_ap_mode(const char *ssid, const char *password, uint32_t auth) {
  (void)password;
  (void)auth;
  mdfw_log("Wi-Fi: access point \"%s\" (not emulated: nothing can join it)",
           ssid ? ssid : "");
  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, s_country);
}

void cyw43_arch_disable_ap_mode(void) {
  cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, false, s_country);
}

int cyw43_wifi_join(cyw43_t *self, size_t ssid_len, const uint8_t *ssid, size_t key_len,
                    const uint8_t *key, uint32_t auth_type, const uint8_t *bssid,
                    uint32_t channel) {
  (void)self;
  (void)key_len;
  (void)key;
  (void)auth_type;
  (void)bssid;
  (void)channel;
  int rc = -1;
  mdfw_wifi_lock();
  if (s_powered && s_itf_up[CYW43_ITF_STA]) {
    if (ssid_len > 32) ssid_len = 32;
    memcpy(s_ssid, ssid, ssid_len);
    s_ssid[ssid_len] = 0;
    set_link(false);
    s_join = CYW43_LINK_DOWN; /* until it has joined */
    s_join_pending = true;
    s_join_at = make_timeout_time_ms((uint32_t)mdfw_option_int("wifi_join_ms", 250));
    rc = 0;
  }
  mdfw_wifi_unlock();
  return rc;
}

int cyw43_wifi_leave(cyw43_t *self, int itf) {
  (void)self;
  if (itf != CYW43_ITF_STA) return 0;
  mdfw_wifi_lock();
  set_link(false);
  s_join = CYW43_LINK_DOWN;
  s_join_pending = false;
  mdfw_wifi_unlock();
  return 0;
}

int cyw43_wifi_link_status(cyw43_t *self, int itf) {
  (void)self;
  return itf == CYW43_ITF_STA ? s_join : CYW43_LINK_DOWN;
}

int cyw43_tcpip_link_status(cyw43_t *self, int itf) {
  struct netif *n = &self->netif[itf];
  if (s_itf_up[itf] && netif_is_up(n) && netif_is_link_up(n)) {
    return ip4_addr_isany_val(*netif_ip4_addr(n)) ? CYW43_LINK_NOIP : CYW43_LINK_UP;
  }
  return cyw43_wifi_link_status(self, itf);
}

int cyw43_arch_wifi_connect_bssid_async(const char *ssid, const uint8_t *bssid,
                                        const char *pw, uint32_t auth) {
  if (!pw) auth = CYW43_AUTH_OPEN;
  return cyw43_wifi_join(&cyw43_state, strlen(ssid), (const uint8_t *)ssid,
                         pw ? strlen(pw) : 0, (const uint8_t *)pw, auth, bssid,
                         CYW43_CHANNEL_NONE);
}

int cyw43_arch_wifi_connect_async(const char *ssid, const char *pw, uint32_t auth) {
  return cyw43_arch_wifi_connect_bssid_async(ssid, NULL, pw, auth);
}

/* As the Pico SDK's: poll until connected, failed or out of time; with
 * no network found, keep trying. */
static int connect_until(const char *ssid, const uint8_t *bssid, const char *pw,
                         uint32_t auth, absolute_time_t until) {
  int err = cyw43_arch_wifi_connect_bssid_async(ssid, bssid, pw, auth);
  if (err) return err;
  int status = CYW43_LINK_UP + 1;
  while (status >= 0 && status != CYW43_LINK_UP) {
    int now = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (now == CYW43_LINK_NONET) {
      now = CYW43_LINK_JOIN;
      err = cyw43_arch_wifi_connect_bssid_async(ssid, bssid, pw, auth);
      if (err) return err;
    }
    status = now;
    if (time_reached(until)) return PICO_ERROR_TIMEOUT;
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(until);
  }
  if (status == CYW43_LINK_UP) return PICO_OK;
  return status == CYW43_LINK_BADAUTH ? PICO_ERROR_BADAUTH : PICO_ERROR_CONNECT_FAILED;
}

int cyw43_arch_wifi_connect_blocking(const char *ssid, const char *pw, uint32_t auth) {
  return connect_until(ssid, NULL, pw, auth, at_the_end_of_time);
}

int cyw43_arch_wifi_connect_bssid_blocking(const char *ssid, const uint8_t *bssid,
                                           const char *pw, uint32_t auth) {
  return connect_until(ssid, bssid, pw, auth, at_the_end_of_time);
}

int cyw43_arch_wifi_connect_timeout_ms(const char *ssid, const char *pw, uint32_t auth,
                                       uint32_t timeout_ms) {
  return connect_until(ssid, NULL, pw, auth, make_timeout_time_ms(timeout_ms));
}

int cyw43_arch_wifi_connect_bssid_timeout_ms(const char *ssid, const uint8_t *bssid,
                                             const char *pw, uint32_t auth,
                                             uint32_t timeout_ms) {
  return connect_until(ssid, bssid, pw, auth, make_timeout_time_ms(timeout_ms));
}

int cyw43_wifi_scan(cyw43_t *self, cyw43_wifi_scan_options_t *opts, void *env,
                    int (*result_cb)(void *, const cyw43_ev_scan_result_t *)) {
  (void)self;
  int rc = -1;
  mdfw_wifi_lock();
  if (s_powered) {
    size_t len = opts ? opts->ssid_len : 0;
    if (len > 32) len = 32;
    if (len) memcpy(s_scan_ssid, opts->ssid, len);
    s_scan_ssid[len] = 0;
    s_scan_cb = result_cb;
    s_scan_env = env;
    s_scan_at = make_timeout_time_ms(SCAN_MS);
    s_scan_active = true;
    rc = 0;
  }
  mdfw_wifi_unlock();
  return rc;
}

bool cyw43_wifi_scan_active(cyw43_t *self) {
  (void)self;
  return s_scan_active;
}

/* ------------------------------------------------------------------ */
/* Frames, polling, the lock                                            */
/* ------------------------------------------------------------------ */

int cyw43_send_ethernet(cyw43_t *self, int itf, size_t len, const void *buf, bool is_pbuf) {
  (void)self;
  uint8_t frame[2048];
  if (len > sizeof(frame)) return -1;
  if (is_pbuf) {
    pbuf_copy_partial((const struct pbuf *)buf, frame, (u16_t)len, 0);
    buf = frame;
  }
  int rc = -1;
  mdfw_wifi_lock();
  if (itf == CYW43_ITF_STA && s_powered && s_join == CYW43_LINK_JOIN) {
    mdfw_net_send(buf, len);
    rc = 0;
  }
  mdfw_wifi_unlock();
  return rc;
}

void cyw43_arch_poll(void) {
  if (s_context) async_context_poll(s_context);
}

void cyw43_arch_wait_for_work_until(absolute_time_t until) {
  if (s_context) {
    async_context_wait_for_work_until(s_context, until);
  } else {
    sleep_until(until);
  }
}

void cyw43_arch_lwip_begin(void) { mdfw_wifi_lock(); }
void cyw43_arch_lwip_end(void) { mdfw_wifi_unlock(); }

int cyw43_arch_lwip_protect(int (*func)(void *param), void *param) {
  mdfw_wifi_lock();
  const int r = func(param);
  mdfw_wifi_unlock();
  return r;
}

/* ------------------------------------------------------------------ */
/* Everything else                                                      */
/* ------------------------------------------------------------------ */

/* Raspberry Pi's OUI, as a Pico W's MAC has, then the board ID's end. */
int cyw43_wifi_get_mac(cyw43_t *self, int itf, uint8_t mac[6]) {
  (void)self;
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id);
  mac[0] = 0x28;
  mac[1] = 0xcd;
  mac[2] = 0xc1;
  mac[3] = id.id[5];
  mac[4] = id.id[6];
  mac[5] = id.id[7];
  if (itf == CYW43_ITF_AP) mac[0] |= 0x02; /* locally administered */
  return 0;
}

int cyw43_wifi_get_bssid(cyw43_t *self, uint8_t bssid[6]) {
  (void)self;
  memcpy(bssid, ROUTER_MAC, sizeof(ROUTER_MAC));
  return 0;
}

int cyw43_wifi_get_rssi(cyw43_t *self, int32_t *rssi) {
  (void)self;
  *rssi = mdfw_option_int("wifi_rssi", -45);
  return 0;
}

int cyw43_wifi_pm(cyw43_t *self, uint32_t pm) {
  (void)self;
  s_pm = pm;
  return 0;
}

int cyw43_wifi_get_pm(cyw43_t *self, uint32_t *pm) {
  (void)self;
  *pm = s_pm;
  return 0;
}

int cyw43_gpio_set(cyw43_t *self, int gpio, bool val) {
  (void)self;
  if (gpio == CYW43_WL_GPIO_LED_PIN && val != s_led) {
    s_led = val;
    mdfw_debug("Wi-Fi: LED %s\n", val ? "on" : "off");
  }
  return 0;
}

int cyw43_gpio_get(cyw43_t *self, int gpio, bool *val) {
  (void)self;
  *val = gpio == CYW43_WL_GPIO_LED_PIN && s_led;
  return 0;
}

void cyw43_arch_gpio_put(uint wl_gpio, bool value) {
  cyw43_gpio_set(&cyw43_state, (int)wl_gpio, value);
}

bool cyw43_arch_gpio_get(uint wl_gpio) {
  bool v = false;
  cyw43_gpio_get(&cyw43_state, (int)wl_gpio, &v);
  return v;
}
