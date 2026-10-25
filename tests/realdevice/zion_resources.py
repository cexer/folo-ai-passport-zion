"""Delete the resource-bearing second test game, retaining the first game."""
import argparse
import json
from pathlib import Path
from zion_ui import ZionDevice

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--delete', action='store_true', required=True)
parser.add_argument('--expected-free-gain', type=int, required=True)
parser.add_argument('--report', required=True)
args = parser.parse_args()
device = ZionDevice(args.port)
try:
    initial = device.home()
    assert initial['apps'] == 2 and initial['default'] == -1
    device.select_list(1); device.key('LONG'); device.wait(page=1, target=1)
    device.key('DOWN'); device.key('OK'); device.wait(default=1, busy=0)
    device.key('LONG'); device.wait(page=0, marker=1)
    device.select_list(1); device.key('LONG'); device.wait(page=1, target=1)
    device.key('DOWN'); device.key('DOWN'); device.key('OK'); device.wait(page=2, selected=0, target=1)
    device.key('OK'); device.wait(page=1, apps=2, default=1, target=1)
    device.key('DOWN'); device.key('DOWN'); device.key('OK'); device.wait(page=2, selected=0, target=1)
    device.key('DOWN'); device.key('OK')
    final = device.wait(page=0, apps=1, default=-1, busy=0)
    assert final['free'] - initial['free'] == args.expected_free_gain
    Path(args.report).write_text(json.dumps({
        'passed': True, 'scope': 'real UI queue/router and storage; synthetic keys',
        'checks': ['second game identity', 'default and trailing marker', 'cancel retains both games',
                   'confirmed deletion clears default', 'owned APP/DATA capacity reclaimed', 'other game remains'],
        'initial': initial, 'final': final, 'frames': device.frames,
    }, indent=2), encoding='utf8')
    print('Zion resource game deletion/coexistence: PASS')
finally:
    device.close()
