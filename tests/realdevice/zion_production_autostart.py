"""Observe production install/automatic child boot, with no synthetic keys or reboot command."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from zion_ui import ZionDevice

p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--package',required=True)
p.add_argument('--identity-file',required=True)
p.add_argument('--credentials-file',required=True)
p.add_argument('--bleak-path',required=True)
p.add_argument('--child-elf-prefix',required=True)
p.add_argument('--report',required=True)
args=p.parse_args()
manifest=json.loads((Path(args.package)/'manifest.json').read_text())
assert not manifest['test_control'],'Automatic startup must use the production package'
report=Path(args.report);d=ZionDevice(args.port);child=None;passed=False;output=''
try:
    initial=d.wait(timeout=35,page=0,auto=0,busy=0,online=1,default=-1)
    d.command('Z592 STATE');d.pump(.5)
    assert any('elf='+manifest['app_elf_sha256'][:8] in line for line in d.trace),'Production ELF identity mismatch'
    cmd=[sys.executable,str(Path(__file__).with_name('zion_install.py')),'--play','590',
         '--identity-file',args.identity_file,'--credentials-file',args.credentials_file,
         '--bleak-path',args.bleak_path,'--no-reboot-request','--timeout','420',
         '--report',str(report.with_suffix('.ble.json'))]
    child=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8')
    deadline=time.monotonic()+460;last_query=0
    while time.monotonic()<deadline:
        d.pump(.1)
        if any('ELF file SHA256:' in line and args.child_elf_prefix in line for line in d.trace):break
        if time.monotonic()-last_query>=2:
            d.serial.write(b'ZION STATE\n');d.serial.flush();last_query=time.monotonic()
        if child.poll() is not None and child.returncode:
            output=child.communicate(timeout=5)[0];raise AssertionError('BLE install failed: '+output[-1000:])
    assert any('ELF file SHA256:' in line and args.child_elf_prefix in line for line in d.trace),'Automatic child boot not observed'
    output=child.communicate(timeout=15)[0];assert child.returncode==0,output[-1000:]
    ble=json.loads(report.with_suffix('.ble.json').read_text())
    assert ble['passed'] and not ble['reboot_request_sent']
    full=[line for line in d.trace if 'Z592_TRANSFER_END pass=full' in line]
    prefix=[line for line in d.trace if 'Z592_TRANSFER_END pass=layout' in line]
    assert len(full)==len(prefix)==1
    passed=True
finally:
    if child is not None and child.poll() is None:child.terminate()
    report.write_text(json.dumps({'passed':passed,'production_elf_sha256':manifest['app_elf_sha256'],
        'child_elf_prefix':args.child_elf_prefix,'scope':'Production firmware, Windows BLE replay, real HTTPS/Flash and autonomous child boot; no key injection or BLE reboot request',
        'trace':d.trace,'install_frames':d.install_frames,'transfers':d.transfers,'client_output':output},ensure_ascii=False,indent=2),encoding='utf8')
    d.close()
print('Production install and autonomous startup: PASS')
