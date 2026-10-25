import hashlib
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from zion_product import NAME, APP_PROJECT_NAME, brand_app, brand_merged, name_window

class ProductTest(unittest.TestCase):
    def test_identity_limits(self):
        self.assertEqual(len(NAME.encode()),32)
        self.assertEqual(len(APP_PROJECT_NAME.encode()),31)
        self.assertEqual(NAME,'ZION 多玩法小程序管理器')

    def test_mnam_and_unchanged_executable(self):
        raw=bytearray(336);raw[0]=0xe9;raw[1]=1;raw[12]=5;raw[23]=1
        struct.pack_into('<II',raw,24,0x3c000020,256)
        struct.pack_into('<I',raw,44,0xffffffff) # reject the legacy extended-header interpretation
        label=APP_PROJECT_NAME.encode();raw[80:112]=label+b'\0'
        raw[304:]=hashlib.sha256(raw[:304]).digest()
        branded=brand_app(bytes(raw))
        self.assertEqual(branded[:336],raw)
        self.assertEqual(len(branded),8192)
        self.assertEqual(brand_app(branded),branded)
        window=branded[-40:];start=window.index(b'MNAM');length=window[start+4]
        self.assertEqual(window[start+5:start+5+length].decode(),NAME)
        checksum=0
        for value in window[start+4:-1]:checksum^=value
        self.assertEqual(checksum,window[-1])
        merged=bytearray(b'\xff'*0x800000);merged[0x10000:0x10000+len(raw)]=raw
        named,full=brand_merged(bytes(raw),bytes(merged))
        self.assertEqual(named,branded)
        self.assertEqual(full[0x10000:0x10000+len(named)],named)
        self.assertEqual(full[:0x10000],merged[:0x10000])
        self.assertEqual(full[0x210000:],merged[0x210000:])
        with self.assertRaises(ValueError):brand_merged(bytes(raw),b'\xff'*0x800000)

    def test_invalid_names(self):
        for name in ['', 'x'*33, 'x\n']:
            with self.assertRaises(ValueError):name_window(name)
        with self.assertRaises(ValueError):brand_app(b'\0'*336)

if __name__=='__main__':unittest.main()
