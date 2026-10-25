"""Authorized real HTTP scan/provisioning + same-link BLE failure/retry test.

Uses private external identity/credentials; never emits SSIDs or passwords.
HTTP uses the existing STA address: phone association with the AP is unverified.
Requires a test-control build to navigate the real UI queue.
"""
import argparse
import asyncio
import json
import io
import hashlib
from pathlib import Path
import struct
import socket
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from zion_ui import ZionDevice

p = argparse.ArgumentParser()
for field in ['port', 'identity-file', 'credentials-file', 'bleak-path', 'report']:
    p.add_argument('--' + field, required=True)
args = p.parse_args()
sys.coinit_flags = 0
sys.path.insert(0, args.bleak_path)
from bleak import BleakClient, BleakScanner

http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
report = {'passed': False, 'phone_ap_association': 'NOT RUN', 'cases': []}
d = ZionDevice(args.port, initial_key='UP')
uuid = lambda value: f'0000{value:04x}-0000-1000-8000-00805f9b34fb'


def request(base, path, data=None, headers=None):
    with http.open(urllib.request.Request(base + path, data=data, headers=headers or {}), timeout=15) as response:
        result = io.BytesIO(response.read())
        result.status = response.status
        result.headers = response.headers
        return result


def read_status(data):
    state, error, _, received, total = struct.unpack('<BBHII', bytes(data))
    return dict(state=state, error=error, received=received, total=total)


def probe_dns():
    # The caller may add a temporary host route to the AP IP via the existing
    # device STA IP. This still does not claim a phone associated with the AP.
    q = bytes.fromhex('123401000001000000000000') + b'\x03abc\x00\x00\x01\x00\x01'
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2)
        sock.sendto(q, ('192.168.4.1', 53))
        response, _ = sock.recvfrom(512)
    assert response[:2] == b'\x12\x34' and response[-4:] == bytes([192,168,4,1])
    assert response[7] == 1


async def transfer(client, payload):
    await client.write_gatt_char(uuid(0xfff6), b'\x0a\x02' + struct.pack('<H', len(payload)), response=True)
    for at in range(0, len(payload), 17):
        await client.write_gatt_char(uuid(0xfff6), b'\x0b' + struct.pack('<H', at) + payload[at:at+17], response=True)
    await client.write_gatt_char(uuid(0xfff6), b'\x0c', response=True)


async def failures():
    name = Path(args.identity_file).read_text().strip()
    target = await BleakScanner.find_device_by_filter(lambda dev, _: dev.name == name, timeout=20)
    assert target, 'Authorized device not found'
    async with BleakClient(target, timeout=20, winrt={'use_cached_services': False}) as client:
        await client.read_gatt_char(uuid(0xfff4))
        assert read_status(await client.read_gatt_char(uuid(0xfff3)))['state'] == 6
        history = []
        await client.start_notify(uuid(0xfff3), lambda _, data: history.append(read_status(data)))
        for number in range(2):
            # Real HTTPS failure, no Flash reservation and no existing app deletion.
            payload = struct.pack('<II', 0, 65536) + bytes(32) + b'https://example.invalid/zion-retry.bin'
            history.clear()
            await transfer(client, payload)
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                await asyncio.sleep(.2)
                d.pump(.02)
                if any(item['state'] == 4 for item in history) and not d.state()['busy']:
                    break
            else:
                raise TimeoutError('Expected bounded download failure')
            assert client.is_connected, 'Failure disconnected BLE'
            failed = read_status(await client.read_gatt_char(uuid(0xfff3)))
            assert failed['state'] == 4 and failed['error'] == 1
            ui = d.wait(busy=0, online=1)
            assert ui['apps'] == report['initial']['apps']
            assert ui['free'] == report['initial']['free']
            # Exact new-session handshake on the SAME live BLE link, without Wi-Fi writes.
            await client.read_gatt_char(uuid(0xfff4))
            ready = read_status(await client.read_gatt_char(uuid(0xfff3)))
            assert ready['state'] == 6 and ready['error'] == 0
            assert d.state()['result'] != 0, 'Session rearm erased the device failure diagnosis'
            report['cases'].append({'attempt': number+1, 'ble_still_connected': True,
                'failure': failed, 'new_session': ready, 'credentials_resent': False,
                'apps_preserved': True, 'capacity_preserved': True, 'ui_failure_preserved': True})
    async with BleakClient(target, timeout=20, winrt={'use_cached_services': False}) as client:
        await client.read_gatt_char(uuid(0xfff4))
        assert read_status(await client.read_gatt_char(uuid(0xfff3)))['state'] == 6
    report['reconnect_without_provisioning'] = True


try:
    d.home()
    report['initial'] = d.wait(page=0, busy=0, online=1)
    d.select_list(report['initial']['apps'] + 1)
    d.key('OK'); d.wait(page=3, selected=0)
    d.key('OK'); d.wait(page=4, ap=1, busy=0)
    base = 'http://' + d.network_address()
    page = request(base, '/').read().decode()
    assert '<select name=ssid' in page and '<input name=ssid' not in page
    assert '重新扫描' in page and '无需密码' in page
    assert request(base, '/wifi.js').headers['Content-Type'].startswith('application/javascript')
    # DNS is AP-bound. A caller-installed route allows exercising that exact
    # listener through Ethernet without changing the device's production bind.
    try:
        probe_dns()
        report['real_ap_dns'] = 'PASS (routed to AP address via STA)'
    except (OSError, TimeoutError):
        report['real_ap_dns'] = 'NOT RUN (AP route/association unavailable)'
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *args, **kwargs):
            return None
    probe = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    for path in ['/generate_204', '/hotspot-detect.html', '/ncsi.txt']:
        try:
            probe.open(base+path, timeout=10)
            raise AssertionError('OS connectivity probe was not redirected')
        except urllib.error.HTTPError as error:
            assert error.code == 303 and error.headers['Location'] == 'http://192.168.4.1/'
            assert error.read(), 'iOS needs a nonempty response body'
            error.close()
    report['os_http_probes'] = 'PASS (Android/iOS/Windows paths; actual phone popup NOT RUN)'
    png = request(base, '/mini-code.png')
    assert png.headers['Content-Type'] == 'image/png'
    assert hashlib.sha256(png.read()).digest() == hashlib.sha256((Path(__file__).resolve().parents[2]/'assets/images/folo-mini-code-208.png').read_bytes()).digest()
    report['offline_mini_code'] = True
    token = request(base, '/token').read().decode()
    assert len(token) == 16
    try:
        request(base, '/scan', b'', {'X-Zion-Token': '0'*16})
        raise AssertionError('Invalid scan token accepted')
    except urllib.error.HTTPError as error:
        assert error.code == 400
        error.read();error.close()
    scan_counts = []
    wifi = json.loads(Path(args.credentials_file).read_text())
    for _ in range(2):
        assert request(base, '/scan', b'', {'X-Zion-Token': token}).status == 200
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            result = json.load(request(base, '/scan'))
            if not result['pending']:
                break
            time.sleep(.5)
        assert not result['pending'] and result['error'] == 0
        assert any(bytes.fromhex(n['hex']).decode('utf8', 'replace') == wifi['ssid'] for n in result['networks'])
        scan_counts.append(len(result['networks']))
    report['real_scan_counts'] = scan_counts
    payload = urllib.parse.urlencode({**wifi, 'token': token}).encode()
    result_page = request(base, '/wifi', payload).read().decode()
    assert 'result.js' in result_page and 'weixin://dl/business/?appid=wxd2a90fe8e2accc23' in result_page
    assert json.load(request(base, '/status')) == {'state':0,'error':0,'pending':True}
    assert request(base, '/apply', urllib.parse.urlencode({'token': token}).encode()).status == 200
    d.wait(page=4, ap=1, online=1, busy=0, result=0)
    assert json.load(request(base, '/status'))['state'] == 2
    report['success_delivered_before_ap_close'] = True
    try:
        request(base, '/finish', urllib.parse.urlencode({'token':'0'*16}).encode())
        raise AssertionError('Invalid finish token accepted')
    except urllib.error.HTTPError as error:
        assert error.code == 400
        error.read();error.close()
    assert request(base, '/finish', urllib.parse.urlencode({'token': token}).encode()).status == 200
    d.wait(page=4, ap=0, online=1, busy=0, result=0)
    report['acknowledged_handoff_closed_ap'] = True
    report['real_http_provisioning'] = True
    d.home()
    asyncio.run(failures())
    d.home()
    report['final'] = d.wait(page=0, busy=0, online=1)
    report['passed'] = True
    print('Real HTTP scan/rescan/provisioning + BLE same-link failure/session retry: PASS')
finally:
    report['trace'] = d.trace
    Path(args.report).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    d.close()
