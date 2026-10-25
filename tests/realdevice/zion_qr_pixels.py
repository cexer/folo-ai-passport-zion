"""Capture RGB565 code pixels: compare an issued Mini Program code or decode a URL QR."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
from zion_ui import ZionDevice

p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--decoder-path',required=True)
p.add_argument('--report',required=True)
p.add_argument('--expect-mini-code',action='store_true',help='Compare official proprietary Mini Program code pixels; does not claim WeChat decoding')
args=p.parse_args()
sys.path.insert(0,str(Path(args.decoder_path).resolve()))
from PIL import Image
if not args.expect_mini_code:
    import zxingcpp

report=Path(args.report)
d=ZionDevice(args.port)
rows={};shape=None
try:
    state=d.home();d.select_list(state['apps']);d.key('OK');d.wait(page=5,busy=0,qr=1)
    d.serial.write(b'ZION QR\n');d.serial.flush()
    end=time.monotonic()+40
    while time.monotonic()<end:
        line=d.serial.readline().decode('utf8','replace').strip()
        if 'ZION_QR_PIXELS error=' in line:
            raise AssertionError(line)
        match=re.search(r'ZION_QR_PIXELS width=(\d+) height=(\d+) stride=(\d+)',line)
        if match:
            shape=tuple(map(int,match.groups()))
        match=re.search(r'ZION_QR_ROW y=(\d+) data=([0-9a-f]+)',line)
        if match:
            y=int(match[1]);assert y not in rows,'Duplicate snapshot row'
            rows[y]=bytes.fromhex(match[2])
        if 'ZION_QR_END' in line:
            break
    assert shape,'Missing RGB565 snapshot header'
    w,h,stride=shape
    assert set(rows)==set(range(h)) and all(len(row)==stride for row in rows.values())
    raw=b''.join(rows[y] for y in range(h))
    report.with_suffix('.rgb565').write_bytes(raw)
    pixels=[]
    for y in range(h):
        for x in range(w):
            value=int.from_bytes(rows[y][2*x:2*x+2],'little')
            pixels.append((((value>>11)&31)*255//31,((value>>5)&63)*255//63,(value&31)*255//31))
    image=Image.new('RGB',(w,h));image.putdata(pixels)
    image.save(report.with_suffix('.png'))
    entry={}
    if args.expect_mini_code:
        asset=Path(__file__).resolve().parents[2]/'assets/images/folo-mini-code-208.i1'
        expected=asset.read_bytes()[8:]
        assert (w,h)==(208,208)
        expected_pixels=[(0,0,0) if expected[y*26+x//8]&(0x80>>(x%8)) else (255,255,255)
                         for y in range(h) for x in range(w)]
        assert pixels==expected_pixels,'Rendered pixels differ from the official issued code asset'
        entry={'entry_kind':'official-wechat-mini-code','render_matches_official':True,'wechat_scan':'NOT RUN'}
    else:
        decoded=zxingcpp.read_barcode(image)
        expected='https://ai-passport.folotoy.cn/plays/'
        assert decoded and decoded.text==expected,'Rendered QR did not decode to official community'
        entry={'entry_kind':'url-qr','decoded_url':decoded.text}
    assert pixels.count((0,0,0))>100 and pixels.count((255,255,255))>100
    d.key('OK');final=d.wait(page=0,busy=0)
    report.write_text(json.dumps({'passed':True,'format':'RGB565','width':w,'height':h,'stride':stride,
        'raw_sha256':hashlib.sha256(raw).hexdigest(),**entry,
        'black_pixels':pixels.count((0,0,0)),'white_pixels':pixels.count((255,255,255)),
        'final':final,'scope':'On-device LVGL RGB565 render snapshot; not physical LCD optical capture'},indent=2),encoding='utf8')
    print('Device-rendered RGB565 code verified: PASS ('+entry['entry_kind']+')')
finally:
    d.close()
