#include "keymap.h"
#include "config.h"

void keymap_apply(const key_report_t *input, key_report_t *output,
                  const app_config_t *config) {
  *output = *input;
  if (!config->keymap_enabled) return;
  bool in_shift = (input->modifiers & 0x22u) != 0;
  bool out_shift = in_shift;
  bool shift_state_set = false;
  bool shift_conflict = false;
  bool any_key = false;
  for (size_t i = 0; i < KEY_COUNT; ++i) {
    if (input->keycode[i] == 0) continue;
    if (input->keycode[i] <= 3) continue; /* HID rollover/error usages */
    const keymap_entry_t *m = &config->keymap[input->keycode[i]];
    output->keycode[i] = in_shift ? m->shifted_key : m->plain_key;
    bool key_shift = in_shift ? m->shifted_shift != 0 : m->plain_shift != 0;
    if (!shift_state_set) {
      out_shift = key_shift;
      shift_state_set = true;
    } else if (out_shift != key_shift) {
      shift_conflict = true;
    }
    any_key = true;
  }
  if (any_key && !shift_conflict) {
    if (out_shift && !in_shift) output->modifiers |= 0x02u;
    else if (!out_shift) output->modifiers &= (uint8_t)~0x22u;
  }
}
