#include <string.h>
#include "config.h"
#include "default_config.h"

void config_defaults(app_config_t *config) {
  (void)config_decode(config_default_wire, sizeof(config_default_wire), config);
}

bool config_validate(const app_config_t *config) {
  if (config->timeout_ms < 50 || config->timeout_ms > 2000) return false;
  if (config->keymap_enabled > 1) return false;
  if (config->left_alt_action > CONFIG_ACTION_IME_OFF ||
      config->right_alt_action > CONFIG_ACTION_IME_OFF) return false;
  if (config->ime_on_usage < 4 || config->ime_off_usage < 4 ||
      (config->ime_on_usage >= 0xe0 && config->ime_on_usage <= 0xe7) ||
      (config->ime_off_usage >= 0xe0 && config->ime_off_usage <= 0xe7)) return false;
  for (unsigned i = 0; i < 256; ++i) {
    const keymap_entry_t *entry = &config->keymap[i];
    if (entry->plain_shift > 1 || entry->shifted_shift > 1) return false;
    if ((entry->plain_key > 0 && entry->plain_key < 4) ||
        (entry->shifted_key > 0 && entry->shifted_key < 4) ||
        (entry->plain_key >= 0xe0 && entry->plain_key <= 0xe7) ||
        (entry->shifted_key >= 0xe0 && entry->shifted_key <= 0xe7)) return false;
  }
  return true;
}

bool config_decode(const uint8_t *data, size_t length, app_config_t *config) {
  if (length != CONFIG_WIRE_SIZE || data[0] != CONFIG_WIRE_VERSION) return false;
  uint16_t timeout_ms = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
  if (timeout_ms < 50 || timeout_ms > 2000 || data[1] > 1 ||
      data[4] > CONFIG_ACTION_IME_OFF || data[5] > CONFIG_ACTION_IME_OFF) return false;
  if (data[6] < 4 || data[7] < 4 ||
      (data[6] >= 0xe0 && data[6] <= 0xe7) ||
      (data[7] >= 0xe0 && data[7] <= 0xe7)) return false;
  for (unsigned i = 0; i < 256; ++i) {
    const uint8_t *src = &data[8 + 4 * i];
    if (src[1] > 1 || src[3] > 1 ||
        (src[0] > 0 && src[0] < 4) || (src[2] > 0 && src[2] < 4) ||
        (src[0] >= 0xe0 && src[0] <= 0xe7) ||
        (src[2] >= 0xe0 && src[2] <= 0xe7)) return false;
  }
  memset(config, 0, sizeof(*config));
  config->keymap_enabled = data[1];
  config->timeout_ms = timeout_ms;
  config->left_alt_action = data[4];
  config->right_alt_action = data[5];
  config->ime_on_usage = data[6];
  config->ime_off_usage = data[7];
  for (unsigned i = 0; i < 256; ++i) {
    const uint8_t *src = &data[8 + 4 * i];
    config->keymap[i] = (keymap_entry_t){src[0], src[1], src[2], src[3]};
  }
  return true;
}

void config_encode(const app_config_t *config, uint8_t data[CONFIG_WIRE_SIZE]) {
  data[0] = CONFIG_WIRE_VERSION;
  data[1] = config->keymap_enabled;
  data[2] = (uint8_t)(config->timeout_ms & 0xff);
  data[3] = (uint8_t)(config->timeout_ms >> 8);
  data[4] = config->left_alt_action;
  data[5] = config->right_alt_action;
  data[6] = config->ime_on_usage;
  data[7] = config->ime_off_usage;
  for (unsigned i = 0; i < 256; ++i) {
    const keymap_entry_t *entry = &config->keymap[i];
    uint8_t *dst = &data[8 + 4 * i];
    dst[0] = entry->plain_key;
    dst[1] = entry->plain_shift;
    dst[2] = entry->shifted_key;
    dst[3] = entry->shifted_shift;
  }
}

uint32_t config_crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
  }
  return ~crc;
}
