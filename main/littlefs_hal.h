#ifndef ESP_LITTLEFS_API_H__
#define ESP_LITTLEFS_API_H__

#include <stdbool.h>
#include "littlefs/lfs.h"

struct lfs_config *esp32_littlefs_hal (void);
bool esp32_littlefs_is_blank (const struct lfs_config *cfg);

#endif
