"""Optional destructive acceptance on an explicitly disposable test device."""
import argparse
import json
from pathlib import Path
from zion_ui import ZionDevice

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--expected-free-gain', required=True, type=int)
parser.add_argument('--delete', action='store_true', required=True)
parser.add_argument('--report', required=True)
args = parser.parse_args()
device = ZionDevice(args.port)
try:
    initial = device.home()
    assert initial['apps'] == 1 and initial['default'] == -1 and initial['aligned'] == 1
    device.select_list(0)
    device.key('LONG'); device.wait(page=1, selected=0)
    device.key('DOWN'); device.key('OK'); device.wait(default=0, busy=0)
    device.key('LONG'); device.wait(page=0, marker=0, aligned=1)
    device.key('LONG'); device.wait(page=1, selected=0)
    device.key('DOWN'); device.key('DOWN'); device.key('OK')
    device.wait(page=2, selected=0)
    device.key('OK'); device.wait(page=1, apps=1, default=0)
    device.key('DOWN'); device.key('DOWN'); device.key('OK')
    device.wait(page=2, selected=0)
    device.key('DOWN'); device.key('OK')
    final = device.wait(page=0, apps=0, default=-1, busy=0, marker=-1)
    assert final['free'] - initial['free'] == args.expected_free_gain
    Path(args.report).write_text(json.dumps({
        'passed': True, 'scope': 'test-only key injection through actual queue/router/LVGL; not physical buttons',
        'checks': ['management navigation without release-click boot', 'default setting',
                   'visible trailing default marker and aligned LVGL rows', 'delete cancellation',
                   'confirmed last-game removal', 'default cleared', 'exact storage reclamation'],
        'initial': initial, 'final': final, 'frames': device.frames,
    }, indent=2), encoding='utf8')
    print('Zion controls/default/delete acceptance: PASS')
finally:
    device.close()
