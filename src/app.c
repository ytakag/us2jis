#include "app.h"
#include "keymap.h"
#include "tusb.h"
#include "pico/stdlib.h"

#define MOD_LALT 0x04u
#define MOD_RALT 0x40u
#define MOD_ALT_MASK (MOD_LALT | MOD_RALT)

static key_report_t current_output;
static bool output_dirty;
static bool left_alt_pending;
static bool right_alt_pending;
static bool pulse_pending;
static bool pulse_in_flight;
static uint8_t pulse_usage;
static uint8_t previous_input_modifiers;
static absolute_time_t left_alt_started;
static absolute_time_t right_alt_started;
static app_config_t active_config;
static app_config_t pending_config;
static bool has_pending_config;
static bool input_all_up = true;

static bool has_non_alt_input(const key_report_t *report) {
  if ((report->modifiers & (uint8_t)~MOD_ALT_MASK) != 0) return true;
  for (size_t i = 0; i < KEY_COUNT; ++i) {
    if (report->keycode[i] != 0) return true;
  }
  return false;
}

static void queue_ime_pulse(uint8_t usage) {
  pulse_usage = usage;
  pulse_pending = true;
}

static uint8_t ime_usage_for_action(uint8_t action) {
  if (action == CONFIG_ACTION_IME_ON) return active_config.ime_on_usage;
  if (action == CONFIG_ACTION_IME_OFF) return active_config.ime_off_usage;
  return 0;
}

void app_init(const app_config_t *config) {
  current_output = (key_report_t){0};
  output_dirty = true;
  active_config = *config;
  has_pending_config = false;
}

void app_set_config(const app_config_t *config) {
  if (input_all_up) {
    active_config = *config;
    has_pending_config = false;
  } else {
    pending_config = *config;
    has_pending_config = true;
  }
}

void app_host_report(const key_report_t *report) {
  input_all_up = report->modifiers == 0 && !has_non_alt_input(report);
  if (has_pending_config && !has_non_alt_input(report) && report->modifiers == 0) {
    active_config = pending_config;
    has_pending_config = false;
  }
  bool left_down = (report->modifiers & MOD_LALT) != 0;
  bool right_down = (report->modifiers & MOD_RALT) != 0;
  bool left_was_down = (previous_input_modifiers & MOD_LALT) != 0;
  bool right_was_down = (previous_input_modifiers & MOD_RALT) != 0;
  bool any_other = has_non_alt_input(report);

  if (left_down && !left_was_down) {
    left_alt_pending = !any_other && !right_down && active_config.left_alt_action != CONFIG_ACTION_NONE;
    left_alt_started = get_absolute_time();
  }
  if (right_down && !right_was_down) {
    right_alt_pending = !any_other && !left_down && active_config.right_alt_action != CONFIG_ACTION_NONE;
    right_alt_started = get_absolute_time();
  }

  if (any_other) {
    left_alt_pending = false;
    right_alt_pending = false;
  }
  if (left_down && right_down) {
    left_alt_pending = false;
    right_alt_pending = false;
  }

  if (left_alt_pending && left_down && absolute_time_diff_us(left_alt_started, get_absolute_time()) >= (int64_t)active_config.timeout_ms * 1000) {
    left_alt_pending = false;
  }
  if (right_alt_pending && right_down && absolute_time_diff_us(right_alt_started, get_absolute_time()) >= (int64_t)active_config.timeout_ms * 1000) {
    right_alt_pending = false;
  }

  if (!left_down && left_alt_pending && !right_down && !any_other) {
    int64_t held_us = absolute_time_diff_us(left_alt_started, get_absolute_time());
    if (held_us < (int64_t)active_config.timeout_ms * 1000) {
      uint8_t usage = ime_usage_for_action(active_config.left_alt_action);
      if (usage != 0) queue_ime_pulse(usage);
    }
  }
  if (!right_down && right_alt_pending && !left_down && !any_other) {
    int64_t held_us = absolute_time_diff_us(right_alt_started, get_absolute_time());
    if (held_us < (int64_t)active_config.timeout_ms * 1000) {
      uint8_t usage = ime_usage_for_action(active_config.right_alt_action);
      if (usage != 0) queue_ime_pulse(usage);
    }
  }
  if (!left_down) left_alt_pending = false;
  if (!right_down) right_alt_pending = false;
  previous_input_modifiers = report->modifiers;

  keymap_apply(report, &current_output, &active_config);
  if (left_alt_pending) current_output.modifiers &= (uint8_t)~MOD_LALT;
  if (right_alt_pending) current_output.modifiers &= (uint8_t)~MOD_RALT;
  output_dirty = true;
}

void app_host_disconnected(void) {
  current_output = (key_report_t){0};
  left_alt_pending = false;
  right_alt_pending = false;
  pulse_pending = false;
  previous_input_modifiers = 0;
  input_all_up = true;
  output_dirty = true;
}

void app_task(void) {
  absolute_time_t now = get_absolute_time();
  if (left_alt_pending && (previous_input_modifiers & MOD_LALT) &&
      absolute_time_diff_us(left_alt_started, now) >= (int64_t)active_config.timeout_ms * 1000) {
    left_alt_pending = false;
    current_output.modifiers |= MOD_LALT;
    output_dirty = true;
  }
  if (right_alt_pending && (previous_input_modifiers & MOD_RALT) &&
      absolute_time_diff_us(right_alt_started, now) >= (int64_t)active_config.timeout_ms * 1000) {
    right_alt_pending = false;
    current_output.modifiers |= MOD_RALT;
    output_dirty = true;
  }
  if (!tud_hid_ready()) return;
  if (pulse_pending) {
    uint8_t keys[KEY_COUNT] = { pulse_usage, 0, 0, 0, 0, 0 };
    if (tud_hid_keyboard_report(0, current_output.modifiers, keys)) {
      pulse_pending = false;
      pulse_in_flight = true;
    }
    return;
  }
  if (output_dirty) {
    if (tud_hid_keyboard_report(0, current_output.modifiers, current_output.keycode)) {
      output_dirty = false;
    }
  }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
  (void)instance;
  (void)report;
  (void)len;
  if (pulse_in_flight) {
    pulse_in_flight = false;
    output_dirty = true;
  }
}
