#ifndef US2JIS_CONFIG_STORE_H
#define US2JIS_CONFIG_STORE_H

#include "config.h"

void config_store_init(app_config_t *config);
bool config_store_save(const app_config_t *config);

#endif
