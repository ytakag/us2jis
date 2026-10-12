#include <assert.h>
#include <stdio.h>
#include "config.h"
#include "keymap.h"

int main(void) {
  app_config_t config;
  config_defaults(&config);
  config.keymap[0x2e] = (keymap_entry_t){0x2d, 1, 0x33, 1};

  key_report_t input = {0};
  key_report_t output = {0};
  input.keycode[0] = 0x2e;
  keymap_apply(&input, &output, &config);
  assert(output.keycode[0] == 0x2d);
  assert((output.modifiers & 0x22u) == 0x02u);

  input.modifiers = 0x20u; /* Right Shift */
  keymap_apply(&input, &output, &config);
  assert(output.keycode[0] == 0x33);
  assert((output.modifiers & 0x22u) == 0x20u);

  input.keycode[0] = 0x2e;
  input.keycode[1] = 0x04; /* Conflicting output Shift requirements: keep input state. */
  input.modifiers = 0;
  keymap_apply(&input, &output, &config);
  assert((output.modifiers & 0x22u) == 0);

  puts("keymap tests passed");
  return 0;
}
