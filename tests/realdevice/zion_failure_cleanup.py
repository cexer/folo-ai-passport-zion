"""Actual resource hash failure and confirmed UI cancellation reclaim all capacity."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from zion_ui import ZionDevice
p=argparse.ArgumentParser()
for field in ['port','identity-file','credentials-file','bleak-path','report']:p.add_argument('--'+field,required=True)
p.add_argument('--case',choices=['all','resource','cancel'],default='all')
args=p.parse_args();report=Path(args.report);d=ZionDevice(args.port,initial_key='UP')
child=None;passed=False;cases=[]
try:
    d.home();baseline=d.wait(page=0,apps=1,default=-1,busy=0,online=1)
    d.command('ZION HOLD ON')
    for play,cancel in [(725,False),(590,True)]:
        if args.case!='all' and (args.case=='cancel')!=cancel:continue
        output='';cancelled=False;old_frames=len(d.install_frames);old_transfers=len(d.transfers);cancel_bytes=0
        cmd=[sys.executable,str(Path(__file__).with_name('zion_install.py')),'--play',str(play),
             '--identity-file',args.identity_file,'--credentials-file',args.credentials_file,'--bleak-path',args.bleak_path,
             '--timeout','600','--report',str(report.with_name(report.stem+f'-{play}.ble.json')),
             '--expect-cancel' if cancel else '--corrupt-sha']
        child=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8')
        deadline=time.monotonic()+640;last_query=0;last_bucket=-1
        while child.poll() is None and time.monotonic()<deadline:
            if time.monotonic()-last_query>.5:d.state();last_query=time.monotonic()
            else:d.pump(.05)
            new=d.install_frames[old_frames:]
            if new and new[-1].get('stage')==30:
                pct=new[-1].get('percent',0)
                if pct//10!=last_bucket:last_bucket=pct//10;print(f'Play {play}: {pct}%',flush=True)
                if cancel and not cancelled and len(d.transfers)>old_transfers and d.transfers[-1].get('bytes',0)>131072:
                    cancel_bytes=d.transfers[-1]['bytes']
                    d.key('LONG');d.wait(page=6,busy=1);d.key('DOWN');d.key('OK');cancelled=True
        assert child.poll() is not None,'Bounded failure test expired'
        output=child.communicate(timeout=10)[0];assert child.returncode==0,output[-1200:]
        final=d.wait(busy=0,apps=baseline['apps'],free=baseline['free'])
        new=d.install_frames[old_frames:]
        failure=next(f for f in reversed(new) if f.get('stage')==255)
        assert failure.get('cleanup')==1 and failure['reason']==('cancelled' if cancel else 'package-sha256')
        assert not cancel or cancelled
        assert not any(f.get('stage')==100 for f in new)
        d.home();final=d.wait(page=0,apps=1,default=-1,free=baseline['free'])
        cases.append({'play':play,'cancel':cancel,'cancel_confirmed':cancelled,'bytes_before_cancel':cancel_bytes,'failure':failure,'final':final,'client_output':output})
        print(f'Play {play} cleanup: PASS; free={final["free"]}',flush=True)
    passed=True
finally:
    if child is not None and child.poll() is None:child.terminate()
    report.write_text(json.dumps({'passed':passed,'scope':'Real BLE/Wi-Fi/Flash, resource-bearing wrong SHA and actual UI queue cancellation; no physical ADC claim',
        'cases':cases,'trace':d.trace,'install_frames':d.install_frames},ensure_ascii=False,indent=2),encoding='utf8')
    d.close()
print('Real failure/cancel space reclamation: PASS')
