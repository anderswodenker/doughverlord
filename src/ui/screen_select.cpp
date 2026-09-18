// Screen 1: Rezeptauswahl -- die JSON-Dateien von der Karte, gross genug
// zum Antippen, mit Schrittzahl und Timerzeit als Vorschau.
#include <vector>

#include "../net.hpp"
#include "../sd_card.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t   *scr    = nullptr;
lv_obj_t   *lbl_clock  = nullptr;
lv_obj_t   *status = nullptr;
std::vector<String> paths;

void item_cb(lv_event_t *e)
{
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (i >= paths.size()) return;

    recipe::Recipe r;
    String err;
    if (!recipe::load(paths[i].c_str(), r, err)) {
        Serial.printf("[ui] %s: %s\n", paths[i].c_str(), err.c_str());
        lv_label_set_text(status, err.c_str());
        lv_obj_set_style_text_color(status, lv_color_hex(ui::COL_ALARM), 0);
        return;
    }
    ui::show_ingredients(r, true);
}

void add_item(lv_obj_t *list, size_t index, const String &path)
{
    recipe::Recipe r;
    String err;
    const bool ok = recipe::load(path.c_str(), r, err);

    lv_obj_t *btn = lv_button_create(list);
    lv_obj_set_size(btn, lv_pct(100), 60);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ui::COL_PANEL), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_add_event_cb(btn, item_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);

    const String name = ok ? r.name : path.substring(path.lastIndexOf('/') + 1);
    lv_obj_t *l = ui::make_label(btn, name.c_str(), &font_ms_24, ok ? ui::COL_TEXT : ui::COL_MUTED);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    String info;
    if (ok) {
        info = String(r.schritte.size()) + " Schritte · " + recipe::format_duration(r.timer_gesamt_s());
    } else {
        info = err;
    }
    lv_obj_t *i = ui::make_label(btn, info.c_str(), &font_ms_16, ok ? ui::COL_MUTED : ui::COL_ALARM);
    lv_obj_align(i, LV_ALIGN_RIGHT_MID, 0, 0);
}

void build()
{
    scr = ui::make_screen();
    ui::Header h = ui::make_header(scr, "Rezept wählen", false);
    lbl_clock = h.clock;

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, lv_pct(100), 320 - ui::HEADER_H - 28);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, ui::HEADER_H);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 12, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    paths.clear();
    if (!sdcard::ready()) {
        ui::make_label(list, "Keine SD-Karte -- keine Rezepte", &font_ms_24, ui::COL_ALARM);
    } else {
        for (const String &n : recipe::list_files()) paths.push_back(String("/rezepte/") + n);
        if (paths.empty()) ui::make_label(list, "Keine Rezepte in /rezepte", &font_ms_24, ui::COL_MUTED);
        for (size_t i = 0; i < paths.size(); i++) add_item(list, i, paths[i]);
    }

    status = ui::make_label(scr, "", &font_ms_16, ui::COL_MUTED);
    lv_obj_align(status, LV_ALIGN_BOTTOM_LEFT, 12, -6);
}

}  // namespace

namespace ui::select {

void show()
{
    if (scr) lv_obj_delete(scr);   // Liste jedes Mal neu: die Karte kann sich geaendert haben
    build();
    refresh();
    lv_screen_load(scr);
}

void refresh()
{
    if (!scr || lv_screen_active() != scr) return;
    lv_label_set_text(lbl_clock, ui::clock_text());
    String s = net::wifi_connected() ? String("WLAN ") + net::ip() : String("kein WLAN");
    s += net::ntp_synced() ? " · Uhr per NTP" : " · Uhr nicht gestellt";
    lv_label_set_text(status, s.c_str());
    lv_obj_set_style_text_color(status, lv_color_hex(ui::COL_MUTED), 0);
}

}  // namespace ui::select
