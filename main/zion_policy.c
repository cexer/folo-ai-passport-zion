#include "zion_policy.h"

int zion_list_move(int selected, int app_count, int direction)
{
    const int count = app_count + 2;
    return (selected + count + (direction < 0 ? -1 : 1)) % count;
}
#include <stddef.h>
#include <string.h>

uint32_t zion_crc32(const void *data, unsigned size)
{
    const uint8_t *p = data;
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}

bool zion_prefs_valid(const zion_prefs_t *p)
{
    return p && p->magic == ZION_PREFS_MAGIC &&
           (p->flags & ~(ZION_PREFS_ENABLED | ZION_PREFS_SKIP_ONCE)) == 0 &&
           p->crc == zion_crc32(p, offsetof(zion_prefs_t, crc));
}

void zion_prefs_seal(zion_prefs_t *p)
{
    p->magic = ZION_PREFS_MAGIC;
    p->crc = zion_crc32(p, offsetof(zion_prefs_t, crc));
}

int zion_prefs_latest(const uint8_t sector[ZION_PREFS_SECTOR_SIZE])
{
    int best = -1;
    uint32_t sequence = 0;
    for (unsigned i = 0; i < ZION_PREFS_SECTOR_SIZE / sizeof(zion_prefs_t); ++i) {
        zion_prefs_t entry;
        memcpy(&entry, sector + i * sizeof(entry), sizeof(entry));
        if (zion_prefs_valid(&entry) && (best < 0 || (int32_t)(entry.sequence - sequence) > 0)) {
            best = (int)i;
            sequence = entry.sequence;
        }
    }
    return best;
}

bool zion_should_autostart(bool configured, bool target_valid, bool cold_start,
                           bool key_pressed, bool install_pending)
{
    return configured && target_valid && cold_start && !key_pressed && !install_pending;
}
