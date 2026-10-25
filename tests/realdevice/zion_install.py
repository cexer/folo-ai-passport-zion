"""Replay official v4 requests against a selected, authorized real device.

This tests BLE/Wi-Fi/HTTPS/Flash. It is not the official phone UI acceptance.
Credentials and device identity are read from private files and never reported.
"""
import argparse
import asyncio
import json
from pathlib import Path
import struct
import sys
import time
import urllib.parse
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument('--play', required=True, type=int)
parser.add_argument('--identity-file', required=True)
parser.add_argument('--credentials-file', required=True)
parser.add_argument('--report', required=True)
parser.add_argument('--bleak-path')
parser.add_argument('--timeout', type=int, default=1800)
parser.add_argument('--corrupt-sha', action='store_true', help='Expect rejection of a deliberately wrong package hash before executable header/VALID publication')
parser.add_argument('--no-reboot-request', action='store_true', help='Observe the firmware automatic startup without sending BLE reboot command 4')
parser.add_argument('--expect-cancel', action='store_true', help='Expect ERROR after a separately confirmed device UI cancellation')
args = parser.parse_args()
sys.coinit_flags = 0
if args.bleak_path:
    sys.path.insert(0, str(Path(args.bleak_path).resolve()))
from bleak import BleakClient, BleakScanner

name = Path(args.identity_file).read_text().strip()
wifi = json.loads(Path(args.credentials_file).read_text())
play = json.load(urllib.request.urlopen(f'https://ai-passport.folotoy.cn/api/plays/id/{args.play}', timeout=20))['play']
firmware = play['firmware']
url = urllib.parse.urljoin('https://ai-passport.folotoy.cn', firmware['url'])
uuid = lambda value: f'0000{value:04x}-0000-1000-8000-00805f9b34fb'


def status(data):
    assert len(data) == 12
    return dict(zip(['state', 'error', 'reserved', 'received', 'total'], struct.unpack('<BBHII', data)))


async def transfer(client, kind, payload):
    await client.write_gatt_char(uuid(0xfff6), b'\x0a' + bytes([kind]) + struct.pack('<H', len(payload)), response=True)
    for at in range(0, len(payload), 17):
        await client.write_gatt_char(uuid(0xfff6), b'\x0b' + struct.pack('<H', at) + payload[at:at+17], response=True)
    await client.write_gatt_char(uuid(0xfff6), b'\x0c', response=True)


async def main():
    target = await BleakScanner.find_device_by_name(name, timeout=20)
    assert target, 'Selected own device not advertising'
    history = []
    start = time.monotonic()

    def notification(_, data):
        if len(data) == 12:
            history.append(status(bytes(data)))

    async with BleakClient(target, timeout=20, winrt={'use_cached_services': False}) as client:
        info = bytes(await client.read_gatt_char(uuid(0xfff4)))
        assert info[:5] == b'AIPR\x04' and info[24:37].rstrip(b'\0').decode() == name
        await client.start_notify(uuid(0xfff3), notification)
        initial = status(bytes(await client.read_gatt_char(uuid(0xfff3))))
        if initial['state'] == 5:
            until = time.monotonic() + 25
            while initial['state'] == 5 and time.monotonic() < until:
                await asyncio.sleep(.3)
                initial = status(bytes(await client.read_gatt_char(uuid(0xfff3))))
        if initial['state'] != 6:
            ssid, password = wifi['ssid'].encode(), wifi['password'].encode()
            await transfer(client, 1, bytes([len(ssid), len(password)]) + ssid + password)
        until = time.monotonic() + 40
        while time.monotonic() < until:
            current = status(bytes(await client.read_gatt_char(uuid(0xfff3))))
            if current['state'] == 6:
                break
            assert current['state'] != 4, 'Device Wi-Fi failed; credentials omitted'
            await asyncio.sleep(.3)
        else:
            raise TimeoutError('Device Wi-Fi did not connect')
        raw = firmware.get('format') == 'esp-app'
        package_hash = bytearray.fromhex(firmware['sha256'])
        if args.corrupt_sha:
            package_hash[0] ^= 1
        payload = struct.pack('<II', 0 if raw else 0x10000, firmware['size']) + package_hash + url.encode()
        assert len(payload) <= 552
        history.clear()
        await transfer(client, 2, payload)
        print(f'Real install submitted: play {args.play}, {firmware["size"]} bytes', flush=True)
        until = time.monotonic() + args.timeout
        bucket = -1
        while time.monotonic() < until:
            ready = next((item for item in history if item['state'] == 3), None)
            if ready:
                assert not (args.corrupt_sha or args.expect_cancel), 'Rejected/cancelled install must never report READY'
                assert ready['received'] == ready['total'] == firmware['size'], 'READY byte counter mismatch'
                break
            if (args.corrupt_sha or args.expect_cancel) and history and history[-1]['state'] == 4:
                break
            assert not history or history[-1]['state'] != 4, f'Device install failed: {history[-1] if history else {}}'
            if history:
                pct = history[-1]['received'] * 100 // max(1, history[-1]['total'])
                if pct // 10 != bucket:
                    bucket = pct // 10
                    print(f'Progress: {pct}%', flush=True)
            await asyncio.sleep(.2)
        else:
            raise TimeoutError('No READY within configured deadline')
        if not (args.corrupt_sha or args.expect_cancel) and not args.no_reboot_request:
            try:
                await client.write_gatt_char(uuid(0xfff1), b'\x04', response=True)
            except Exception:
                pass  # The verified post-DONE automatic reboot may win this race.
    Path(args.report).write_text(json.dumps({
        'passed': True, 'client': 'Windows BLE replay; not official phone mini-program',
        'play_id': args.play, 'published_file_bytes': firmware['size'], 'published_sha256': firmware['sha256'],
        'ready_byte_counters_verified': not (args.corrupt_sha or args.expect_cancel), 'expected_hash_rejection': args.corrupt_sha,
        'reboot_request_sent': not (args.corrupt_sha or args.expect_cancel) and not args.no_reboot_request,
        'expected_cancellation': args.expect_cancel,
        'elapsed_seconds': round(time.monotonic() - start, 2),
        'history': history, 'credentials_omitted': True,
    }, indent=2), encoding='utf8')
    outcome='cancellation' if args.expect_cancel else 'hash rejection' if args.corrupt_sha else 'Flash READY'
    print(f'Real BLE/HTTPS {outcome}: PASS (play {args.play})', flush=True)


asyncio.run(main())
