#pragma once
#include "zion_prefs.h"
#include "metapass_hook.h"

/* Optional integration for child firmware. Link zion_prefs.c and zion_policy.c
 * and their headers from this fork. Unknown third-party firmware needs no hook
 * for the universal power-on key escape. */
static inline esp_err_t zion_return_to_launcher(void)
{
    zion_prefs_t prefs;
    esp_err_t err = zion_prefs_load(&prefs);
    if (err != ESP_OK) return err;
    prefs.flags |= ZION_PREFS_SKIP_ONCE;
    err = zion_prefs_save(&prefs);
    if (err != ESP_OK) return err;
    metapass_return_to_launcher();
    return ESP_FAIL;
}
