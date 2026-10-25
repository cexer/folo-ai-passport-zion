#pragma once
#include "esp_err.h"
#include "meta_carve.h"
#include "zion_policy.h"
int zion_default_find(const meta_carve_t *c,const zion_prefs_t *p);
esp_err_t zion_default_update(zion_prefs_t *p,int slot);
/* Removes the durable APP/owned DATA registry before erasing their bytes.
 * cleanup_pending is true if metadata removal succeeded but erasing failed. */
esp_err_t zion_delete(zion_prefs_t *p,int slot,bool *cleanup_pending);
