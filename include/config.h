#ifndef US2JIS_CONFIG_H
#define US2JIS_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONFIG_WIRE_VERSION 2u
#define CONFIG_WIRE_SIZE (8u + 256u * 4u)
#define CONFIG_ACTION_NONE 0u
#define CONFIG_ACTION_IME_ON 1u
#define CONFIG_ACTION_IME_OFF 2u

typedef struct {
  uint8_t plain_key;
  uint8_t plain_shift;
  uint8_t shifted_key;
  uint8_t shifted_shift;
} keymap_entry_t;

typedef struct {
  uint16_t timeout_ms;
  uint8_t keymap_enabled;
  uint8_t left_alt_action;
  uint8_t right_alt_action;
  uint8_t ime_on_usage;
  uint8_t ime_off_usage;
  keymap_entry_t keymap[256];
} app_config_t;

void config_defaults(app_config_t *config);
bool config_decode(const uint8_t *data, size_t length, app_config_t *config);
void config_encode(const app_config_t *config, uint8_t data[CONFIG_WIRE_SIZE]);
uint32_t config_crc32(const uint8_t *data, size_t length);
bool config_validate(const app_config_t *config);

#endif
