/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File: bluepad32_test.c
 * Description: Checks EmuMD's Bluepad32 with a platform of its own: the
 *              pads script (and its errors), a pad's connected, ready
 *              and data callbacks in that order, slots and the index to
 *              instance round trip, state lines, the _safe calls waiting
 *              for a poll, a pad with no rumble, what the firmware asks
 *              of a pad reaching the log, a pad the platform does not
 *              want, one it refuses, and a power cycle.
 *
 *   bluepad32_test <empty scratch folder>
 */
#include <sys/stat.h>

#include "emumd_plugin.h"
#include "pico/stdlib.h"
#include "runtime.h"
#include "uni.h"

static int s_fails;
#define CHECK(c)                                                   \
  do {                                                             \
    if (!(c)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
      s_fails++;                                                   \
    }                                                              \
  } while (0)

/* ---- the firmware, and the log ---------------------------------------- */

const mdfw_app_t mdfw_app = {.name = "bt", .version = "v0"};

static char s_log[16384];
static void host_log(const char *line) {
  strncat(s_log, line, sizeof(s_log) - strlen(s_log) - 2);
  strcat(s_log, "\n");
}
static bool logged(const char *text) { return strstr(s_log, text) != NULL; }
static void clear_log(void) { s_log[0] = 0; }

/* ---- a platform that keeps a diary ------------------------------------ */

static char s_diary[1024]; /* "init complete connected(0) ready(0) data(0) ..." */
static uni_hid_device_t *s_dev[CONFIG_BLUEPAD32_MAX_DEVICES];
static uni_gamepad_t s_last[CONFIG_BLUEPAD32_MAX_DEVICES];
static int s_inits, s_datas;

static void note(const char *what, int idx) {
  char s[32];
  if (idx < 0) {
    snprintf(s, sizeof(s), "%s ", what);
  } else {
    snprintf(s, sizeof(s), "%s(%d) ", what, idx);
  }
  strncat(s_diary, s, sizeof(s_diary) - strlen(s_diary) - 1);
}

static void plat_init(int argc, const char **argv) {
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
  s_inits++;
  note("init", -1);
}

static void plat_on_init_complete(void) {
  note("complete", -1);
  uni_bt_start_scanning_and_autoconnect_unsafe();
}

/* As a platform that wants gamepads and not keyboards would. */
static uni_error_t plat_on_device_discovered(bd_addr_t addr, const char *name, uint16_t cod,
                                             uint8_t rssi) {
  ARG_UNUSED(addr);
  ARG_UNUSED(rssi);
  if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD)
    return UNI_ERROR_IGNORE_DEVICE;
  if (name && !strcmp(name, "Ignore me")) return UNI_ERROR_IGNORE_DEVICE;
  return UNI_ERROR_SUCCESS;
}

static void plat_on_device_connected(uni_hid_device_t *d) {
  const int idx = uni_hid_device_get_idx_for_instance(d);
  s_dev[idx] = d;
  note("connected", idx);
}

static void plat_on_device_disconnected(uni_hid_device_t *d) {
  note("disconnected", uni_hid_device_get_idx_for_instance(d));
}

static uni_error_t plat_on_device_ready(uni_hid_device_t *d) {
  const int idx = uni_hid_device_get_idx_for_instance(d);
  note("ready", idx);
  if (!strcmp(d->name, "Refuse me")) return UNI_ERROR_NO_SLOTS;
  if (d->report_parser.set_player_leds) d->report_parser.set_player_leds(d, (uint8_t)(1u << idx));
  return UNI_ERROR_SUCCESS;
}

static void plat_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
  const int idx = uni_hid_device_get_idx_for_instance(d);
  if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;
  s_last[idx] = ctl->gamepad;
  s_datas++;
  note("data", idx);
}

static const uni_property_t *plat_get_property(uni_property_idx_t idx) {
  ARG_UNUSED(idx);
  return NULL;
}

static void plat_on_oob_event(uni_platform_oob_event_t event, void *data) {
  ARG_UNUSED(event);
  ARG_UNUSED(data);
}

static struct uni_platform s_platform = {
    .name = "Test",
    .init = plat_init,
    .on_init_complete = plat_on_init_complete,
    .on_device_discovered = plat_on_device_discovered,
    .on_device_connected = plat_on_device_connected,
    .on_device_disconnected = plat_on_device_disconnected,
    .on_device_ready = plat_on_device_ready,
    .on_oob_event = plat_on_oob_event,
    .on_controller_data = plat_on_controller_data,
    .get_property = plat_get_property,
};

/* ---- the script --------------------------------------------------------- */

static const char SCRIPT[] =
    "# EmuMD bluepad32_test\n"
    "\n"
    "0 0 connect Test Pad One   # a name with spaces\n"
    "0 1 connect\n"
    "abc 0 connect\n"              /* 5: not a time */
    "100 9 connect\n"              /* 6: no such slot */
    "100 0 frobnicate\n"           /* 7: no such verb */
    "100 0 state foo=1\n"          /* 8: no such key */
    "100 0 state lx=600\n"         /* 9: out of range */
    "100 0 state buttons=0x10000\n" /* 10: out of range */
    "100 0 state buttons\n"        /* 11: no value */
    "100 0 disconnect now\n"       /* 12: trailing words */
    "100 0 state buttons=0x3 dpad=1 lx=-512 ly=511 lt=1023\n"
    "50 0 connect\n"               /* 14: time goes backwards */
    "100 0 state rx=100\n"
    "200 1 norumble\n"
    "300 0 disconnect\n"
    "300 0 state buttons=1\n"      /* fires on an empty slot */
    "400 2 connect Ignore me\n"
    "500 3 connect Refuse me\n"
    "600 3 connect Again\n";

/* ---- the emulator's side ------------------------------------------------ */

static const emumd_plugin_t *s_plugin;

/* Emulated time moves on (microseconds), then the chip gets its turn. */
static void poll_at(uint64_t us) {
  s_plugin->tick(us);
  mdfw_runtime_bt_poll();
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/tmp/emumd-bluepad32-test";
  mkdir(dir, 0777);
  char path[1024], options[1100];
  snprintf(path, sizeof(path), "%s/pads.txt", dir);
  FILE *f = fopen(path, "w");
  if (!f) {
    perror(path);
    return 1;
  }
  fputs(SCRIPT, f);
  fclose(f);
  snprintf(options, sizeof(options), "pads=%s", path);

  s_plugin = emumd_plugin_v1();
  const emumd_host_t host = {EMUMD_PLUGIN_ABI, dir, options, 0, host_log};
  CHECK(s_plugin->power_on(&host, 1000) == 0);

  /* Nothing before uni_init(), which wants a platform first. */
  mdfw_runtime_bt_poll();
  CHECK(uni_init(0, NULL) == -1);
  CHECK(logged("uni_init() before uni_platform_set_custom()"));
  uni_platform_set_custom(&s_platform);
  CHECK(uni_get_platform() == &s_platform);
  CHECK(uni_init(0, NULL) == 0);
  CHECK(s_inits == 1 && !strcmp(s_diary, "init "));
  CHECK(logged("Bluepad32: platform \"Test\""));

  /* The script's bad lines were reported, by number, and the rest kept. */
  CHECK(logged("pads.txt line 5: abc?"));
  CHECK(logged("pads.txt line 6: slot 9? (0 to 3)"));
  CHECK(logged("pads.txt line 7: frobnicate?"));
  CHECK(logged("pads.txt line 8: foo?"));
  CHECK(logged("pads.txt line 9: lx=600? (-512 to 511)"));
  CHECK(logged("pads.txt line 10: buttons=0x10000? (0 to 65535)"));
  CHECK(logged("pads.txt line 11: buttons? (key=value)"));
  CHECK(logged("pads.txt line 12: disconnect takes nothing after it"));
  CHECK(logged("pads.txt line 14: 50 ms is before line 13's 100 ms"));
  CHECK(logged("Bluepad32: 10 pad events from"));

  /* The first poll: init complete (which starts scanning), then the two
   * pads at 0 ms, each connected, ready and reporting itself at rest. */
  poll_at(1000);
  CHECK(!strcmp(s_diary, "init complete connected(0) ready(0) data(0) connected(1) ready(1) "
                         "data(1) "));
  CHECK(logged("Bluepad32: scanning") && uni_bt_is_scanning());
  CHECK(logged("Bluepad32: pad 0 connected \"Test Pad One\""));
  CHECK(logged("Bluepad32: pad 0 ready"));
  CHECK(logged("Bluepad32: pad 1 connected \"\""));
  CHECK(logged("Bluepad32: pad 0 leds=0x01") && logged("Bluepad32: pad 1 leds=0x02"));
  CHECK(s_dev[0] && !strcmp(s_dev[0]->name, "Test Pad One"));
  CHECK(s_dev[1] && s_dev[1]->name[0] == 0);
  CHECK(s_last[0].axis_x == 0 && s_last[0].buttons == 0 && s_last[0].brake == 0);
  CHECK(s_dev[0]->conn.connected && s_dev[0]->conn.state == UNI_BT_CONN_STATE_DEVICE_READY);
  CHECK(s_dev[0]->cod == (UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD));
  CHECK(uni_hid_device_is_gamepad(s_dev[0]) && !uni_hid_device_is_virtual_device(s_dev[0]));

  /* Slots: an index is a place in the table, both ways. */
  CHECK(uni_hid_device_get_idx_for_instance(s_dev[0]) == 0);
  CHECK(uni_hid_device_get_idx_for_instance(s_dev[1]) == 1);
  CHECK(uni_hid_device_get_instance_for_idx(1) == s_dev[1]);
  CHECK(uni_hid_device_get_instance_for_idx(3) != NULL);
  CHECK(uni_hid_device_get_instance_for_idx(4) == NULL);
  CHECK(uni_hid_device_get_instance_for_idx(-1) == NULL);
  static uni_hid_device_t elsewhere;
  CHECK(uni_hid_device_get_idx_for_instance(&elsewhere) == -1);
  CHECK(s_dev[1] - s_dev[0] == 1);

  /* Nothing is due at 50 ms. */
  s_diary[0] = 0;
  poll_at(51000);
  CHECK(s_diary[0] == 0);

  /* 100 ms: a state, then one that keeps the first's values. */
  poll_at(101000);
  CHECK(!strcmp(s_diary, "data(0) data(0) "));
  CHECK(s_last[0].buttons == 3 && s_last[0].dpad == DPAD_UP && s_last[0].axis_x == -512 &&
        s_last[0].axis_y == 511 && s_last[0].brake == 1023 && s_last[0].axis_rx == 100 &&
        s_last[0].throttle == 0);

  /* What the firmware asks of a pad goes to the log. */
  clear_log();
  CHECK(s_dev[0]->report_parser.play_dual_rumble != NULL);
  s_dev[0]->report_parser.play_dual_rumble(s_dev[0], 0, 250, 10, 20);
  CHECK(logged("bt: Bluepad32: pad 0 rumble weak=10 strong=20 for 250 ms"));
  s_dev[1]->report_parser.set_player_leds(s_dev[1], 0x0a);
  CHECK(logged("bt: Bluepad32: pad 1 leds=0x0a"));

  /* 200 ms: pad 1 loses its motors, and nothing else. */
  poll_at(201000);
  CHECK(s_dev[1]->report_parser.play_dual_rumble == NULL);
  CHECK(s_dev[1]->report_parser.set_player_leds != NULL);
  CHECK(s_dev[0]->report_parser.play_dual_rumble != NULL);

  /* The _safe calls wait for the next poll, and keep their order. */
  s_diary[0] = 0;
  clear_log();
  uni_bt_del_keys_safe();
  uni_bt_disconnect_device_safe(1);
  uni_bt_disconnect_device_safe(2); /* empty: nothing to do */
  uni_bt_stop_scanning_safe();
  uni_bt_start_scanning_and_autoconnect_safe();
  CHECK(s_log[0] == 0 && s_diary[0] == 0 && uni_bt_is_scanning());
  poll_at(202000);
  CHECK(!strcmp(s_diary, "disconnected(1) "));
  CHECK(logged("Bluepad32: bonds forgotten") && logged("Bluepad32: pad 1 disconnected"));
  CHECK(strstr(s_log, "bonds forgotten") < strstr(s_log, "pad 1 disconnected"));
  CHECK(strstr(s_log, "scanning stopped") < strstr(s_log, "Bluepad32: scanning\n"));
  CHECK(uni_bt_is_scanning());
  CHECK(!s_dev[1]->conn.connected && s_dev[1]->name[0] == 0);
  uni_bt_allow_incoming_connections(false);
  CHECK(!uni_bt_incoming_connections_is_allowed());

  /* 300 ms: the script drops pad 0, then a state for it has nowhere to go. */
  s_diary[0] = 0;
  clear_log();
  poll_at(301000);
  CHECK(!strcmp(s_diary, "disconnected(0) "));
  CHECK(logged("Bluepad32: pad 0 disconnected"));
  CHECK(logged("pads.txt line 18: pad 0 is not connected"));

  /* 400 ms: a pad the platform does not want is never connected. */
  s_diary[0] = 0;
  clear_log();
  poll_at(401000);
  CHECK(s_diary[0] == 0);
  CHECK(logged("Bluepad32: pad 2 \"Ignore me\" not wanted by the platform (5)"));
  CHECK(!uni_hid_device_get_instance_for_idx(2)->conn.connected);

  /* 500 ms: one the platform refuses when ready is dropped, with no
   * disconnect, and its slot is free for the next. */
  s_diary[0] = 0;
  clear_log();
  poll_at(501000);
  CHECK(!strcmp(s_diary, "connected(3) ready(3) "));
  CHECK(logged("Bluepad32: pad 3 refused by the platform (3)"));
  CHECK(!uni_hid_device_get_instance_for_idx(3)->conn.connected);
  s_diary[0] = 0;
  poll_at(601000);
  CHECK(!strcmp(s_diary, "connected(3) ready(3) data(3) "));
  CHECK(!strcmp(uni_hid_device_get_instance_for_idx(3)->name, "Again"));

  /* A power cycle: the chip forgets its pads and its platform, and
   * says nothing to the firmware. Started again, the script plays from
   * the beginning. */
  s_diary[0] = 0;
  clear_log();
  s_plugin->power_off();
  CHECK(s_diary[0] == 0);
  CHECK(uni_get_platform() == NULL);
  CHECK(!uni_hid_device_get_instance_for_idx(3)->conn.connected);
  CHECK(uni_hid_device_get_instance_for_idx(3)->name[0] == 0);
  mdfw_runtime_bt_poll();
  CHECK(s_diary[0] == 0);
  CHECK(s_plugin->power_on(&host, 10000000) == 0);
  uni_platform_set_custom(&s_platform);
  CHECK(uni_init(0, NULL) == 0);
  CHECK(s_inits == 2);
  poll_at(10001000);
  CHECK(!strcmp(s_diary, "init complete connected(0) ready(0) data(0) connected(1) ready(1) "
                         "data(1) "));
  CHECK(logged("Bluepad32: pad 0 connected \"Test Pad One\""));
  s_plugin->power_off();

  /* No script: a note, and no pads. */
  clear_log();
  const emumd_host_t bare = {EMUMD_PLUGIN_ABI, dir, "", 0, host_log};
  CHECK(s_plugin->power_on(&bare, 20000000) == 0);
  uni_platform_set_custom(&s_platform);
  CHECK(uni_init(0, NULL) == 0);
  CHECK(logged("Bluepad32: no pads (--md-option pads=FILE scripts some)"));
  s_diary[0] = 0;
  poll_at(20001000);
  CHECK(!strcmp(s_diary, "complete "));
  s_plugin->power_off();

  /* A script that cannot be read. */
  clear_log();
  const emumd_host_t missing = {EMUMD_PLUGIN_ABI, dir, "pads=/nowhere/pads.txt", 0, host_log};
  CHECK(s_plugin->power_on(&missing, 30000000) == 0);
  uni_platform_set_custom(&s_platform);
  CHECK(uni_init(0, NULL) == 0);
  CHECK(logged("Bluepad32: cannot read the pads script /nowhere/pads.txt"));
  s_plugin->power_off();

  printf("bluepad32_test: %s\n", s_fails ? "FAIL" : "OK");
  return s_fails ? 1 : 0;
}
