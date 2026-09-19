// Screen 3: der laufende Timer. Die Restzeit traegt den Screen, Kontext in
// der Kopfzeile (Rezept, Schritt) und unten nur das Ziel: die Endzeit.
// Das X links oben bricht das Backen ab (mit Rueckfrage) und fuehrt zum Dashboard.
#include "../config.hpp"
#include "../net.hpp"
#include "../session.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t *scr        = nullptr;
ui::Header header{};
lv_obj_t *raffer     = nullptr;
lv_obj_t *step_name  = nullptr;
lv_obj_t *big        = nullptr;   // Restzeit: Zeile aus Einzelzeichen
lv_obj_t *big_cell[5] = {};       // je ein Label pro Zeichen, feste Zellbreite
int32_t   digit_w    = 0;         // breiteste Ziffer der 96er
int32_t   colon_w    = 0;
lv_obj_t *big_unit   = nullptr;   // "Stunden : Minuten" o. ae.
lv_obj_t *lbl_word       = nullptr;   // Text statt Ziffern (48): "Erledigt?", "Brot fertig"
lv_obj_t *hint       = nullptr;
lv_obj_t *bar        = nullptr;
lv_obj_t *lbl_round      = nullptr;
lv_obj_t *btn        = nullptr;
lv_obj_t *btn_label  = nullptr;
lv_obj_t *eta        = nullptr;   // Fusszeile: Symbol + Endzeit, sonst nichts

void confirm_cb(lv_event_t *)
{
    if (session::state() == session::State::Done) {
        session::abort();       // raeumt den fertigen Teig weg
        ui::show_home();
        return;
    }
    session::confirm();
    ui::timer::refresh();
}

void abort_ok()
{
    session::abort();
    ui::show_home();
}

// Der Teig laeuft Stunden -- ein versehentlicher Tipp darf ihn nicht loeschen.
void abort_cb(lv_event_t *)
{
    ui::confirm("Backen abbrechen?", "Der laufende Teig wird verworfen. Das lässt sich nicht rückgängig machen.",
                "Verwerfen", abort_ok);
}

void center_cb(lv_event_t *)
{
    // Schrittliste als Overlay kommt in Schritt 7; bis dahin: Zutaten.
    const recipe::Recipe *r = session::current_recipe();
    if (r) ui::show_ingredients(*r, false);
}

void build()
{
    scr = ui::make_screen();
    header = ui::make_header(scr, "", true);
    lv_label_set_text(lv_obj_get_child(header.left_btn, 0), LV_SYMBOL_CLOSE);
    lv_obj_add_event_cb(header.left_btn, abort_cb, LV_EVENT_CLICKED, nullptr);

    // Mittelteil: eine Spalte, alles zentriert. Tippen oeffnet die Zutaten.
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, lv_pct(100), 320 - ui::HEADER_H - ui::FOOTER_H);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, ui::HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_ver(body, 4, 0);
    lv_obj_set_style_pad_row(body, 2, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(body, false);
    lv_obj_add_event_cb(body, center_cb, LV_EVENT_CLICKED, nullptr);

    // Zeitraffer deutlich markieren, damit niemand den Modus mit echtem Teig benutzt.
    raffer    = ui::make_label(body, "", &font_ms_20, ui::COL_ALARM);
    step_name = ui::make_label(body, "", &font_ms_28);
    // Montserrat hat proportionale Ziffern (die 1 ist schmal) -- damit die
    // Stellen nicht wandern, bekommt jedes Zeichen eine Zelle fester Breite.
    for (char c = '0'; c <= '9'; c++)
        digit_w = LV_MAX(digit_w, (int32_t)lv_font_get_glyph_width(&font_ms_96, c, 0));
    colon_w = lv_font_get_glyph_width(&font_ms_96, ':', 0);

    big = lv_obj_create(body);
    lv_obj_set_size(big, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(big, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(big, 0, 0);
    lv_obj_set_style_pad_all(big, 0, 0);
    lv_obj_set_flex_flow(big, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(big, -4, 0);   // Zellen etwas ueberlappen lassen: enger, wie eine Uhr
    lv_obj_set_scrollable(big, false);
    for (lv_obj_t *&cell : big_cell) {
        cell = ui::make_label(big, "", &font_ms_96);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    }
    big_unit  = ui::make_label(body, "", &font_ms_16, ui::COL_MUTED);
    lbl_word      = ui::make_label(body, "", &font_ms_48, ui::COL_ACCENT);
    hint      = ui::make_label(body, "", &font_ms_20, ui::COL_MUTED);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(hint, 440);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    bar = lv_bar_create(body);
    lv_obj_set_size(bar, 400, 10);
    lv_bar_set_range(bar, 0, 1000);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A323C), LV_PART_MAIN);   // heller als das Panel, sonst unsichtbar
    lv_obj_set_style_bg_color(bar, lv_color_hex(ui::COL_ACCENT), LV_PART_INDICATOR);

    lbl_round = ui::make_label(body, "", &font_ms_20, ui::COL_MUTED);

    btn = ui::make_big_button(body, "Erledigt", confirm_cb);
    lv_obj_set_width(btn, 300);
    btn_label = lv_obj_get_child(btn, 0);

    // Fusszeile
    lv_obj_t *foot = lv_obj_create(scr);
    lv_obj_set_size(foot, lv_pct(100), ui::FOOTER_H);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(foot, lv_color_hex(ui::COL_PANEL), 0);
    lv_obj_set_style_border_width(foot, 0, 0);
    lv_obj_set_style_radius(foot, 0, 0);
    lv_obj_set_style_pad_hor(foot, 12, 0);
    lv_obj_set_style_pad_ver(foot, 0, 0);
    lv_obj_set_scrollable(foot, false);

    // Nur ein Zielsymbol und die Endzeit, mittig -- kein Text.
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(foot, 10, 0);
    ui::make_label(foot, LV_SYMBOL_GPS, LV_FONT_DEFAULT, ui::COL_MUTED);
    eta = ui::make_label(foot, "", &font_ms_24);
}

// Setzt die grosse Restzeit zeichenweise in die festen Zellen.
void set_big(const char *text)
{
    for (size_t i = 0; i < 5; i++) {
        lv_obj_t *cell = big_cell[i];
        if (i < strlen(text)) {
            const char t[2] = { text[i], 0 };
            lv_label_set_text(cell, t);
            lv_obj_set_width(cell, text[i] == ':' ? colon_w : digit_w);
            lv_obj_set_hidden(cell, false);
        } else {
            lv_obj_set_hidden(cell, true);
        }
    }
}

// Voraussichtliches Ende als Uhrzeit; "+1" wenn es erst morgen ist.
String eta_text()
{
    const time_t t = session::eta_end();
    if (!t) return "--:--";
    const time_t now = time(nullptr);
    struct tm tn, te;
    localtime_r(&now, &tn);
    localtime_r(&t, &te);
    char buf[16];
    strftime(buf, sizeof buf, "%H:%M", &te);
    String s = buf;
    if (te.tm_yday != tn.tm_yday) s += " +1";
    return s;
}

}  // namespace

namespace ui::timer {

void show()
{
    if (!scr) build();
    lv_screen_load(scr);
    refresh();
}

void ask_abort() { abort_cb(nullptr); }

void refresh()
{
    if (!scr || lv_screen_active() != scr) return;

    ui::refresh_header(header);
    if (session::time_factor() > 1) {
        lv_label_set_text_fmt(raffer, "ZEITRAFFER ×%lu", (unsigned long)session::time_factor());
    } else {
        lv_label_set_text(raffer, "");
    }

    const recipe::Recipe *r = session::current_recipe();
    const recipe::Step   *s = session::current_step();
    const session::State  st = session::state();

    // Alles ausblenden, dann je nach Zustand einblenden.
    for (lv_obj_t *o : { big, big_unit, lbl_word, hint, bar, lbl_round, btn }) lv_obj_set_hidden(o, true);

    if (st == session::State::Done) {
        lv_label_set_text(header.title, "Fertig");
        lv_label_set_text(step_name, "");
        lv_label_set_text(lbl_word, "Brot fertig!");
        lv_obj_set_style_text_color(lbl_word, lv_color_hex(ui::COL_OK), 0);
        lv_obj_set_hidden(lbl_word, false);
        lv_label_set_text(btn_label, "Neues Brot");
        lv_obj_set_hidden(btn, false);
        lv_label_set_text(eta, "");
        return;
    }
    if (!r || !s) { lv_label_set_text(header.title, "Kein Teig"); return; }

    lv_label_set_text_fmt(header.title, "%s · Schritt %u/%u", r->name.c_str(),
                          session::step_index() + 1, (unsigned)r->schritte.size());
    lv_label_set_text(step_name, s->name.c_str());
    lv_label_set_text(btn_label, "Erledigt");

    if (s->hinweis.length()) {
        lv_label_set_text(hint, s->hinweis.c_str());
        lv_obj_set_hidden(hint, false);
    }
    if (s->runden > 1) {
        lv_label_set_text_fmt(lbl_round, "Runde %u von %u", session::round_index() + 1, s->runden);
        lv_obj_set_hidden(lbl_round, false);
    }

    if (st == session::State::Waiting) {
        lv_label_set_text(lbl_word, s->offen ? "Erledigt?" : "Zeit ist um");
        lv_obj_set_style_text_color(lbl_word, lv_color_hex(s->offen ? ui::COL_ACCENT : ui::COL_ALARM), 0);
        lv_obj_set_hidden(lbl_word, false);
        lv_obj_set_hidden(btn, false);
    } else {
        const int32_t rest = session::remaining_s();
        if (rest < 0) {
            lv_label_set_text(lbl_word, "Restzeit unbekannt");
            lv_obj_set_style_text_color(lbl_word, lv_color_hex(ui::COL_MUTED), 0);
            lv_obj_set_hidden(lbl_word, false);
        } else {
            char t[8];
            if (rest >= 3600) {
                snprintf(t, sizeof t, "%ld:%02ld", (long)(rest / 3600), (long)((rest % 3600) / 60));
                lv_label_set_text(big_unit, "Stunden : Minuten");
            } else {
                snprintf(t, sizeof t, "%02ld:%02ld", (long)(rest / 60), (long)(rest % 60));
                lv_label_set_text(big_unit, "Minuten : Sekunden");
            }
            set_big(t);
            lv_obj_set_hidden(big, false);
            lv_obj_set_hidden(big_unit, false);
            const uint32_t d = session::step_duration_s();
            if (d) {
                lv_bar_set_value(bar, (int32_t)(1000 - (int64_t)rest * 1000 / d), LV_ANIM_OFF);
                lv_obj_set_hidden(bar, false);
            }
        }
    }

    lv_label_set_text(eta, eta_text().c_str());
}

}  // namespace ui::timer
