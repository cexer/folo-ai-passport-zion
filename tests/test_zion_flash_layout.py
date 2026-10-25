"""Reject same-factory layouts with moved identity/settings before flashing."""
import hashlib
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from verify_firmware import compatible_zion_layout


def table(entries):
    raw = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, sub, off, size, label.encode(), 0)
                   for label, kind, sub, off, size in entries)
    raw += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(raw).digest()
    return raw + b'\xff' * (0xc00 - len(raw))


fixed = [('factory', 0, 0, 0x10000, 0x200000), ('nvs', 1, 2, 0x9000, 0x6000),
         ('phy_init', 1, 1, 0xf000, 0x1000), ('cardid', 1, 2, 0x356000, 0x4000),
         ('store', 1, 2, 0x35a000, 0x6000), ('otadata', 1, 0, 0x7fe000, 0x2000)]
target = table(fixed)
assert compatible_zion_layout(target, target)
carved = fixed + [('ota_0', 0, 0x10, 0x360000, 0x24a000), ('assets', 1, 0x82, 0x5b0000, 0x200000)]
assert compatible_zion_layout(table(carved), target)
for index in (0, 1, 3, 4, 5):
    changed = fixed.copy()
    label, kind, sub, off, size = changed[index]
    changed[index] = label, kind, sub, off + 0x1000, size
    assert not compatible_zion_layout(table(changed), target)
assert not compatible_zion_layout(table(fixed[:-1]), target)
assert not compatible_zion_layout(table(fixed + [fixed[0]]), target)
bad = bytearray(target); bad[16] ^= 1
assert not compatible_zion_layout(bytes(bad), target)
assert not compatible_zion_layout(b'\xff' * 0xc00, target)
print('Zion flash layout: PASS (carved views allowed; moved fixed regions, duplicates and bad MD5 rejected)')
