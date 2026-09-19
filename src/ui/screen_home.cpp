// Startbildschirm ohne laufenden Teig: Datum in der Kopfzeile, Uhrzeit gross,
// darunter der Hausverbrauch der letzten 24 h als Linie (der Momentanwert
// steht schon im Header), unten der Knopf "Backen" zur Rezeptauswahl.
#include <time.h>

#include "../net.hpp"
#include "../session.hpp"
#include "../strom.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t  *scr       = nullptr;
ui::Header header{};
lv_obj_t  *lbl_clock = nullptr;
lv_obj_t  *chart     = nullptr;
lv_chart_series_t *series = nullptr;
lv_obj_t  *lbl_info  = nullptr;   // "24 h · max 3210 W" bzw. Stoerung
int32_t    chart_y[strom::HIST_N];
int16_t    hist[strom::HIST_N];
uint32_t   shown_samples = UINT32_MAX;

const char *const WEEKDAYS[] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" };
const char *const MONTHS[]   = { "Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                                 "August", "September", "Oktober", "November", "Dezember" };

void bake_cb(lv_event_t *) { ui::show_select(); }

void build()
{
    scr = ui::make_screen();
    header = ui::make_header(scr, "", false);

    // Eine Spalte, alles zentriert: Uhr, Verlauf, Knopf.
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, lv_pct(100), 320 - ui::HEADER_H);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, ui::HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_ver(body, 8, 0);
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(body, false);

    lbl_clock = ui::make_label(body, "--:--", &font_ms_96);

    // Verlauf: 1440 Minutenpunkte auf 440 px, Linie ohne Punktmarker,
    // ein leises Gitter (4 x 6 h waagerecht, 4 Stufen senkrecht).
    lv_obj_t *panel = lv_obj_create(body);
    lv_obj_set_size(panel, 440, 96);
    lv_obj_set_style_bg_color(panel, lv_color_hex(ui::COL_PANEL), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_scrollable(panel, false);

    chart = lv_chart_create(panel);
    lv_obj_set_size(chart, lv_pct(100), lv_pct(100));
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, strom::HIST_N);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart, 4, 5);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 10, 0);
    lv_obj_set_style_pad_all(chart, 6, 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x2A323C), 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);   // keine Punktmarker
    lv_obj_set_scrollable(chart, false);
    series = lv_chart_add_series(chart, lv_color_hex(ui::COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);
    for (int32_t &v : chart_y) v = LV_CHART_POINT_NONE;
    lv_chart_set_ext_y_array(chart, series, chart_y);

    lbl_info = ui::make_label(panel, "", &font_ms_16, ui::COL_MUTED);
    lv_obj_align(lbl_info, LV_ALIGN_TOP_LEFT, 10, 4);

    lv_obj_t *btn = ui::make_big_button(body, "Backen", bake_cb);
    lv_obj_set_width(btn, 300);
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

    ui::refresh_header(header);

    if (session::time_valid()) {
        const time_t t = time(nullptr);
        struct tm tm;
        localtime_r(&t, &tm);
        char buf[48];
        strftime(buf, sizeof buf, "%H:%M", &tm);
        lv_label_set_text(lbl_clock, buf);
        snprintf(buf, sizeof buf, "%s, %d. %s", WEEKDAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);
        lv_label_set_text(header.title, buf);
    } else {
        lv_label_set_text(lbl_clock, "--:--");
        lv_label_set_text(header.title, net::wifi_connected() ? "Uhr wird gestellt …" : "Kein WLAN – Uhr nicht gestellt");
    }

    // Verlauf nur neu zeichnen, wenn ein Messwert dazugekommen ist (alle ~15 s).
    if (strom::samples() != shown_samples) {
        shown_samples = strom::samples();
        strom::history(hist);
        int32_t peak = 0;
        size_t  have = 0;
        for (size_t i = 0; i < strom::HIST_N; i++) {
            chart_y[i] = hist[i] < 0 ? LV_CHART_POINT_NONE : hist[i];
            if (hist[i] > peak) peak = hist[i];
            if (hist[i] >= 0) have++;
        }
        // Skala auf die naechsten 500 W aufrunden, mindestens 1 kW
        const int32_t top = peak < 1000 ? 1000 : ((peak + 499) / 500) * 500;
        lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, top);
        lv_chart_refresh(chart);
        if (have) lv_label_set_text_fmt(lbl_info, "24 h · max %ld W", (long)peak);
    }

    // Stoerung statt Beschriftung, wenn etwas fehlt.
    if (!strom::configured())  lv_label_set_text(lbl_info, "Stromzähler nicht konfiguriert – mqtt in config.json");
    else if (!strom::valid())  lv_label_set_text(lbl_info, strom::connected() ? "Warte auf Zählerwert …" : "Kein Kontakt zum Stromzähler");
    else if (shown_samples == 0) lv_label_set_text(lbl_info, "24 h · Verlauf beginnt");
}

}  // namespace ui::home
