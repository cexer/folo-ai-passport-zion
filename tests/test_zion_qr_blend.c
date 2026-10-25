/* Real LVGL RGB565 blender, fed the exact ARGB8888 black/white format emitted
 * by its indexed-image decoder. Compile both with support disabled and enabled. */
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/* Host allocator for unused strdup entry points in LVGL's string object. */
void *lv_malloc(size_t bytes) {return malloc(bytes);}
int main(void) {
    const uint32_t pixels[]={0xffffffff,0xff000000,0xff000000,0xffffffff};
    uint16_t out[]={0x1234,0x1234,0x1234,0x1234};
    lv_draw_sw_blend_image_dsc_t d={.dest_buf=out,.dest_w=2,.dest_h=2,.dest_stride=4,
        .src_buf=pixels,.src_stride=8,.src_color_format=LV_COLOR_FORMAT_ARGB8888,
        .opa=255,.blend_mode=LV_BLEND_MODE_NORMAL};
    lv_draw_sw_blend_image_to_rgb565(&d);
#if LV_DRAW_SW_SUPPORT_ARGB8888
    assert(out[0]==0xffff && out[1]==0 && out[2]==0 && out[3]==0xffff);
    puts("PASS real LVGL blender: ARGB8888 enabled renders black/white into RGB565");
#else
    for(unsigned i=0;i<4;i++) assert(out[i]==0x1234);
    puts("PASS reproduced missing QR: ARGB8888 disabled leaves all RGB565 pixels unchanged");
#endif
}
