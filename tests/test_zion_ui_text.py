"""Check glyph coverage and the flat-theme timeout teardown regression."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
main = (root / 'main/main.c').read_text(encoding='utf8')
coverage = set((root / 'assets/fonts/coverage.txt').read_text(encoding='utf8'))
missing = set()
for name in ['main/main.c', 'main/zion_text.c', 'main/zion_launcher.c', 'main/zion_install_view.c']:
    src = (root / name).read_text(encoding='utf8')
    for literal in re.findall(r'"(?:[^"\\]|\\.)*"|/\*.*?\*/|//[^\n]*', src, flags=re.S):
        if not literal.startswith('"'):
            continue
        missing.update(c for c in literal if '\u4e00' <= c <= '\u9fff' and c not in coverage)
assert not missing, f'Font missing UI glyphs: {sorted(missing)}'
cleanup = main.split('static void store_clear_timeout_prompt(void)', 1)[1].split('static void store_tick', 1)[0]
assert 'lv_obj_delete(s_timeout_panel)' in cleanup
assert 'lv_obj_get_child' not in cleanup and 'lv_obj_get_index' not in cleanup, 'flat panel must not delete preceding siblings'
assert 'lv_obj_get_child(panel, 2)' in main and '7 + (extent.x < 188' in main
assert 'slot == default_slot ? "* "' not in main, 'default marker must follow the title'
print('Zion UI: PASS (Chinese glyph coverage, trailing marker, timeout ownership)')
