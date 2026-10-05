/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: wifi_test.c
 * Description: Checks EmuMD's Wi-Fi with the Wi-Fi example, loaded as
 *              Hatari loads it: joining, DHCP, an HTTP GET from a server
 *              on this computer (10.0.2.2 to the device), one bigger than
 *              lwIP's TCP window, a 404, this computer fetching the
 *              device's own page through a forwarded port, a join that
 *              fails, and power cycles in between. WIFI_TEST_INTERNET=1 adds a page from
 *              the internet, found with DNS; VERBOSE=1 shows the log.
 *
 *   wifi_test examples/wifi/build/wi-fi.mdfw
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "emumd_plugin.h"

static int s_fails;
#define CHECK(c)                                                   \
  do {                                                             \
    if (!(c)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
      s_fails++;                                                   \
    }                                                              \
  } while (0)

/* examples/wifi/firmware/wifi_demo.h */
#define DEMO_STATUS 0x1000
#define DEMO_STATUS_TEXT 0x1002
#define DEMO_ANSWERS 0x1100
#define DEMO_ANSWER_TEXT 0x1102
#define DEMO_FETCH 0x1234

#define BIG_BYTES 40000

/* ---- a web server on this computer ----------------------------------- */

static int s_port;

static void *serve(void *arg) {
  const int ls = *(int *)arg;
  static char big[BIG_BYTES + 1];
  for (int i = 0; i < BIG_BYTES; i++) big[i] = (i % 40 == 39) ? '\n' : (char)('a' + i % 26);
  for (;;) {
    const int s = accept(ls, NULL, NULL);
    if (s < 0) continue;
    char req[1024];
    const ssize_t n = recv(s, req, sizeof(req) - 1, 0);
    req[n > 0 ? n : 0] = 0;
    const char *body = NULL;
    const char *status = "404 Not Found";
    if (!strncmp(req, "GET /small ", 11)) {
      body = "Hello from this computer\n";
      status = "200 OK";
    } else if (!strncmp(req, "GET /big ", 9)) {
      body = big;
      status = "200 OK";
    }
    if (!body) body = "Not here\n";
    char head[256];
    snprintf(head, sizeof(head),
             "HTTP/1.0 %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             status, strlen(body));
    send(s, head, strlen(head), 0);
    for (size_t off = 0, len = strlen(body); off < len;) {
      const ssize_t w = send(s, body + off, len - off, 0);
      if (w <= 0) break;
      off += (size_t)w;
    }
    close(s);
  }
  return NULL;
}

static void start_server(void) {
  static int ls;
  ls = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(ls, (struct sockaddr *)&a, sizeof(a)) || listen(ls, 4)) {
    perror("server");
    exit(1);
  }
  socklen_t len = sizeof(a);
  getsockname(ls, (struct sockaddr *)&a, &len);
  s_port = ntohs(a.sin_port);
  pthread_t t;
  pthread_create(&t, NULL, serve, &ls);
}

/* ---- the emulator's side --------------------------------------------- */

static const emumd_plugin_t *s_plugin;
static uint64_t s_now = 1000;
static int s_verbose;

static void host_log(const char *line) {
  if (s_verbose) fprintf(stderr, "%s\n", line);
}

/* What the ST would read: ROM4 words, high byte first. */
static uint16_t rom4_word(uint32_t offset) { return s_plugin->rom4_read(offset, s_now); }

static void rom4_text(uint32_t offset, char *out, size_t len) {
  size_t i = 0;
  for (; i + 1 < len; i += 2) {
    const uint16_t w = rom4_word(offset + (uint32_t)i);
    out[i] = (char)(w >> 8);
    out[i + 1] = (char)w;
    if (!out[i] || !out[i + 1]) break;
  }
  out[len - 1] = 0;
}

/* A frame of emulated time, and a little real time for the network. */
static void frame(void) {
  const struct timespec ms = {0, 1000000};
  s_now += 20000;
  s_plugin->tick(s_now);
  nanosleep(&ms, NULL);
}

/* Until ROM4's word at `offset` is non-zero, or 30 emulated seconds. */
static uint16_t wait_word(uint32_t offset) {
  for (int i = 0; i < 1500; i++) {
    const uint16_t w = rom4_word(offset);
    if (w) return w;
    frame();
  }
  return 0;
}

/* The device's own page, through a forward from localhost:port. */
static void fetch_from_device(int port, char *page, size_t len) {
  page[0] = 0;
  const int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons((uint16_t)port);
  if (connect(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
    close(s);
    return;
  }
  const char *req = "GET / HTTP/1.0\r\n\r\n";
  send(s, req, strlen(req), 0);
  fcntl(s, F_SETFL, O_NONBLOCK);
  size_t got = 0;
  for (int i = 0; i < 500 && got + 1 < len; i++) {
    const ssize_t n = recv(s, page + got, len - 1 - got, 0);
    if (n == 0) break;
    if (n > 0) got += (size_t)n;
    frame();
  }
  page[got] = 0;
  close(s);
}

/* A port nobody is using just now. */
static int free_port(void) {
  const int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bind(s, (struct sockaddr *)&a, sizeof(a));
  socklen_t len = sizeof(a);
  getsockname(s, (struct sockaddr *)&a, &len);
  close(s);
  return ntohs(a.sin_port);
}

/* Power on with these options, join, fetch the URL (and, given a
 * forwarded port, the device's own page); the answer, or "". */
static uint16_t run(const char *options, char *status, char *answer, size_t len,
                    int forwarded, char *page) {
  static emumd_host_t host;
  host = (emumd_host_t){EMUMD_PLUGIN_ABI, "/tmp", options, s_verbose, host_log};
  status[0] = answer[0] = 0;
  CHECK(s_plugin->power_on(&host, s_now) == 0);
  const uint16_t joined = wait_word(DEMO_STATUS);
  rom4_text(DEMO_STATUS_TEXT, status, len);
  if (joined == 1) {
    s_plugin->rom3_read(DEMO_FETCH, s_now);
    if (wait_word(DEMO_ANSWERS)) rom4_text(DEMO_ANSWER_TEXT, answer, len);
    if (forwarded) fetch_from_device(forwarded, page, len);
  }
  s_plugin->power_off();
  return joined;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: wifi_test WI-FI.mdfw\n");
    return 2;
  }
  s_verbose = getenv("VERBOSE") != NULL;
  void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    fprintf(stderr, "%s\n", dlerror());
    return 1;
  }
  const emumd_plugin_entry_t entry = (emumd_plugin_entry_t)dlsym(lib, EMUMD_PLUGIN_ENTRY);
  s_plugin = entry ? entry() : NULL;
  if (!s_plugin || s_plugin->abi != EMUMD_PLUGIN_ABI) {
    fprintf(stderr, "%s: not a .mdfw\n", argv[1]);
    return 1;
  }
  start_server();

  char options[256], status[4096], answer[4096];

  /* Joining, DHCP and a small page from 10.0.2.2, which is this computer. */
  snprintf(options, sizeof(options), "url=http://10.0.2.2:%d/small\n", s_port);
  CHECK(run(options, status, answer, sizeof(answer), 0, NULL) == 1);
  CHECK(strstr(status, "Joined EmuMD as 10.0.2.15") != NULL);
  CHECK(strstr(answer, "HTTP 200, 25 bytes") != NULL);
  CHECK(strstr(answer, "Hello from this computer") != NULL);

  /* More than lwIP's TCP window, which the firmware has to reopen. */
  snprintf(options, sizeof(options), "url=http://10.0.2.2:%d/big\n", s_port);
  CHECK(run(options, status, answer, sizeof(answer), 0, NULL) == 1);
  CHECK(strstr(answer, "HTTP 200, 40000 bytes") != NULL);
  CHECK(strstr(answer, "abcdefghijklmnopqrstuvwxyzabcdefghijkl\r\n") != NULL);

  /* This computer fetching the device's own page, through a forward. */
  const int port = free_port();
  char page[4096];
  snprintf(options, sizeof(options), "url=http://10.0.2.2:%d/small\nwifi_forward=tcp:%d:80\n",
           s_port, port);
  CHECK(run(options, status, answer, sizeof(answer), port, page) == 1);
  CHECK(strstr(page, "HTTP/1.0 200 OK") != NULL);
  CHECK(strstr(page, "Hello from the Multi-device (pages fetched: 1)") != NULL);

  /* Not there. */
  snprintf(options, sizeof(options), "url=http://10.0.2.2:%d/nothing\n", s_port);
  CHECK(run(options, status, answer, sizeof(answer), 0, NULL) == 1);
  CHECK(strstr(answer, "HTTP 404, 9 bytes") != NULL);

  /* A host name, looked up with DNS: this needs the internet. */
  if (getenv("WIFI_TEST_INTERNET")) {
    CHECK(run("url=http://example.com/\n", status, answer, sizeof(answer), 0, NULL) == 1);
    CHECK(strstr(answer, "HTTP 200") != NULL);
  }

  /* A join that fails: PICO_ERROR_BADAUTH. */
  CHECK(run("wifi=badauth\n", status, answer, sizeof(answer), 0, NULL) == 2);
  CHECK(strstr(status, "Could not join EmuMD (error -7)") != NULL);

  if (s_fails) {
    fprintf(stderr, "status: %s\nanswer: %s\n", status, answer);
    fprintf(stderr, "wifi_test: %d check(s) failed\n", s_fails);
    return 1;
  }
  printf("wifi_test: all checks passed\n");
  return 0;
}
