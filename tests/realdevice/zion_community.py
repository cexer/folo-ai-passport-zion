"""Authorized test-only key acceptance: community QR, retry and settings.

This inspects actual LVGL object state and QR generation logs, not LCD pixels.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from zion_ui import ZionDevice

p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--identity-file',required=True)
p.add_argument('--credentials-file',required=True)
p.add_argument('--bleak-path',required=True)
p.add_argument('--report',required=True)
args=p.parse_args()
report=Path(args.report).resolve()
states={}
d=ZionDevice(args.port)
try:
    d.home()
    states['list']=d.wait(timeout=35,page=0,busy=0,online=1,settings_marked=1)
    d.select_list(states['list']['apps']);d.key('OK')
    states['install']=d.wait(page=5,busy=0,qr=1)
    assert any('ZION_COMMUNITY_QR kind=official-mini-code' in line for line in d.trace), 'Official Mini Program code was not loaded'
    child=subprocess.Popen([sys.executable,str(Path(__file__).with_name('zion_install.py')),
        '--play','138','--identity-file',args.identity_file,'--credentials-file',args.credentials_file,
        '--bleak-path',args.bleak_path,'--corrupt-sha','--timeout','200',
        '--report',str(report.with_suffix('.ble.json'))],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8')
    end=time.monotonic()+250
    while child.poll() is None and time.monotonic()<end:
        d.pump(.1)
    if child.poll() is None:
        child.terminate();raise TimeoutError('BLE negative-path replay expired')
    output=child.communicate(timeout=5)[0];print(output,end='')
    assert child.returncode==0, 'Expected package rejection did not complete'
    states['failure']=d.wait(page=5,busy=0,qr=0)
    assert states['failure']['result']!=0
    d.pump(2)
    assert d.state()['qr']==0, 'QR must not hide an unattended failure'
    d.key('OK');d.wait(page=0,busy=0)
    d.select_list(states['list']['apps']);d.key('OK')
    states['retry']=d.wait(page=5,busy=0,qr=1)
    d.key('OK');d.wait(page=0,busy=0)
    d.select_list(states['list']['apps']+1);d.key('OK')
    states['settings']=d.wait(page=3,busy=0,qr=0)
    d.key('LONG');states['final']=d.wait(page=0,busy=0,settings_marked=1)
    report.write_text(json.dumps({'passed':True,'states':states,'trace':d.trace,
        'scope':'Test-control keys through actual queue/router/LVGL and real BLE hash rejection; physical scan/pixels/buttons not verified'},indent=2),encoding='utf8')
    print('Community QR direct entry/retry/settings: PASS')
finally:
    d.close()
