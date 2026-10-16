/* Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for Bluepad32's parser/uni_hid_parser.h: the report
 * parser's interface, as a platform finds it in a device's
 * report_parser. Only set_player_leds, set_lightbar_color and
 * play_dual_rumble are filled in (they log); the parsing side is never
 * called, as the pads are scripted. */
#ifndef MDFW_SHIM_UNI_HID_PARSER_H
#define MDFW_SHIM_UNI_HID_PARSER_H
#include <stdint.h>

struct uni_hid_device_s;

struct hid_globals_s {
  int32_t logical_minimum;
  int32_t logical_maximum;
  uint16_t usage_page;
  uint8_t report_size;
  uint8_t report_count;
  uint8_t report_id;
};
typedef struct hid_globals_s hid_globals_t;

typedef void (*report_setup_fn_t)(struct uni_hid_device_s *d);
typedef void (*report_init_report_fn_t)(struct uni_hid_device_s *d);
typedef void (*report_parse_usage_fn_t)(struct uni_hid_device_s *d,
                                        const hid_globals_t *globals, uint16_t usage_page,
                                        uint16_t usage, int32_t value);
typedef void (*report_parse_input_report_fn_t)(struct uni_hid_device_s *d,
                                               const uint8_t *report, uint16_t report_len);
typedef void (*report_parse_feature_report_fn_t)(struct uni_hid_device_s *d,
                                                 const uint8_t *report, uint16_t report_len);
typedef void (*report_set_player_leds_fn_t)(struct uni_hid_device_s *d, uint8_t leds);
typedef void (*report_set_lightbar_color_fn_t)(struct uni_hid_device_s *d, uint8_t r,
                                               uint8_t g, uint8_t b);
/* start_delay_ms: 0 starts now. duration_ms: 0 stops. A pad with one
 * motor uses the larger of the two magnitudes. */
typedef void (*report_play_dual_rumble_fn_t)(struct uni_hid_device_s *d,
                                             uint16_t start_delay_ms, uint16_t duration_ms,
                                             uint8_t weak_magnitude, uint8_t strong_magnitude);
typedef void (*report_device_dump_t)(struct uni_hid_device_s *d);

typedef struct {
  report_setup_fn_t setup;
  report_init_report_fn_t init_report;
  report_parse_usage_fn_t parse_usage;
  report_parse_input_report_fn_t parse_input_report;
  report_parse_feature_report_fn_t parse_feature_report;
  report_set_player_leds_fn_t set_player_leds;
  report_set_lightbar_color_fn_t set_lightbar_color;
  report_play_dual_rumble_fn_t play_dual_rumble;
  report_device_dump_t device_dump;
} uni_report_parser_t;
#endif
