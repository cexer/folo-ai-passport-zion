"""Product identity and the right-aligned MNAM display-name record."""
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent/'signing'))
from locate_app_image import esp_image_len, tail_offset

PRODUCT=json.loads((Path(__file__).resolve().parents[1]/'zion-product.json').read_text(encoding='utf8'))
NAME=PRODUCT['name']
APP_PROJECT_NAME=PRODUCT['app_project_name']
assert 1<=len(NAME.encode('utf8'))<=32
assert len(APP_PROJECT_NAME.encode('utf8'))<=31

def name_window(name=NAME):
    encoded=name.encode('utf8')
    if not 1<=len(encoded)<=32 or any(ord(c)<32 for c in name):raise ValueError('Invalid MNAM name')
    payload=bytes([len(encoded)])+encoded
    checksum=0
    for value in payload:checksum^=value
    blob=b'MNAM'+payload+bytes([checksum])
    return b'\xff'*(40-len(blob))+blob

def brand_app(app):
    """Preserve the executable and its appended hash; add one metadata sector."""
    descriptor=app[80:112].split(b'\0',1)[0].decode('utf8')
    if descriptor!=APP_PROJECT_NAME:raise ValueError('ESP descriptor brand mismatch')
    image_len=esp_image_len(app,0)
    at=tail_offset(image_len)
    if at+4096>0x200000:raise ValueError('APP plus name metadata exceeds factory')
    result=bytearray(app[:image_len]);result.extend(b'\xff'*(at+4096-len(result)))
    result[-40:]=name_window()
    return bytes(result)
