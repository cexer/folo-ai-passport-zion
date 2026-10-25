"""Real-device download timing; optional live modem-sleep crossover on one TCP transfer.

Requires the explicitly enabled test controls. Each successful install must
contain one short prefix probe and exactly one complete BIN transfer.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from zion_ui import ZionDevice

p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--play',type=int,default=590)
p.add_argument('--identity-file',required=True)
p.add_argument('--credentials-file',required=True)
p.add_argument('--bleak-path',required=True)
p.add_argument('--report',required=True)
p.add_argument('--crossover',action='store_true')
p.add_argument('--baseline-seconds',type=int,default=25)
p.add_argument('--initial-ps',choices=['on','off'])
p.add_argument('--switch-command',default='ZION PS OFF')
p.add_argument('--timeout',type=int,default=600)
args=p.parse_args()
report=Path(args.report).resolve()
d=ZionDevice(args.port,initial_key='UP')
child=None;passed=False;output='';switch_time=None;full_start=None
def rate(frames):
    frames=[f for f in frames if f.get('bytes',0)>0]
    if len(frames)<2: return None
    first,last=frames[0],frames[-1]
    elapsed=(last['ms']-first['ms'])/1000
    return round((last['bytes']-first['bytes'])/elapsed,2) if elapsed>0 else None
try:
    initial=d.state()
    if initial.get('auto'): d.key('UP')
    d.home();d.wait(timeout=35,page=0,busy=0,online=1)
    initial_ps=args.initial_ps or ('on' if args.crossover else 'off')
    d.command('ZION HOLD ON');d.command('ZION PS ON' if initial_ps=='on' else 'ZION PS OFF')
    d.command('ZION RADIO')
    d.command('Z592 STATE')
    cmd=[sys.executable,str(Path(__file__).with_name('zion_install.py')),'--play',str(args.play),
         '--identity-file',args.identity_file,'--credentials-file',args.credentials_file,
         '--bleak-path',args.bleak_path,'--timeout',str(args.timeout),'--report',str(report.with_suffix('.ble.json'))]
    child=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8')
    start=time.monotonic();last_query=0
    while child.poll() is None and time.monotonic()-start<args.timeout+45:
        now=time.monotonic()
        if now-last_query>=.5:
            try: d.state()
            except TimeoutError: pass
            last_query=now
        else: d.pump(.05)
        if d.install_frames and d.install_frames[-1].get('stage')==30 and d.transfers and d.transfers[-1].get('bytes',0)>0:
            if full_start is None: full_start=time.monotonic()
            if args.crossover and switch_time is None and time.monotonic()-full_start>=args.baseline_seconds:
                switch_time=time.monotonic();d.command(args.switch_command)
                print('Live comparison: '+args.switch_command,flush=True)
    if child.poll() is None:
        child.terminate();raise TimeoutError('Bounded benchmark expired')
    output=child.communicate(timeout=10)[0];print(output,end='')
    assert child.returncode==0,'BLE/HTTPS install did not finish successfully'
    d.pump(2);d.state()
    ble=json.loads(report.with_suffix('.ble.json').read_text())
    transfer_ends=[]
    for line in d.trace:
        m=re.search(r'Z592_TRANSFER_END pass=(layout|full) bytes=(\d+) total=(\d+) ms=(\d+)',line)
        if m: transfer_ends.append({'pass':m[1],'bytes':int(m[2]),'total':int(m[3]),'ms':int(m[4])})
    full=[f for f in transfer_ends if f['pass']=='full']
    prefix=[f for f in transfer_ends if f['pass']=='layout']
    assert len(full)==len(prefix)==1 and full[0]['bytes']==ble['published_file_bytes']
    assert prefix[0]['bytes']<ble['published_file_bytes']
    if args.play==590:
        assert any(frame.get('name')=='小智情绪胸牌' for frame in d.install_frames),'Official Chinese title did not resolve'
        assert any('Z592_TITLE matched=1' in line for line in d.trace)
    d.home();passed=True
finally:
    if child is not None and child.poll() is None: child.terminate()
    if switch_time:
        before=[f for f in d.transfers if full_start and full_start+2<=f['host_time']<=switch_time]
        after=[f for f in d.transfers if f['host_time']>=switch_time+1]
    else: before=[];after=d.transfers
    report.write_text(json.dumps({'passed':passed,'play':args.play,'crossover':args.crossover,
        'switch_time':switch_time,'switch_command':args.switch_command,'initial_ps':args.initial_ps,
        'before_bps':rate(before),'after_bps':rate(after),
        'transfers':d.transfers,'install_frames':d.install_frames,'trace':d.trace,'client_output':output,
        'scope':'Real device radio/HTTPS/Flash and actual-byte timing; Windows BLE replay, not phone frontend'},
        ensure_ascii=False,indent=2),encoding='utf8')
    d.close()
print('Real one-complete-transfer benchmark: PASS')
