import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from zion_release import export_release


def fixture(path, version='0.4.4-zion', elf=b'fixture ELF', config=b'CONFIG_ZION_LAUNCHER=y\n'):
    path.mkdir()
    app = bytearray(336)
    app[0] = 0xe9
    app[48:48+len(version)] = version.encode()
    app[176:208] = hashlib.sha256(elf).digest()
    body = bytearray(b'\xff'*0x10000)
    body[:2] = b'\xe9\x01'
    body[0x8000:0x8002] = b'\xaa\x50'
    body.extend(app)
    hybrid = body+b'MPUPV2\0\0'+struct.pack('<I',len(body))+hashlib.sha256(body).digest()
    contents = {'app.bin':bytes(app),'app.elf':elf,'sdkconfig':config,
                'bootloader.bin':b'\xe9\x01','partitions.bin':b'\xaa\x50','zion-full.bin':hybrid}
    manifest = {'product':'Zion','test_control':False,'version':version,
                'display_name':'ZION','chip':'esp32c3','idf':'v5.5.3',
                'app_elf_sha256':hashlib.sha256(elf).hexdigest(), 'files':{}}
    for name, data in contents.items():
        (path/name).write_bytes(data)
        manifest['files'][name] = {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    (path/'manifest.json').write_text(json.dumps(manifest))
    return manifest


def rewrite_manifest(path, manifest):
    (path/'manifest.json').write_text(json.dumps(manifest))


class ReleaseTest(unittest.TestCase):
    def test_production_names_checksums_and_idempotence(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);package=root/'package';out=root/'release';fixture(package)
            result=export_release(package,out)
            self.assertEqual(result.read_bytes(),(package/'zion-full.bin').read_bytes())
            self.assertEqual(len(list(out.iterdir())),3)
            self.assertTrue(all('0.4.4-zion' in p.name for p in out.iterdir()))
            receipt=json.loads(result.with_suffix('.json').read_text())
            self.assertEqual(receipt['firmware']['sha256'],hashlib.sha256(result.read_bytes()).hexdigest())
            self.assertEqual(receipt['firmware']['initial_flash_offset'],'0x0')
            self.assertNotIn(str(root),result.with_suffix('.json').read_text())
            self.assertEqual(result.with_suffix('.sha256').read_text(),f"{receipt['firmware']['sha256']}  {result.name}\n")
            before=result.stat().st_mtime_ns
            self.assertEqual(export_release(package,out),result)
            self.assertEqual(before,result.stat().st_mtime_ns)
            second=root/'second';fixture(second,elf=b'different ELF')
            self.assertNotEqual(export_release(second,out),result)

    def test_test_builds_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);package=root/'package';manifest=fixture(package)
            manifest['test_control']=True;rewrite_manifest(package,manifest)
            with self.assertRaisesRegex(ValueError,'production'):
                export_release(package,root/'release')
            self.assertFalse((root/'release').exists())
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);package=root/'package';fixture(package,config=b'CONFIG_META_E2E_TEST_CONTROL=y\n')
            with self.assertRaisesRegex(ValueError,'Test-control'):
                export_release(package,root/'release')

    def test_tampered_package_and_hybrid(self):
        for mutate_footer in (False,True):
            with tempfile.TemporaryDirectory() as temp:
                root=Path(temp);package=root/'package';manifest=fixture(package)
                data=bytearray((package/'zion-full.bin').read_bytes());data[-1]^=1
                (package/'zion-full.bin').write_bytes(data)
                if mutate_footer:
                    manifest['files']['zion-full.bin']['sha256']=hashlib.sha256(data).hexdigest()
                    rewrite_manifest(package,manifest)
                with self.assertRaisesRegex(ValueError,'integrity|Hybrid'):
                    export_release(package,root/'release')

    def test_version_and_path_validation(self):
        for version in ('../escape','0.4.5-zion'):
            with tempfile.TemporaryDirectory() as temp:
                root=Path(temp);package=root/'package';manifest=fixture(package)
                manifest['version']=version;rewrite_manifest(package,manifest)
                with self.assertRaises(ValueError):
                    export_release(package,root/'release')
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);package=root/'package';manifest=fixture(package)
            manifest['files']['../escape']={'bytes':1,'sha256':'a'*64};rewrite_manifest(package,manifest)
            with self.assertRaisesRegex(ValueError,'filenames'):
                export_release(package,root/'release')

    def test_existing_release_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);package=root/'package';fixture(package)
            result=export_release(package,root/'release');result.write_bytes(b'other data')
            with self.assertRaisesRegex(ValueError,'different content'):
                export_release(package,root/'release')
            self.assertEqual(result.read_bytes(),b'other data')


if __name__=='__main__':
    unittest.main()
