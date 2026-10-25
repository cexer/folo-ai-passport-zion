"""Authorized USB-reset/default/escape/offline-launch acceptance."""
import argparse
import atexit
import asyncio
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
from zion_ui import ZionDevice

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--identity-file', required=True)
parser.add_argument('--bleak-path')
parser.add_argument('--child-elf-prefix', required=True)
parser.add_argument('--report', required=True)
args = parser.parse_args()
sys.coinit_flags = 0
if args.bleak_path:
    sys.path.insert(0, str(Path(args.bleak_path).resolve()))
from bleak import BleakClient, BleakScanner

observations = {}
completed = False


def checkpoint():
    Path(args.report).write_text(json.dumps({'passed': completed,
        'scope': 'USB-reset cold path and synthetic keys; not power-button/ADC acceptance',
        'checks': observations, 'last_frames': device.frames if 'device' in globals() else [],
        'last_trace': device.trace if 'device' in globals() else []}, indent=2), encoding='utf8')


atexit.register(checkpoint)


def reset():
    result = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32c3', '--port', args.port,
                             '--before', 'usb_reset', '--after', 'hard_reset', 'read_mac'],
                            capture_output=True, text=True)
    assert result.returncode == 0, 'Authorized own-device USB reset failed; identity omitted'


def wait_boot(device, seconds=8):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        device.pump(.2)
        if any('ELF file SHA256:' in line and args.child_elf_prefix in line for line in device.trace):
            return
    raise AssertionError('Expected child boot not observed')


device = ZionDevice(args.port)
try:
    state = device.home()
    assert state['apps'] == 1 and state['default'] == -1
    device.select_list(0); device.key('LONG'); device.wait(page=1)
    device.key('DOWN'); device.key('OK'); device.wait(default=0, busy=0)
finally:
    device.close()

reset()
device = ZionDevice(args.port)
try:
    device.wait_ready()
    observations['cold_reset_default'] = device.wait(default=0, auto=1)
    wait_boot(device)
    observations['cold_reset_child_boot'] = True
finally:
    device.close()

reset()
device = ZionDevice(args.port)
try:
    device.wait_ready(); device.key('OK')
    observations['ok_cancel'] = device.wait(page=0, default=0, auto=0, busy=0)
    device.pump(2.5)
    assert not any('ELF file SHA256:' in line and args.child_elf_prefix in line for line in device.trace)
    observations['ok_release_did_not_launch'] = True

    async def unavailable_wifi_launch():
        name = Path(args.identity_file).read_text().strip()
        target = await BleakScanner.find_device_by_name(name, timeout=20)
        assert target, 'Own Zion device not advertising'
        client = BleakClient(target, timeout=20, winrt={'use_cached_services': False})
        uuid = lambda n: f'0000{n:04x}-0000-1000-8000-00805f9b34fb'
        try:
            await client.connect()
            ssid, password = b'Zion-QA-Unavailable-5841', b'test-only-pass'
            payload = bytes([len(ssid), len(password)]) + ssid + password
            await client.write_gatt_char(uuid(0xfff6), b'\x0a\x01' + struct.pack('<H', len(payload)), response=True)
            for at in range(0, len(payload), 17):
                await client.write_gatt_char(uuid(0xfff6), b'\x0b' + struct.pack('<H', at) + payload[at:at+17], response=True)
            await client.write_gatt_char(uuid(0xfff6), b'\x0c', response=True)
            await asyncio.sleep(.4)
            status = bytes(await client.read_gatt_char(uuid(0xfff3)))
            assert status[0] == 5, 'Unavailable network was not in connection wait'
            started = time.monotonic()
            device.select_list(0); device.key('OK')
            wait_boot(device)
            elapsed = time.monotonic() - started
            assert elapsed < 8, 'Game launch waited for the 20-second Wi-Fi timeout'
            observations['offline_launch_seconds'] = round(elapsed, 3)
        finally:
            try:
                await client.disconnect()
            except Exception:
                pass  # Verified child reboot closes the transport.
    asyncio.run(unavailable_wifi_launch())
finally:
    device.close()

reset()
device = ZionDevice(args.port)
try:
    device.wait_ready(); device.key('UP')
    device.wait(page=0, default=0, auto=0)
    device.wait(online=1, busy=0)  # Failed fake network must not replace saved credentials.
    device.select_list(0); device.key('LONG'); device.wait(page=1)
    device.key('DOWN'); device.key('OK'); device.wait(default=-1, busy=0)
    device.key('LONG'); observations['final'] = device.wait(page=0, apps=1, default=-1, online=1)
finally:
    device.close()

completed = True
checkpoint()
print('Zion default/cancel/offline-priority acceptance: PASS')
