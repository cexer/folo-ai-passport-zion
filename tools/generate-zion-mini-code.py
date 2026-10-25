#!/usr/bin/env python3
"""Encode the official, service-issued WeChat Mini Program code as LVGL I1.

This is a format conversion of the published code, not a generated URL QR.
Run using the optional requirements-zion-pixels.txt environment.
"""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
args=p.parse_args()
root=Path(__file__).resolve().parents[1]
out=root/'assets/images'
out.mkdir(parents=True,exist_ok=True)
size=208
gray=Image.open(args.source).convert('L').resize((size,size),Image.Resampling.LANCZOS)
binary=gray.point(lambda v:255 if v>=128 else 0).convert('1',dither=Image.Dither.NONE)
# PIL mode 1: white=1; LVGL palette below: white=0 and black=1.
payload=bytes(v^255 for v in binary.tobytes())
data=b'\xff\xff\xff\xff\x00\x00\x00\xff'+payload
raw=out/'folo-mini-code-208.i1'
raw.write_bytes(data)
source=out/'folo_mini_code_208.c'
lines=['/* Official FoloToy Mini Program code. See folo-mini-code-manifest.json. */',
       '#include "lvgl.h"','_Alignas(4) static const uint8_t pixels[] = {']
for at in range(0,len(data),16):
    lines.append('    '+','.join(f'0x{v:02x}' for v in data[at:at+16])+',')
lines+=['};','const lv_image_dsc_t zion_official_mini_code = {',
        '    .header = {.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=208,.h=208,.stride=26},',
        '    .data_size=sizeof(pixels),.data=pixels,','};','']
source.write_text('\n'.join(lines),encoding='utf8',newline='\n')
binary.save(out/'folo-mini-code-208.png')
(out/'folo-mini-code-manifest.json').write_text(json.dumps({
    'source_url':'https://ai-passport.folotoy.cn/assets/guides/wireless-install/mini-program-code.webp',
    'source_sha256':hashlib.sha256(args.source.read_bytes()).hexdigest(),
    'kind':'WeChat Mini Program code issued by official FoloToy website; not a standard URL QR',
    'width':size,'height':size,'stride':26,'format':'I1','palette':'0=opaque white; 1=opaque black',
    'conversion':'Pillow L grayscale; LANCZOS 208x208; threshold 128; no dithering; invert packed bits',
    'data_bytes':len(data),'data_sha256':hashlib.sha256(data).hexdigest(),
    'c_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'c_sha256_newlines':'LF',
    'rights':'Official FoloToy published asset; source does not state a separate redistribution license',
},indent=2),encoding='utf8')
print('Official mini-code I1 asset:',len(data),'bytes')
