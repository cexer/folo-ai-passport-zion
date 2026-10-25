#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${root}"
lv="managed_components/lvgl__lvgl"
python3 - <<'PY'
from pathlib import Path
defaults=Path('sdkconfig.defaults').read_text(encoding='utf8').splitlines()
assert 'CONFIG_LV_DRAW_SW_SUPPORT_ARGB8888=y' in defaults, 'QR decoder emits ARGB8888; RGB565-only output support is insufficient'
print('QR renderer configuration: PASS')
PY
if [[ ! -f "${lv}/src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.c" ]]; then
    echo 'QR vendor pixel fixture: NOT RUN (managed LVGL dependency unavailable)'
    exit 0
fi
tmp="$(mktemp -d /tmp/zion-qr-render.XXXXXX)"
trap 'rm -f -- "${tmp}/blend-0" "${tmp}/blend-0.exe" "${tmp}/blend-1" "${tmp}/blend-1.exe"; rmdir -- "${tmp}"' EXIT
for flag in 0 1; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -O2 \
        -DLV_CONF_SKIP=1 -DLV_USE_OS=0 -DLV_USE_LOG=0 -DLV_USE_STDLIB_STRING=1 \
        -DLV_DRAW_SW_SUPPORT_ARGB8888="${flag}" -DLV_DRAW_SW_SUPPORT_RGB565_SWAPPED=0 \
        -DLV_DRAW_SW_SUPPORT_RGB888=1 -DLV_DRAW_SW_SUPPORT_XRGB8888=0 \
        -DLV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED=0 -DLV_DRAW_SW_SUPPORT_L8=0 \
        -DLV_DRAW_SW_SUPPORT_AL88=0 -I"${lv}" tests/test_zion_qr_blend.c \
        "${lv}/src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.c" \
        "${lv}/src/misc/lv_color.c" "${lv}/src/misc/lv_color_op.c" \
        "${lv}/src/stdlib/clib/lv_string_clib.c" -o "${tmp}/blend-${flag}"
    "${tmp}/blend-${flag}"
done
