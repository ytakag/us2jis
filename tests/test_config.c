#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

int main(int argc, char **argv) {
  app_config_t config, decoded;
  uint8_t wire[CONFIG_WIRE_SIZE];
  config_defaults(&config);
  assert(config_validate(&config));
  assert(config.timeout_ms == 200);
  assert(config.left_alt_action == CONFIG_ACTION_IME_OFF);
  assert(config.right_alt_action == CONFIG_ACTION_IME_ON);
  assert(config.keymap[0x1f].plain_key == 0x1f);
  assert(config.keymap[0x1f].shifted_key == 0x2f);
  assert(config.keymap[0x2e].plain_key == 0x2d);
  assert(config.keymap[0x2e].plain_shift == 1);
  assert(config.keymap[0x2e].shifted_key == 0x33);
  assert(config.keymap[0x2e].shifted_shift == 1);

  config_encode(&config, wire);
  assert(sizeof(wire) == 1032);
  assert(config_decode(wire, sizeof(wire), &decoded));
  assert(memcmp(&config, &decoded, sizeof(config)) == 0);

  static const uint8_t crc_check[] = "123456789";
  assert(config_crc32(crc_check, sizeof(crc_check) - 1) == 0xcbf43926u);
  if (argc == 2) {
    FILE *file = fopen(argv[1], "wb");
    assert(file != NULL);
    assert(fwrite(wire, 1, sizeof(wire), file) == sizeof(wire));
    assert(fclose(file) == 0);
  }
  wire[0] = 99;
  assert(!config_decode(wire, sizeof(wire), &decoded));
  wire[0] = CONFIG_WIRE_VERSION;
  wire[4] = 99;
  assert(!config_decode(wire, sizeof(wire), &decoded));
  puts("config tests passed");
  return 0;
}
