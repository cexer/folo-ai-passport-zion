#include "ui_pixel.h"

/* All decoration is drawn from rectangles and labels; no images or animation. */
static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *ui_pixel_label(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, uint32_t color)
{
    (void)font;
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, ZION_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *ui_pixel_screen_create(const char *title)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_SKY), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_t *heading = ui_pixel_label(scr, title, ZION_FONT, UI_YELLOW);
    lv_obj_set_style_text_font(heading, ZION_FONT, 0);
    lv_obj_set_pos(heading, 10, 9);
    block(scr, 10, 32, 220, 1, UI_INK2);
    block(scr, 10, 288, 220, 1, UI_INK2);
    return scr;
}

lv_obj_t *ui_pixel_panel_create_plain(lv_obj_t *parent, int x, int y, int w, int h,
                                     uint32_t color)
{
    lv_obj_t *panel = block(parent, x, y, w, h, color);
    lv_obj_set_style_border_color(panel, lv_color_hex(UI_INK2), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_all(panel, 3, 0);
    return panel;
}

lv_obj_t *ui_pixel_panel_create_shadowed(lv_obj_t *parent, int x, int y, int w, int h,
                                        uint32_t color, lv_obj_t **shadow_out)
{
    if (shadow_out) *shadow_out = NULL;
    return ui_pixel_panel_create_plain(parent, x, y, w, h, color);
}

lv_obj_t *ui_pixel_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                               uint32_t color)
{
    return ui_pixel_panel_create_plain(parent, x, y, w, h, color);
}

lv_obj_t *ui_pixel_mascot_create(lv_obj_t *parent, int x, int y)
{
    (void)parent; (void)x; (void)y;
    return NULL;
}

void ui_pixel_mascot_jump(lv_obj_t *mascot) { (void)mascot; }

void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled)
{
    if (!panel) return;
    lv_obj_set_style_bg_color(panel, lv_color_hex(selected ? UI_YELLOW : UI_PAPER), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(selected ? UI_YELLOW : UI_INK2), 0);
    const uint32_t color = !enabled ? UI_INK2 : selected ? UI_SKY : UI_INK;
    for (uint32_t i = 0; i < lv_obj_get_child_count(panel); ++i)
        lv_obj_set_style_text_color(lv_obj_get_child(panel, i), lv_color_hex(color), 0);
}
