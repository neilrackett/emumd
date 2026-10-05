/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: wifi_demo.c
 * Description: The Wi-Fi example's logic (see wifi_demo.h): plain Pico W
 *              code, with cyw43_arch and lwIP's HTTP client, so the same
 *              file would build into a real firmware.
 */
#include "wifi_demo.h"

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "lwip/apps/http_client.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

/* Any will do in EmuMD; a real firmware would keep the user's in flash. */
#define WIFI_SSID "EmuMD"
#define WIFI_PASSWORD "emumd"

#define LINES 10   /* of the page, shown on the ST */
#define COLUMNS 38 /* fits a low resolution screen */

static volatile uint8_t *s_rom4;
static const char *s_url;
static volatile bool s_fetch;
static uint16_t s_answers;

/* The RP keeps ROM4 as little-endian halfwords, and the ST reads each as a
 * big-endian word, so a byte for ST address n goes at RP byte n ^ 1. */
static void st_puts(uint32_t offset, const char *text) {
  size_t i = 0;
  do {
    s_rom4[(offset + i) ^ 1u] = (uint8_t)text[i];
  } while (text[i++]);
}

static void st_put16(uint32_t offset, uint16_t value) {
  *(volatile uint16_t *)(s_rom4 + offset) = value;
}

void wifi_demo_init(uintptr_t rom4_base, const char *url) {
  s_rom4 = (volatile uint8_t *)rom4_base;
  s_url = url;
  s_answers = 0;
  s_fetch = false;
  st_put16(DEMO_STATUS, 0);
  st_put16(DEMO_ANSWERS, 0);
}

/* Interrupt context: note it, let the main loop fetch. */
void wifi_demo_sample(uint16_t sample) {
  if (sample == DEMO_FETCH) s_fetch = true;
}

/* ------------------------------------------------------------------ */
/* Fetching a page                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
  bool done;
  httpc_result_t result;
  u32_t status;
  size_t bytes;
  char text[2048]; /* the start of the page */
  size_t len;
} fetch_t;

static fetch_t s_page;
static httpc_connection_t s_settings;

static err_t on_body(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
  (void)err;
  fetch_t *f = arg;
  if (!p) return ERR_OK;
  f->bytes += p->tot_len;
  const size_t room = sizeof(f->text) - 1 - f->len;
  f->len += pbuf_copy_partial(p, f->text + f->len, (u16_t)(room < p->tot_len ? room : p->tot_len), 0);
  f->text[f->len] = 0;
  altcp_recved(pcb, p->tot_len); /* make room for more */
  pbuf_free(p);
  return ERR_OK;
}

static void on_done(void *arg, httpc_result_t result, u32_t rx_content_len, u32_t srv_res,
                    err_t err) {
  (void)rx_content_len;
  (void)err;
  fetch_t *f = arg;
  f->result = result;
  f->status = srv_res;
  f->done = true;
}

/* http://host[:port][/path] */
static bool parse_url(const char *url, char *host, size_t hostlen, u16_t *port,
                      const char **path) {
  if (strncmp(url, "http://", 7) != 0) return false;
  const char *h = url + 7;
  const char *slash = strchr(h, '/');
  const char *end = slash ? slash : h + strlen(h);
  const char *colon = memchr(h, ':', (size_t)(end - h));
  const size_t n = (size_t)((colon ? colon : end) - h);
  if (n == 0 || n >= hostlen) return false;
  memcpy(host, h, n);
  host[n] = 0;
  *port = colon ? (u16_t)atoi(colon + 1) : 80;
  *path = slash ? slash : "/";
  return true;
}

/* The page's first lines, as the ST prints them. */
static void show_page(const char *text, char *out, size_t outlen) {
  size_t o = strlen(out);
  int line = 0, col = 0;
  for (const char *c = text; *c && line < LINES && o + 4 < outlen; c++) {
    if (*c == '\n') {
      out[o++] = '\r';
      out[o++] = '\n';
      line++;
      col = 0;
    } else if (*c != '\r' && col < COLUMNS) {
      out[o++] = (*c >= 32 && *c < 127) ? *c : '?';
      col++;
    }
  }
  if (col) {
    out[o++] = '\r';
    out[o++] = '\n';
  }
  out[o] = 0;
}

static void fetch(char *out, size_t outlen) {
  char host[64];
  u16_t port;
  const char *path;
  if (!parse_url(s_url, host, sizeof(host), &port, &path)) {
    snprintf(out, outlen, "Not an http:// URL:\r\n%.*s\r\n", COLUMNS, s_url);
    return;
  }
  const char *shown = s_url + 7; /* without http:// */
  memset(&s_page, 0, sizeof(s_page));
  memset(&s_settings, 0, sizeof(s_settings));
  s_settings.result_fn = on_done;
  httpc_state_t *state;
  cyw43_arch_lwip_begin();
  const err_t err =
      httpc_get_file_dns(host, port, path, &s_settings, on_body, &s_page, &state);
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    snprintf(out, outlen, "%.*s\r\ncannot fetch it (%d)\r\n", COLUMNS, shown, err);
    return;
  }
  /* lwIP's HTTP client gives up by itself, after 15 s of silence. */
  while (!s_page.done) {
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
  }
  DPRINTF("wifi: %s: result %d, HTTP %lu, %lu bytes\n", s_url, s_page.result,
          (unsigned long)s_page.status, (unsigned long)s_page.bytes);
  if (s_page.result != HTTPC_RESULT_OK) {
    snprintf(out, outlen, "%.*s\r\nfailed (lwIP httpc result %d)\r\n", COLUMNS, shown,
             s_page.result);
    return;
  }
  snprintf(out, outlen, "%.*s\r\nHTTP %lu, %lu bytes:\r\n", COLUMNS, shown,
           (unsigned long)s_page.status, (unsigned long)s_page.bytes);
  show_page(s_page.text, out, outlen);
}

/* ------------------------------------------------------------------ */
/* Being fetched from: a web page of its own, on port 80                */
/* ------------------------------------------------------------------ */

static err_t on_request(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
  (void)err;
  if (!p) { /* the other side has gone */
    tcp_close(pcb);
    return ERR_OK;
  }
  tcp_recved(pcb, p->tot_len);
  pbuf_free(p);
  if (arg) return ERR_OK; /* answered already */
  char reply[160];
  snprintf(reply, sizeof(reply),
           "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n"
           "Hello from the Multi-device (pages fetched: %u)\n",
           s_answers);
  tcp_arg(pcb, pcb);
  tcp_write(pcb, reply, (u16_t)strlen(reply), TCP_WRITE_FLAG_COPY);
  tcp_close(pcb); /* once it is sent */
  return ERR_OK;
}

static err_t on_connect(void *arg, struct tcp_pcb *pcb, err_t err) {
  (void)arg;
  if (err != ERR_OK || !pcb) return ERR_VAL;
  tcp_arg(pcb, NULL);
  tcp_recv(pcb, on_request);
  return ERR_OK;
}

static void serve(void) {
  cyw43_arch_lwip_begin();
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (pcb && tcp_bind(pcb, IP_ANY_TYPE, 80) == ERR_OK) {
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, on_connect);
  }
  cyw43_arch_lwip_end();
}

/* ------------------------------------------------------------------ */
/* The main loop                                                        */
/* ------------------------------------------------------------------ */

void wifi_demo_main(void) {
  static char text[DEMO_ANSWER_MAX];
  int rc = cyw43_arch_init();
  if (rc == 0) {
    cyw43_arch_enable_sta_mode();
    rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                            CYW43_AUTH_WPA2_AES_PSK, 30000);
  }
  if (rc != 0) {
    snprintf(text, sizeof(text), "Could not join %s (error %d)\r\n", WIFI_SSID, rc);
    st_puts(DEMO_STATUS_TEXT, text);
    st_put16(DEMO_STATUS, 2);
    for (;;) sleep_ms(1000);
  }
  const struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
  snprintf(text, sizeof(text), "Joined %s as %s\r\n", WIFI_SSID,
           ip4addr_ntoa(netif_ip4_addr(n)));
  st_puts(DEMO_STATUS_TEXT, text);
  st_put16(DEMO_STATUS, 1);
  DPRINTF("wifi: %s", text);
  serve();

  for (;;) {
    if (s_fetch) {
      s_fetch = false;
      fetch(text, sizeof(text));
      st_puts(DEMO_ANSWER_TEXT, text);
      st_put16(DEMO_ANSWERS, ++s_answers); /* last: the ST waits for this */
    }
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(10));
  }
}
