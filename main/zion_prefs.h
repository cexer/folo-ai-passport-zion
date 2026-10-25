#pragma once
#include "esp_err.h"
#include "zion_policy.h"
esp_err_t zion_prefs_load(zion_prefs_t *prefs);
esp_err_t zion_prefs_save(zion_prefs_t *prefs);
