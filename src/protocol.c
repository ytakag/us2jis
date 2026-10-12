#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "config_store.h"
#include "pico/stdlib.h"
#include "protocol.h"
#include "tusb.h"

#define COMMAND_BYTES 48u
#define TRANSFER_TIMEOUT_MS 10000u

typedef enum { RX_COMMAND, RX_PAYLOAD } rx_state_t;
static app_config_t *live_config;
static rx_state_t rx_state;
static char command[COMMAND_BYTES];
static size_t command_length;
static uint8_t incoming[CONFIG_WIRE_SIZE];
static size_t incoming_length;
static uint32_t expected_crc;
static absolute_time_t transfer_started;
static uint8_t outgoing[CONFIG_WIRE_SIZE];
static size_t outgoing_length;
static size_t outgoing_offset;

static void reply(const char *message) {
  (void)tud_cdc_write_str(message);
}

static void begin_get(void) {
  config_encode(live_config, outgoing);
  outgoing_length = sizeof(outgoing);
  outgoing_offset = 0;
  char header[64];
  (void)snprintf(header, sizeof(header), "DATA %u %u %08lx\n",
                 CONFIG_WIRE_VERSION, (unsigned)outgoing_length,
                 (unsigned long)config_crc32(outgoing, outgoing_length));
  reply(header);
}

static bool parse_set(const char *line) {
  unsigned version = 0, length = 0;
  unsigned long crc = 0;
  int consumed = 0;
  if (sscanf(line, "SET %u %u %lx%n", &version, &length, &crc, &consumed) != 3 ||
      line[consumed] != '\0') {
    reply("ERR syntax\n");
    return false;
  }
  if (version != CONFIG_WIRE_VERSION || length != CONFIG_WIRE_SIZE) {
    reply("ERR version_or_size\n");
    return false;
  }
  expected_crc = (uint32_t)crc;
  incoming_length = 0;
  rx_state = RX_PAYLOAD;
  transfer_started = get_absolute_time();
  return true;
}

static void finish_set(void) {
  if (config_crc32(incoming, sizeof(incoming)) != expected_crc) {
    reply("ERR checksum\n");
    rx_state = RX_COMMAND;
    return;
  }
  app_config_t candidate;
  if (!config_decode(incoming, sizeof(incoming), &candidate)) {
    reply("ERR invalid_config\n");
    rx_state = RX_COMMAND;
    return;
  }
  if (!config_store_save(&candidate)) {
    reply("ERR storage\n");
    rx_state = RX_COMMAND;
    return;
  }
  *live_config = candidate;
  app_set_config(live_config);
  reply("OK\n");
  rx_state = RX_COMMAND;
}

static void command_done(void) {
  command[command_length] = '\0';
  if (strcmp(command, "GET") == 0) {
    begin_get();
  } else if (strcmp(command, "INFO") == 0) {
    char response[48];
    (void)snprintf(response, sizeof(response), "US2JIS %u %u\n",
                   CONFIG_WIRE_VERSION, CONFIG_WIRE_SIZE);
    reply(response);
  } else if (strncmp(command, "SET ", 4) == 0) {
    (void)parse_set(command);
  } else if (strcmp(command, "RESET") == 0) {
    app_config_t defaults;
    config_defaults(&defaults);
    if (!config_store_save(&defaults)) {
      reply("ERR storage\n");
    } else {
      *live_config = defaults;
      app_set_config(live_config);
      reply("OK\n");
    }
  } else {
    reply("ERR command\n");
  }
}

void protocol_init(app_config_t *config) {
  live_config = config;
  rx_state = RX_COMMAND;
  command_length = 0;
  outgoing_length = 0;
}

void protocol_task(void) {
  if (outgoing_offset < outgoing_length) {
    uint32_t available = tud_cdc_write_available();
    if (available > 0) {
      uint32_t count = (uint32_t)(outgoing_length - outgoing_offset);
      if (count > available) count = available;
      uint32_t written = tud_cdc_write(outgoing + outgoing_offset, count);
      outgoing_offset += written;
    }
  }

  if (rx_state == RX_PAYLOAD &&
      absolute_time_diff_us(transfer_started, get_absolute_time()) >
          (int64_t)TRANSFER_TIMEOUT_MS * 1000) {
    rx_state = RX_COMMAND;
    incoming_length = 0;
    reply("ERR timeout\n");
  }

  uint8_t byte;
  while (tud_cdc_available() && tud_cdc_read(&byte, 1) == 1) {
    if (rx_state == RX_PAYLOAD) {
      incoming[incoming_length++] = byte;
      if (incoming_length == sizeof(incoming)) finish_set();
      continue;
    }
    if (byte == '\r') continue;
    if (byte == '\n') {
      command_done();
      command_length = 0;
      continue;
    }
    if (command_length + 1 >= sizeof(command)) {
      command_length = 0;
      reply("ERR command_too_long\n");
      continue;
    }
    command[command_length++] = (char)byte;
  }
}
