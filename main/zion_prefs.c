#include "zion_prefs.h"
#include "esp_flash.h"
#include "meta_carve_store.h"
#include <string.h>

/* Dedicated reserved store sector: never share child NVS or carve records. */
#define PREFS_ADDRESS (META_STORE_OFFSET + META_STORE_SCRATCH_SECTOR * META_STORE_SECTOR_SIZE)
static uint8_t s_sector[ZION_PREFS_SECTOR_SIZE];

esp_err_t zion_prefs_load(zion_prefs_t *prefs)
{
    memset(prefs, 0, sizeof(*prefs));
    esp_err_t err = esp_flash_read(NULL, s_sector, PREFS_ADDRESS, sizeof(s_sector));
    if (err != ESP_OK) return err;
    const int index = zion_prefs_latest(s_sector);
    if (index >= 0) memcpy(prefs, s_sector + index * sizeof(*prefs), sizeof(*prefs));
    return ESP_OK;
}

esp_err_t zion_prefs_save(zion_prefs_t *prefs)
{
    esp_err_t err = esp_flash_read(NULL, s_sector, PREFS_ADDRESS, sizeof(s_sector));
    if (err != ESP_OK) return err;
    const int latest = zion_prefs_latest(s_sector);
    zion_prefs_t old;
    memset(&old, 0, sizeof(old));
    if (latest >= 0) memcpy(&old, s_sector + latest * sizeof(old), sizeof(old));
    prefs->sequence = old.sequence + 1;
    zion_prefs_seal(prefs);
    /* Skip torn records. Only completely erased records are appendable. */
    unsigned next = 0;
    for (unsigned i = 0; i < sizeof(s_sector); ++i)
        if (s_sector[i] != 0xff) next = i / sizeof(*prefs) + 1;
    if (next >= sizeof(s_sector) / sizeof(*prefs)) {
        err = esp_flash_erase_region(NULL, PREFS_ADDRESS, sizeof(s_sector));
        if (err != ESP_OK) return err;
        next = 0;
    }
    const uint32_t address = PREFS_ADDRESS + next * sizeof(*prefs);
    err = esp_flash_write(NULL, prefs, address, sizeof(*prefs));
    if (err != ESP_OK) return err;
    zion_prefs_t verify;
    err = esp_flash_read(NULL, &verify, address, sizeof(verify));
    if (err != ESP_OK) return err;
    return memcmp(prefs, &verify, sizeof(verify)) == 0 && zion_prefs_valid(&verify)
               ? ESP_OK : ESP_FAIL;
}
