// Startbildschirm ohne laufenden Teig: Uhrzeit und Datum gross, darunter
// der aktuelle Hausverbrauch vom Stromzaehler, unten der Knopf "Backen"
// zur Rezeptauswahl.
#include <time.h>

#include "../net.hpp"
#include "../session.hpp"
#include "../strom.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t *scr       = nullptr;
lv_obj_t *lbl_clock = nullptr;
lv_obj_t *lbl_date  = nullptr;
lv_obj_t *lbl_watts = nullptr;
lv_obj_t *lbl_info  = nullptr;
lv_obj_t *wifi      = nullptr;

const char *const WEEKDAYS[] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" };
const char *const MONTHS[]   = { "Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                                 "August", "September", "Oktober", "November", "Dezember" };

void bake_cb(lv_event_t *) { ui::show_select(); }

void build()
{
    scr = ui::make_screen();

    // Eine Spalte, alles zentriert: Uhr, Datum, Verbrauch, Knopf.
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_ver(body, 8, 0);
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(body, false);

    lbl_clock = ui::make_label(body, "--:--", &font_ms_96);
    lbl_date  = ui::make_label(body, "", &font_ms_28, ui::COL_MUTED);

    lv_obj_t *panel = lv_obj_create(body);
    lv_obj_set_size(panel, 440, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(ui::COL_PANEL), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_set_style_pad_row(panel, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(panel, false);
    lbl_watts = ui::make_label(panel, "", &font_ms_48, ui::COL_ACCENT);
    lbl_info  = ui::make_label(panel, "", &font_ms_16, ui::COL_MUTED);

    lv_obj_t *btn = ui::make_big_button(body, "Backen", bake_cb);
    lv_obj_set_width(btn, 300);

    wifi = ui::make_label(scr, LV_SYMBOL_WIFI, LV_FONT_DEFAULT, ui::COL_MUTED);
    lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -12, 10);
}

}  // namespace

namespace ui::home {

void show()
{
    if (!scr) build();
    lv_screen_load(scr);
    refresh();   // erst nach dem Laden, sonst greift die Aktiv-Pruefung
}

void refresh()
{
    if (!scr || lv_screen_active() != scr) return;

    if (session::time_valid()) {
        const time_t t = time(nullptr);
        struct tm tm;
        localtime_r(&t, &tm);
        char buf[48];
        strftime(buf, sizeof buf, "%H:%M", &tm);
        lv_label_set_text(lbl_clock, buf);
        snprintf(buf, sizeof buf, "%s, %d. %s %d", WEEKDAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon], tm.tm_year + 1900);
        lv_label_set_text(lbl_date, buf);
    } else {
        lv_label_set_text(lbl_clock, "--:--");
        lv_label_set_text(lbl_date, net::wifi_connected() ? "Uhr wird gestellt …" : "Kein WLAN – Uhr nicht gestellt");
    }

    if (!strom::configured()) {
        lv_label_set_text(lbl_watts, "– W");
        lv_label_set_text(lbl_info, "Stromzähler nicht konfiguriert – mqtt in config.json");
        lv_obj_set_style_text_color(lbl_watts, lv_color_hex(ui::COL_MUTED), 0);
    } else if (!strom::valid()) {
        lv_label_set_text(lbl_watts, "– W");
        lv_label_set_text(lbl_info, strom::connected() ? "Warte auf Zählerwert …" : "Kein Kontakt zum Stromzähler");
        lv_obj_set_style_text_color(lbl_watts, lv_color_hex(ui::COL_MUTED), 0);
    } else {
        char buf[64];
        snprintf(buf, sizeof buf, "%d W", strom::watts());
        lv_label_set_text(lbl_watts, buf);
        snprintf(buf, sizeof buf, "Zählerstand %.1f kWh · vor %lu s", strom::kwh(), (unsigned long)strom::age_s());
        lv_label_set_text(lbl_info, buf);
        lv_obj_set_style_text_color(lbl_watts, lv_color_hex(ui::COL_ACCENT), 0);
    }

    lv_obj_set_style_text_color(wifi, lv_color_hex(net::wifi_connected() ? ui::COL_MUTED : ui::COL_ALARM), 0);
}

}  // namespace ui::home
