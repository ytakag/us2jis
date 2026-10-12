#ifndef US2JIS_APP_H
#define US2JIS_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#define KEY_COUNT 6

typedef struct {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keycode[KEY_COUNT];
} key_report_t;

void app_init(const app_config_t *config);
void app_task(void);
void app_set_config(const app_config_t *config);
void app_host_report(const key_report_t *report);
void app_host_disconnected(void);
void app_led_state(uint8_t led_mask);

#endif
