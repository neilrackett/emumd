/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: bluepad32.c
 * Description: Bluepad32, the Bluetooth controller host a Pico W runs on
 *              its CYW43, for a firmware built with [bluetooth] in
 *              mdfw.ini: the custom-platform interface (shim/uni.h), with
 *              scripted controllers in place of the radio. There is no
 *              Bluetooth on the far side. The pads come from a script
 *              named with --md-option pads=FILE, which connects them,
 *              moves their sticks and presses their buttons at the times
 *              it says, in emulated milliseconds from uni_init():
 *
 *                # ms  slot  what
 *                0     0     connect Xbox Wireless Controller
 *                500   0     state buttons=0x1 lx=-512
 *                700   0     state buttons=0
 *                2000  0     disconnect
 *
 *              The platform's callbacks run inside cyw43_arch_poll(),
 *              where BTstack would run them on the Pico W, so a firmware
 *              that polls the chip from its main loop hears its pads
 *              there. What the firmware asks of a pad (LEDs, rumble) is
 *              logged, so a test can grep for it.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdfw.h"
#include "runtime.h"
#include "uni.h"

const int AXIS_NORMALIZE_RANGE = 1024;
const int AXIS_THRESHOLD = 1024 / 8;

#define MAX_DEVICES CONFIG_BLUEPAD32_MAX_DEVICES
#define MAX_EVENTS 256
#define MAX_NAME 64
#define MAX_LINE 256
#define MAX_DEFERRED 32

/* The device table: a device's index is its place in it, so a platform
 * can go from one to the other as Bluepad32 lets it. */
static uni_hid_device_t g_devices[MAX_DEVICES];
static bool s_used[MAX_DEVICES];

static struct uni_platform *s_platform;
static bool s_started;      /* uni_init() has run */
static bool s_init_pending; /* on_init_complete is owed to the next poll */
static bool s_scanning;
static bool s_incoming = true;
static uint64_t s_zero_us; /* uni_init(): the script's 0 ms */

/* ------------------------------------------------------------------ */
/* The script                                                           */
/* ------------------------------------------------------------------ */

enum { EV_CONNECT, EV_STATE, EV_DISCONNECT, EV_NORUMBLE };

/* state's keys, by bit, in the order of the names below. */
enum {
  SET_BUTTONS = 1 << 0,
  SET_MISC = 1 << 1,
  SET_DPAD = 1 << 2,
  SET_LX = 1 << 3,
  SET_LY = 1 << 4,
  SET_RX = 1 << 5,
  SET_RY = 1 << 6,
  SET_LT = 1 << 7,
  SET_RT = 1 << 8,
};

static const struct {
  const char *key;
  long min, max;
} STATE_KEYS[] = {
    {"buttons", 0, 0xffff}, {"misc", 0, 0xff}, {"dpad", 0, 0xff},
    {"lx", -512, 511},      {"ly", -512, 511}, {"rx", -512, 511},
    {"ry", -512, 511},      {"lt", 0, 1023},   {"rt", 0, 1023},
};

typedef struct {
  uint32_t at_ms;
  int line;
  uint8_t slot;
  uint8_t kind;
  uint16_t set;        /* EV_STATE: the keys given */
  uni_gamepad_t gp;    /* EV_STATE: their values */
  char name[MAX_NAME]; /* EV_CONNECT */
} event_t;

static event_t s_events[MAX_EVENTS];
static int s_event_count, s_event_next;
static char s_script[1024]; /* its path, for messages */

static const char *script_name(void) {
  const char *slash = strrchr(s_script, '/');
  return slash ? slash + 1 : s_script;
}

static void script_error(int line, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void script_error(int line, const char *fmt, ...) {
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  mdfw_log("Bluepad32: %s line %d: %s", script_name(), line, msg);
}

/* A decimal or 0x hex number, with an optional sign, within [min, max]. */
static bool parse_number(const char *s, long min, long max, long *out) {
  const char *p = s;
  bool neg = false;
  if (*p == '-' || *p == '+') neg = *p++ == '-';
  int base = 10;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    base = 16;
    p += 2;
  }
  if (!*p) return false;
  char *end = NULL;
  const long v = strtol(p, &end, base);
  if (!end || *end || end == p) return false;
  const long n = neg ? -v : v;
  if (n < min || n > max) return false;
  *out = n;
  return true;
}

/* The next space-separated word of *p, cut off, or NULL. */
static char *next_word(char **p) {
  char *s = *p;
  while (*s && isspace((unsigned char)*s)) s++;
  if (!*s) return NULL;
  char *w = s;
  while (*s && !isspace((unsigned char)*s)) s++;
  if (*s) *s++ = 0;
  *p = s;
  return w;
}

static const char *rest_of(char *p) {
  while (*p && isspace((unsigned char)*p)) p++;
  char *end = p + strlen(p);
  while (end > p && isspace((unsigned char)end[-1])) *--end = 0;
  return p;
}

/* One line of the script into s_events, or an error for it. */
static void parse_line(char *text, int line) {
  char *hash = strchr(text, '#');
  if (hash) *hash = 0;
  char *p = text;
  const char *ms = next_word(&p);
  if (!ms) return; /* blank, or only a comment */
  if (s_event_count == MAX_EVENTS) {
    script_error(line, "more than %d events", MAX_EVENTS);
    return;
  }
  event_t *e = &s_events[s_event_count];
  memset(e, 0, sizeof(*e));
  e->line = line;
  long n;
  if (!parse_number(ms, 0, 0x7fffffffL, &n)) {
    script_error(line, "%s? (a time in ms, then slot, then connect, state, disconnect or norumble)",
                 ms);
    return;
  }
  e->at_ms = (uint32_t)n;
  if (s_event_count && e->at_ms < s_events[s_event_count - 1].at_ms) {
    script_error(line, "%u ms is before line %d's %u ms", e->at_ms,
                 s_events[s_event_count - 1].line, s_events[s_event_count - 1].at_ms);
    return;
  }
  const char *slot = next_word(&p);
  if (!slot || !parse_number(slot, 0, MAX_DEVICES - 1, &n)) {
    script_error(line, "slot %s? (0 to %d)", slot ? slot : "missing", MAX_DEVICES - 1);
    return;
  }
  e->slot = (uint8_t)n;
  const char *what = next_word(&p);
  if (!what) {
    script_error(line, "connect, state, disconnect or norumble?");
    return;
  }
  if (!strcmp(what, "connect")) {
    e->kind = EV_CONNECT;
    const char *name = rest_of(p);
    if (strlen(name) >= MAX_NAME) {
      script_error(line, "name longer than %d characters", MAX_NAME - 1);
      return;
    }
    strcpy(e->name, name);
  } else if (!strcmp(what, "state")) {
    e->kind = EV_STATE;
    const char *kv;
    while ((kv = next_word(&p)) != NULL) {
      const char *eq = strchr(kv, '=');
      if (!eq || eq == kv) {
        script_error(line, "%s? (key=value)", kv);
        return;
      }
      int k = 0;
      for (; k < (int)(sizeof(STATE_KEYS) / sizeof(STATE_KEYS[0])); k++) {
        if ((size_t)(eq - kv) == strlen(STATE_KEYS[k].key) &&
            !strncmp(kv, STATE_KEYS[k].key, (size_t)(eq - kv)))
          break;
      }
      if (k == (int)(sizeof(STATE_KEYS) / sizeof(STATE_KEYS[0]))) {
        script_error(line, "%.*s? (buttons, misc, dpad, lx, ly, rx, ry, lt or rt)",
                     (int)(eq - kv), kv);
        return;
      }
      if (!parse_number(eq + 1, STATE_KEYS[k].min, STATE_KEYS[k].max, &n)) {
        script_error(line, "%s? (%ld to %ld)", kv, STATE_KEYS[k].min, STATE_KEYS[k].max);
        return;
      }
      e->set |= (uint16_t)(1u << k);
      switch (1u << k) {
        case SET_BUTTONS: e->gp.buttons = (uint16_t)n; break;
        case SET_MISC: e->gp.misc_buttons = (uint8_t)n; break;
        case SET_DPAD: e->gp.dpad = (uint8_t)n; break;
        case SET_LX: e->gp.axis_x = (int32_t)n; break;
        case SET_LY: e->gp.axis_y = (int32_t)n; break;
        case SET_RX: e->gp.axis_rx = (int32_t)n; break;
        case SET_RY: e->gp.axis_ry = (int32_t)n; break;
        case SET_LT: e->gp.brake = (int32_t)n; break;
        case SET_RT: e->gp.throttle = (int32_t)n; break;
      }
    }
  } else if (!strcmp(what, "disconnect") || !strcmp(what, "norumble")) {
    e->kind = what[0] == 'd' ? EV_DISCONNECT : EV_NORUMBLE;
    if (next_word(&p)) {
      script_error(line, "%s takes nothing after it", what);
      return;
    }
  } else {
    script_error(line, "%s? (connect, state, disconnect or norumble)", what);
    return;
  }
  s_event_count++;
}

static void read_script(void) {
  s_event_count = s_event_next = 0;
  const char *path = mdfw_option("pads");
  if (!path || !*path) {
    s_script[0] = 0;
    mdfw_log("Bluepad32: no pads (--md-option pads=FILE scripts some)");
    return;
  }
  snprintf(s_script, sizeof(s_script), "%s", path);
  FILE *f = fopen(path, "r");
  if (!f) {
    mdfw_log("Bluepad32: cannot read the pads script %s", path);
    return;
  }
  char text[MAX_LINE];
  int line = 0;
  while (fgets(text, sizeof(text), f)) {
    line++;
    if (!strchr(text, '\n') && !feof(f)) {
      script_error(line, "longer than %d characters", MAX_LINE - 2);
      int c;
      while ((c = fgetc(f)) != EOF && c != '\n') {
      }
      continue;
    }
    parse_line(text, line);
  }
  fclose(f);
  mdfw_log("Bluepad32: %d pad events from %s", s_event_count, path);
}

/* ------------------------------------------------------------------ */
/* The pads                                                             */
/* ------------------------------------------------------------------ */

static int idx_of(const uni_hid_device_t *d) { return uni_hid_device_get_idx_for_instance(d); }

/* What the parser lets the platform do to a pad: it goes to the log. */
static void pad_set_player_leds(uni_hid_device_t *d, uint8_t leds) {
  mdfw_log("Bluepad32: pad %d leds=0x%02x", idx_of(d), leds);
}

static void pad_set_lightbar_color(uni_hid_device_t *d, uint8_t r, uint8_t g, uint8_t b) {
  mdfw_log("Bluepad32: pad %d lightbar=%u,%u,%u", idx_of(d), r, g, b);
}

static void pad_play_dual_rumble(uni_hid_device_t *d, uint16_t start_delay_ms,
                                 uint16_t duration_ms, uint8_t weak, uint8_t strong) {
  (void)start_delay_ms;
  mdfw_log("Bluepad32: pad %d rumble weak=%u strong=%u for %u ms", idx_of(d), weak, strong,
           duration_ms);
}

static void deliver(uni_hid_device_t *d) {
  if (s_platform->on_controller_data) s_platform->on_controller_data(d, &d->controller);
}

static void free_slot(int slot) {
  memset(&g_devices[slot], 0, sizeof(g_devices[slot]));
  s_used[slot] = false;
}

/* A pad arrives: offered to the platform as a scan would, then connected,
 * ready (if the platform agrees), and reporting itself at rest. */
static void connect_pad(const event_t *e) {
  const int slot = e->slot;
  if (s_used[slot]) {
    script_error(e->line, "pad %d is already connected", slot);
    return;
  }
  uni_hid_device_t *d = &g_devices[slot];
  memset(d, 0, sizeof(*d));
  bd_addr_t addr = {0x02, 0x4d, 0x44, 0x00, 0x00, (uint8_t)(slot + 1)};
  d->cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD;
  snprintf(d->name, sizeof(d->name), "%s", e->name);
  if (s_platform->on_device_discovered) {
    const uni_error_t rc = s_platform->on_device_discovered(addr, d->name, (uint16_t)d->cod, 200);
    if (rc != UNI_ERROR_SUCCESS) {
      mdfw_log("Bluepad32: pad %d \"%s\" not wanted by the platform (%d)", slot, d->name, rc);
      memset(d, 0, sizeof(*d));
      return;
    }
  }
  s_used[slot] = true;
  memcpy(d->conn.btaddr, addr, sizeof(bd_addr_t));
  d->conn.handle = (hci_con_handle_t)(0x80 + slot);
  d->conn.rssi = 200;
  d->conn.connected = true;
  d->conn.state = UNI_BT_CONN_STATE_DEVICE_PENDING_READY;
  d->conn.protocol = UNI_BT_CONN_PROTOCOL_BR_EDR;
  d->controller_type = k_eControllerType_GenericController;
  d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD; /* at rest: all zero */
  d->controller.battery = UNI_CONTROLLER_BATTERY_NOT_AVAILABLE;
  d->report_parser.set_player_leds = pad_set_player_leds;
  d->report_parser.set_lightbar_color = pad_set_lightbar_color;
  d->report_parser.play_dual_rumble = pad_play_dual_rumble;
  mdfw_log("Bluepad32: pad %d connected \"%s\"", slot, d->name);
  if (s_platform->on_device_connected) s_platform->on_device_connected(d);
  if (s_platform->on_device_ready) {
    const uni_error_t rc = s_platform->on_device_ready(d);
    if (rc != UNI_ERROR_SUCCESS) {
      /* As Bluepad32 does: dropped and deleted, with no disconnect. */
      mdfw_log("Bluepad32: pad %d refused by the platform (%d)", slot, rc);
      free_slot(slot);
      return;
    }
  }
  d->conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
  mdfw_log("Bluepad32: pad %d ready", slot);
  deliver(d);
}

static void disconnect_pad(int slot) {
  uni_hid_device_t *d = &g_devices[slot];
  d->conn.connected = false;
  d->conn.state = UNI_BT_CONN_STATE_DEVICE_NONE;
  if (s_platform->on_device_disconnected) s_platform->on_device_disconnected(d);
  free_slot(slot);
  mdfw_log("Bluepad32: pad %d disconnected", slot);
}

static void fire(const event_t *e) {
  if (e->kind == EV_CONNECT) {
    connect_pad(e);
    return;
  }
  if (!s_used[e->slot]) {
    script_error(e->line, "pad %d is not connected", e->slot);
    return;
  }
  uni_hid_device_t *d = &g_devices[e->slot];
  switch (e->kind) {
    case EV_STATE: {
      uni_gamepad_t *gp = &d->controller.gamepad;
      if (e->set & SET_BUTTONS) gp->buttons = e->gp.buttons;
      if (e->set & SET_MISC) gp->misc_buttons = e->gp.misc_buttons;
      if (e->set & SET_DPAD) gp->dpad = e->gp.dpad;
      if (e->set & SET_LX) gp->axis_x = e->gp.axis_x;
      if (e->set & SET_LY) gp->axis_y = e->gp.axis_y;
      if (e->set & SET_RX) gp->axis_rx = e->gp.axis_rx;
      if (e->set & SET_RY) gp->axis_ry = e->gp.axis_ry;
      if (e->set & SET_LT) gp->brake = e->gp.brake;
      if (e->set & SET_RT) gp->throttle = e->gp.throttle;
      deliver(d);
      break;
    }
    case EV_DISCONNECT: disconnect_pad(e->slot); break;
    case EV_NORUMBLE: d->report_parser.play_dual_rumble = NULL; break;
  }
}

/* ------------------------------------------------------------------ */
/* What the platform asks (the _safe ones at the next poll)             */
/* ------------------------------------------------------------------ */

enum { CMD_SCAN, CMD_STOP_SCAN, CMD_DEL_KEYS, CMD_DISCONNECT };

typedef struct {
  uint8_t cmd;
  int8_t idx;
} deferred_t;

static deferred_t s_deferred[MAX_DEFERRED];
static unsigned s_deferred_head, s_deferred_tail;

static void defer(uint8_t cmd, int idx) {
  if (s_deferred_tail - s_deferred_head == MAX_DEFERRED) {
    mdfw_log("Bluepad32: more than %d calls waiting for a poll; one dropped", MAX_DEFERRED);
    return;
  }
  deferred_t *d = &s_deferred[s_deferred_tail++ % MAX_DEFERRED];
  d->cmd = cmd;
  d->idx = (int8_t)idx;
}

static void run_deferred(void) {
  while (s_deferred_head != s_deferred_tail) {
    const deferred_t d = s_deferred[s_deferred_head++ % MAX_DEFERRED];
    switch (d.cmd) {
      case CMD_SCAN: uni_bt_start_scanning_and_autoconnect_unsafe(); break;
      case CMD_STOP_SCAN: uni_bt_stop_scanning_unsafe(); break;
      case CMD_DEL_KEYS: uni_bt_del_keys_unsafe(); break;
      case CMD_DISCONNECT:
        if (d.idx >= 0 && d.idx < MAX_DEVICES && s_used[d.idx]) {
          disconnect_pad(d.idx);
        } else {
          mdfw_debug("Bluepad32: no pad %d to disconnect\n", d.idx);
        }
        break;
    }
  }
}

void uni_bt_start_scanning_and_autoconnect_unsafe(void) {
  s_scanning = true;
  mdfw_log("Bluepad32: scanning");
}

void uni_bt_stop_scanning_unsafe(void) {
  s_scanning = false;
  mdfw_log("Bluepad32: scanning stopped");
}

void uni_bt_del_keys_unsafe(void) { mdfw_log("Bluepad32: bonds forgotten"); }

void uni_bt_start_scanning_and_autoconnect_safe(void) { defer(CMD_SCAN, -1); }
void uni_bt_stop_scanning_safe(void) { defer(CMD_STOP_SCAN, -1); }
void uni_bt_del_keys_safe(void) { defer(CMD_DEL_KEYS, -1); }
void uni_bt_disconnect_device_safe(int device_idx) { defer(CMD_DISCONNECT, device_idx); }

bool uni_bt_is_scanning(void) { return s_scanning; }
void uni_bt_allow_incoming_connections(bool allow) { s_incoming = allow; }
bool uni_bt_incoming_connections_is_allowed(void) { return s_incoming; }

/* The Pico W's, made up: locally administered, "MD". */
void uni_bt_get_local_bd_addr_safe(bd_addr_t addr) {
  const bd_addr_t local = {0x02, 0x4d, 0x44, 0x00, 0x00, 0x00};
  memcpy(addr, local, sizeof(bd_addr_t));
}

/* ------------------------------------------------------------------ */
/* Devices                                                              */
/* ------------------------------------------------------------------ */

uni_hid_device_t *uni_hid_device_get_instance_for_idx(int idx) {
  if (idx < 0 || idx >= MAX_DEVICES) return NULL;
  return &g_devices[idx];
}

int uni_hid_device_get_idx_for_instance(const uni_hid_device_t *d) {
  const long idx = d - &g_devices[0];
  if (idx < 0 || idx >= MAX_DEVICES) return -1;
  return (int)idx;
}

bool uni_hid_device_is_gamepad(const uni_hid_device_t *d) {
  return d->controller.klass == UNI_CONTROLLER_CLASS_GAMEPAD;
}
bool uni_hid_device_is_mouse(const uni_hid_device_t *d) {
  return d->controller.klass == UNI_CONTROLLER_CLASS_MOUSE;
}
bool uni_hid_device_is_keyboard(const uni_hid_device_t *d) {
  return d->controller.klass == UNI_CONTROLLER_CLASS_KEYBOARD;
}
bool uni_hid_device_is_virtual_device(const uni_hid_device_t *d) { return d->parent != NULL; }
bool uni_hid_device_has_name(const uni_hid_device_t *d) { return d->name[0] != 0; }
uint16_t uni_hid_device_get_vendor_id(const uni_hid_device_t *d) { return d->vendor_id; }
uint16_t uni_hid_device_get_product_id(const uni_hid_device_t *d) { return d->product_id; }

/* ------------------------------------------------------------------ */
/* Starting, polling, power                                             */
/* ------------------------------------------------------------------ */

void uni_platform_set_custom(struct uni_platform *platform) { s_platform = platform; }
struct uni_platform *uni_get_platform(void) { return s_platform; }

static void reset_all(void) {
  memset(g_devices, 0, sizeof(g_devices));
  memset(s_used, 0, sizeof(s_used));
  s_deferred_head = s_deferred_tail = 0;
  s_event_count = s_event_next = 0;
  s_started = s_init_pending = s_scanning = false;
  s_incoming = true;
}

int uni_init(int argc, const char **argv) {
  if (!s_platform) {
    mdfw_log("Bluepad32: uni_init() before uni_platform_set_custom()");
    return -1;
  }
  reset_all();
  mdfw_log("Bluepad32: platform \"%s\" (no radio: pads are scripted)",
           s_platform->name ? s_platform->name : "");
  if (s_platform->init) s_platform->init(argc, argv);
  s_zero_us = mdfw_time_us();
  read_script();
  s_started = true;
  s_init_pending = true;
  return 0;
}

/* From cyw43_arch_poll() (runtime/wifi/cyw43.c), where BTstack's work
 * would be done: the calls the platform left for us, then the script's
 * events that are due, in order. */
void mdfw_runtime_bt_poll(void) {
  if (!s_started) return;
  if (s_init_pending) {
    s_init_pending = false;
    if (s_platform->on_init_complete) s_platform->on_init_complete();
  }
  run_deferred();
  const uint64_t now = mdfw_time_us();
  const uint32_t now_ms = now > s_zero_us ? (uint32_t)((now - s_zero_us) / 1000u) : 0;
  while (s_event_next < s_event_count && s_events[s_event_next].at_ms <= now_ms) {
    fire(&s_events[s_event_next++]);
  }
}

/* The Multi-device is switched off (runtime.c): the chip loses its
 * state, pads and all, without a word to the firmware. The platform is
 * forgotten too, as its RAM would be, so the firmware sets it again and
 * calls uni_init(), which reads the script afresh and plays it from the
 * start, as bonded pads would come back after a reboot. */
void mdfw_runtime_bt_power_off(void) {
  reset_all();
  s_platform = NULL;
}
