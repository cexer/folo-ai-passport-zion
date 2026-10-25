"""Integrity/geometry of the official issued Mini Program code asset.

This proves the embedded pattern, not recognition by the proprietary WeChat app.
"""
import hashlib
import json
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
assets=root/'assets/images'
meta=json.loads((assets/'folo-mini-code-manifest.json').read_text())
raw=(assets/'folo-mini-code-208.i1').read_bytes()
code=(assets/'folo_mini_code_208.c').read_text()
sha=lambda data:hashlib.sha256(data).hexdigest()
assert meta['source_url']=='https://ai-passport.folotoy.cn/assets/guides/wireless-install/mini-program-code.webp'
assert sha((assets/'folo-mini-code-original.webp').read_bytes())==meta['source_sha256']
assert (meta['width'],meta['height'],meta['stride'])==(208,208,26)
assert raw[:8]==b'\xff\xff\xff\xff\x00\x00\x00\xff'
assert len(raw)==8+208*26==meta['data_bytes']
assert sha(raw)==meta['data_sha256']
assert sha(code.encode())==meta['c_sha256']
embedded=bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]{2})',code.split('};',1)[0]))
assert embedded==raw
black=sum(v.bit_count() for v in raw[8:])
assert 1000<black<208*208//2
print('Official Mini Program code: PASS (issued source hash, 208x208 I1 palette/stride, byte-identical C data; WeChat scan NOT RUN)')
