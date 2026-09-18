// Screen 2: Zutaten -- Mengen rechtsbuendig untereinander, Schritt-Zutaten
// mit Vermerk. Vor dem Start mit dem Knopf "Zusammengemischt -- los",
// waehrend des Ablaufs nur zum Nachschlagen.
#include "../session.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t      *scr     = nullptr;
lv_obj_t      *lbl_clock   = nullptr;
lv_obj_t      *msg     = nullptr;
recipe::Recipe current;
bool           can_start = false;

void back_cb(lv_event_t *)
{
    if (can_start) ui::show_select();
    else           ui::show_timer();
}

void start_cb(lv_event_t *)
{
    String err;
    if (!session::start(current.datei.c_str(), err)) {
        Serial.printf("[ui] start: %s\n", err.c_str());
        lv_label_set_text(msg, err.c_str());
        return;
    }
    ui::show_timer();
}

void add_row(lv_obj_t *list, const recipe::Ingredient &z, const char *vermerk)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_scrollable(row, false);

    lv_obj_t *menge = ui::make_label(row, z.menge.c_str(), &font_ms_24, ui::COL_ACCENT);
    lv_obj_set_width(menge, 110);
    lv_obj_set_style_text_align(menge, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(menge, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *was = ui::make_label(row, z.was.c_str(), &font_ms_24);
    lv_obj_align(was, LV_ALIGN_LEFT_MID, 126, 0);

    if (vermerk) {
        lv_obj_t *v = ui::make_label(row, vermerk, &font_ms_16, ui::COL_MUTED);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void build()
{
    scr = ui::make_screen();
    const String title = current.name + " · Zutaten";
    ui::Header h = ui::make_header(scr, title.c_str(), true);
    lbl_clock = h.clock;
    lv_obj_add_event_cb(h.left_btn, back_cb, LV_EVENT_CLICKED, nullptr);

    const int32_t footer = can_start ? 76 : 0;
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, lv_pct(100), 320 - ui::HEADER_H - footer);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, ui::HEADER_H);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_hor(list, 16, 0);
    lv_obj_set_style_pad_ver(list, 8, 0);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    for (const recipe::Ingredient &z : current.zutaten) add_row(list, z, nullptr);
    for (const recipe::Step &s : current.schritte) {
        if (s.zutaten.empty()) continue;
        const String v = String("bei „") + s.name + "“";
        for (const recipe::Ingredient &z : s.zutaten) add_row(list, z, v.c_str());
    }
    if (current.zutaten.empty()) ui::make_label(list, "Keine Zutaten im Rezept", &font_ms_20, ui::COL_MUTED);

    if (can_start) {
        lv_obj_t *box = lv_obj_create(scr);
        lv_obj_set_size(box, lv_pct(100), footer);
        lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_style_pad_hor(box, 16, 0);
        lv_obj_set_style_pad_ver(box, 10, 0);
        lv_obj_set_scrollable(box, false);
        ui::make_big_button(box, "Zusammengemischt — los", start_cb);

        msg = ui::make_label(scr, "", &font_ms_16, ui::COL_ALARM);
        lv_obj_align(msg, LV_ALIGN_BOTTOM_LEFT, 16, -80);
    }
}

}  // namespace

namespace ui::ingredients {

void show(const recipe::Recipe &r, bool startable)
{
    current   = r;
    can_start = startable;
    if (scr) lv_obj_delete(scr);
    build();
    refresh();
    lv_screen_load(scr);
}

void refresh()
{
    if (!scr || lv_screen_active() != scr) return;
    lv_label_set_text(lbl_clock, ui::clock_text());
}

}  // namespace ui::ingredients
