#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ZION_PREFS_MAGIC 0x314E4F5Au
#define ZION_PREFS_ENABLED 1u
#define ZION_PREFS_SKIP_ONCE 2u
#define ZION_PREFS_RECORD_SIZE 64u
#define ZION_PREFS_SECTOR_SIZE 4096u

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t play_id;
    uint32_t offset;
    uint8_t sha256[32];
    uint32_t flags;
    uint32_t reserved[2];
    uint32_t crc;
} zion_prefs_t;
_Static_assert(sizeof(zion_prefs_t) == ZION_PREFS_RECORD_SIZE, "prefs record layout");

uint32_t zion_crc32(const void *data, unsigned size);
bool zion_prefs_valid(const zion_prefs_t *prefs);
void zion_prefs_seal(zion_prefs_t *prefs);
int zion_prefs_latest(const uint8_t sector[ZION_PREFS_SECTOR_SIZE]);
bool zion_should_autostart(bool configured, bool target_valid, bool cold_start,
                           bool key_pressed, bool install_pending);
/* App items followed by Install and Settings, including an empty app list. */
int zion_list_move(int selected, int app_count, int direction);
