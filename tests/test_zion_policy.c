#include "zion_policy.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    for (int apps = 0; apps <= 8; ++apps) {
        const int count = apps + 2;
        int selected = 0;
        for (int i = 1; i <= count; ++i) {
            selected = zion_list_move(selected, apps, 1);
            assert(selected == i % count);
        }
        assert(zion_list_move(0, apps, -1) == count - 1);
        assert(zion_list_move(count - 1, apps, 1) == 0);
        assert(zion_list_move(apps, apps, 1) == apps + 1);
    }
    uint8_t sector[ZION_PREFS_SECTOR_SIZE];
    memset(sector, 0xff, sizeof(sector));
    assert(zion_prefs_latest(sector) == -1);
    zion_prefs_t p = {.sequence = 7, .play_id = 725, .offset = 0x360000, .flags = 1};
    zion_prefs_seal(&p);
    assert(zion_prefs_valid(&p));
    memcpy(sector, &p, sizeof(p));
    assert(zion_prefs_latest(sector) == 0);
    p.sequence++;
    zion_prefs_seal(&p);
    memcpy(sector + sizeof(p), &p, sizeof(p) - 3); /* interrupted write */
    assert(zion_prefs_latest(sector) == 0);
    memcpy(sector + 2 * sizeof(p), &p, sizeof(p));
    assert(zion_prefs_latest(sector) == 2);
    p.sha256[0] ^= 1;
    assert(!zion_prefs_valid(&p));
    p.flags = ZION_PREFS_SKIP_ONCE; zion_prefs_seal(&p);
    assert(zion_prefs_valid(&p));
    p.flags = 4; zion_prefs_seal(&p);
    assert(!zion_prefs_valid(&p));
    assert(zion_should_autostart(true, true, true, false, false));
    assert(!zion_should_autostart(false, true, true, false, false));
    assert(!zion_should_autostart(true, false, true, false, false));
    assert(!zion_should_autostart(true, true, false, false, false));
    assert(!zion_should_autostart(true, true, true, true, false));
    assert(!zion_should_autostart(true, true, true, false, true));
    puts("zion policy: PASS (torn write, fingerprint CRC, safe startup conditions)");
}
