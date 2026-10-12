#ifndef US2JIS_KEYMAP_H
#define US2JIS_KEYMAP_H

#include "app.h"
#include "config.h"

void keymap_apply(const key_report_t *input, key_report_t *output,
                  const app_config_t *config);

#endif
