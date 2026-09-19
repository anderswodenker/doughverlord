#include "common.hpp"

#include "../session.hpp"

namespace ui {

lv_obj_t *make_screen()
{
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(scr, &font_ms_20, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollable(scr, false);
    return scr;
}

Header make_header(lv_obj_t *scr, const char *title, bool back_button)
{
    Header h{};
    h.root = lv_obj_create(scr);
    lv_obj_set_size(h.root, lv_pct(100), HEADER_H);
    lv_obj_align(h.root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(h.root, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_width(h.root, 0, 0);
    lv_obj_set_style_radius(h.root, 0, 0);
    lv_obj_set_style_pad_hor(h.root, 12, 0);
    lv_obj_set_style_pad_ver(h.root, 0, 0);
    lv_obj_set_scrollable(h.root, false);

    int32_t title_x = 0;
    if (back_button) {
        h.left_btn = lv_button_create(h.root);
        lv_obj_set_size(h.left_btn, 44, 32);
        lv_obj_align(h.left_btn, LV_ALIGN_LEFT_MID, -6, 0);
        lv_obj_set_style_bg_color(h.left_btn, lv_color_hex(COL_BG), 0);
        lv_obj_set_style_shadow_width(h.left_btn, 0, 0);
        lv_obj_t *l = lv_label_create(h.left_btn);
        lv_label_set_text(l, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(l, LV_FONT_DEFAULT, 0);
        lv_obj_center(l);
        title_x = 48;
    }

    h.title = make_label(h.root, title, &font_ms_20);
    lv_obj_align(h.title, LV_ALIGN_LEFT_MID, title_x, 0);
    lv_label_set_long_mode(h.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(h.title, 300 - title_x);

    h.clock = make_label(h.root, clock_text(), &font_ms_20, COL_MUTED);
    lv_obj_align(h.clock, LV_ALIGN_RIGHT_MID, 0, 0);
    return h;
}

lv_obj_t *make_big_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, uint32_t color)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, lv_pct(100), 56);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &font_ms_24, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x101010), 0);
    lv_obj_center(l);
    return btn;
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

namespace {

lv_obj_t *modal = nullptr;
void (*modal_ok)() = nullptr;

void modal_close()
{
    if (modal) lv_obj_delete(modal);
    modal = nullptr;
    modal_ok = nullptr;
}

void modal_cancel_cb(lv_event_t *) { modal_close(); }
void modal_ok_cb(lv_event_t *)
{
    void (*cb)() = modal_ok;
    modal_close();
    if (cb) cb();
}

}  // namespace

void confirm(const char *title, const char *text, const char *ok_text, void (*on_ok)())
{
    modal_close();
    modal_ok = on_ok;

    // Abdunkeln und alle Klicks darunter schlucken.
    modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_60, 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_set_style_radius(modal, 0, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);
    lv_obj_set_scrollable(modal, false);

    lv_obj_t *box = lv_obj_create(modal);
    lv_obj_set_size(box, 400, LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_style_pad_row(box, 12, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(box, false);

    make_label(box, title, &font_ms_28);
    lv_obj_t *t = make_label(box, text, &font_ms_20, COL_MUTED);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(t, 360);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *row = lv_obj_create(box);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_scrollable(row, false);

    lv_obj_t *back = make_big_button(row, "Zurück", modal_cancel_cb, COL_BG);
    lv_obj_set_flex_grow(back, 1);
    lv_obj_set_style_text_color(lv_obj_get_child(back, 0), lv_color_hex(COL_TEXT), 0);
    lv_obj_t *ok = make_big_button(row, ok_text, modal_ok_cb, COL_ALARM);
    lv_obj_set_flex_grow(ok, 1);
    lv_obj_set_style_text_color(lv_obj_get_child(ok, 0), lv_color_hex(COL_TEXT), 0);
}

const char *clock_text()
{
    static char buf[8];
    if (!session::time_valid()) return "--:--";
    const time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, sizeof buf, "%H:%M", &tm);
    return buf;
}

}  // namespace ui
