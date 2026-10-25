"""Observe installation page snapshots during a real BLE installation replay.

Read-only USB queries work in production; this does not assert physical pixels.
The caller flashes the selected authorized artifact separately.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time
import serial

p = argparse.ArgumentParser()
p.add_argument('--port', required=True)
p.add_argument('--play', required=True, type=int)
p.add_argument('--identity-file', required=True)
p.add_argument('--credentials-file', required=True)
p.add_argument('--report', required=True)
p.add_argument('--bleak-path')
p.add_argument('--corrupt-sha', action='store_true')
p.add_argument('--timeout', type=int, default=400)
args = p.parse_args()
result_path = Path(args.report).resolve()
child_path = result_path.with_suffix('.ble.json')
cmd = [sys.executable, str(Path(__file__).with_name('zion_install.py')), '--play', str(args.play),
       '--identity-file', args.identity_file, '--credentials-file', args.credentials_file,
       '--report', str(child_path), '--timeout', str(args.timeout)]
if args.bleak_path:
    cmd += ['--bleak-path', args.bleak_path]
if args.corrupt_sha:
    cmd += ['--corrupt-sha']
frames, pages, boot = [], [], []
with serial.Serial(args.port, 115200, timeout=.05) as usb:
    child = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf8')
    start = last_query = time.monotonic()
    exit_at = None
    while time.monotonic() - start < args.timeout + 50:
        now = time.monotonic()
        if now - last_query > .35:
            usb.write(b'ZION STATE\n');last_query = now
        line = usb.readline().decode('utf8', 'replace').strip()
        match = re.search(r'ZION_INSTALL_UI id=(\d+) stage=(\d+) percent=(\d+) name=(.*?) package=(\d+) installed=(\d+) known=(\d+) reason=(.*)', line)
        if match:
            values = match.groups()
            frames.append(dict(zip(['id', 'stage', 'percent', 'name', 'package', 'installed', 'known', 'reason'],
                [int(values[0]), int(values[1]), int(values[2]), values[3], int(values[4]), int(values[5]), int(values[6]), values[7]])))
        if 'ZION_UI ' in line:
            pages.append({k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)})
        if any(key in line for key in ('ELF file SHA256:', 'Loaded app from partition', 'Z592 BOOT slot=')):
            boot.append(line)
        if child.poll() is not None:
            if exit_at is None:
                exit_at = now
            if now - exit_at > 4:
                break
    else:
        child.terminate()
        raise TimeoutError('Bounded installation UI observer expired')
    output = child.communicate(timeout=10)[0]
    print(output, end='')
    assert child.returncode == 0, 'BLE replay failed; see captured output'

expected = json.loads(child_path.read_text())
active = [f for f in frames if f['package'] == expected['published_file_bytes']]
assert active, 'No production install-page snapshots'
assert all(0 <= f['percent'] <= 100 for f in active)
assert any(f['stage'] == 10 for f in active), 'No preflight information'
if args.corrupt_sha:
    failed = [f for f in active if f['stage'] == 255]
    assert len(failed) >= 2 and all(f['reason'] == 'package-sha256' for f in failed)
    assert failed[-1]['name'], 'Keep the resolved title/project name on a hash failure'
    assert pages[-1]['page'] == 5 and not pages[-1]['busy'], 'Failure must remain on installation page'
    assert not any(f['stage'] == 100 for f in active)
else:
    writing = [f for f in active if f['stage'] == 30]
    assert writing and all(f['known'] and f['installed'] > 0 and f['name'] for f in writing)
    assert boot, 'Child boot not observed'
result_path.write_text(json.dumps({'passed': True, 'frames': active, 'final_page': pages[-1],
    'boot': boot, 'expected_hash_rejection': args.corrupt_sha,
    'scope': 'Production service/UI snapshot and real BLE/HTTPS/Flash; physical pixels and official phone UI not verified'},
    ensure_ascii=False, indent=2), encoding='utf8')
print('Production install information observer: PASS')
