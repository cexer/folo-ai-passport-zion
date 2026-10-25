"""Export only verified production firmware to versioned public release files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import tempfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def export_release(package, output):
    package, output = Path(package), Path(output)
    manifest = json.loads((package / 'manifest.json').read_text(encoding='utf8'))
    if manifest.get('product') != 'Zion' or manifest.get('test_control') is not False:
        raise ValueError('Only a production Zion package may enter release/')
    version = manifest['version']
    if not isinstance(version, str) or not re.fullmatch(r'[0-9][0-9A-Za-z._-]{0,63}', version):
        raise ValueError('Unsafe or missing firmware version')
    contents = {}
    for name, record in manifest['files'].items():
        if Path(name).name != name or '/' in name or '\\' in name:
            raise ValueError('Package manifest must contain plain filenames')
        data = (package / name).read_bytes()
        if len(data) != record['bytes'] or digest(data) != record['sha256']:
            raise ValueError(f'Package integrity mismatch: {name}')
        contents[name] = data
    app, firmware = contents['app.bin'], contents['zion-full.bin']
    if len(firmware) < 44 or firmware[-44:-36] != b'MPUPV2\0\0':
        raise ValueError('Firmware is not the bootable MPUPV2 hybrid')
    body = firmware[:-44]
    if struct.unpack_from('<I', firmware, len(firmware)-36)[0] != len(body) or hashlib.sha256(body).digest() != firmware[-32:]:
        raise ValueError('Hybrid length/hash mismatch')
    if len(body) > 0x210000 or not body or body[0] != 0xe9 or body[0x10000:0x10000+len(app)] != app:
        raise ValueError('Hybrid contains a different application/layout')
    for name, offset in [('bootloader.bin', 0), ('partitions.bin', 0x8000)]:
        if body[offset:offset+len(contents[name])] != contents[name]:
            raise ValueError(f'Hybrid contains a different {name}')
    if app[48:80].split(b'\0', 1)[0].decode('utf8') != version:
        raise ValueError('Application descriptor version differs from manifest')
    if digest(contents['app.elf']) != manifest['app_elf_sha256'] or app[176:208].hex() != manifest['app_elf_sha256']:
        raise ValueError('Application/ELF identity mismatch')
    if b'ZION KEY UP' in app or b'CONFIG_META_E2E_TEST_CONTROL=y' in contents['sdkconfig']:
        raise ValueError('Test-control build is not publishable')
    checksum = digest(firmware)
    stem = f'ZION-{version}-{checksum[:12]}'
    name = stem + '.bin'
    public = {
        'format': 'ZION-release-v1', 'product': manifest['product'],
        'display_name': manifest['display_name'], 'version': version,
        'chip': manifest['chip'], 'idf': manifest['idf'], 'test_control': False,
        'app_elf_sha256': manifest['app_elf_sha256'],
        'firmware': {'name': name, 'bytes': len(firmware), 'sha256': checksum,
                     'format': 'bootable-body+MPUPV2', 'initial_flash_offset': '0x0'},
    }
    files = {name: firmware, stem+'.sha256': f'{checksum}  {name}\n'.encode('ascii'),
             stem+'.json': (json.dumps(public, ensure_ascii=False, indent=2)+'\n').encode('utf8')}
    output.mkdir(parents=True, exist_ok=True)
    for filename, data in files.items():
        target = output / filename
        if target.exists() and target.read_bytes() != data:
            raise ValueError(f'Release filename already has different content: {filename}')
    for filename, data in files.items():
        target = output / filename
        if target.exists():
            continue
        temp_path = None
        try:
            with tempfile.NamedTemporaryFile(dir=output, prefix=stem+'-', suffix='.tmp', delete=False) as stream:
                temp_path = Path(stream.name)
                stream.write(data)
            os.replace(temp_path, target)
        finally:
            if temp_path is not None:
                temp_path.unlink(missing_ok=True)
    return output / name


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', required=True)
    p.add_argument('--output', default=str(Path(__file__).resolve().parents[1]/'release'))
    args = p.parse_args()
    print('Zion release:', export_release(args.package, args.output))
