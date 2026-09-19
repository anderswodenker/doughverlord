// Gemeinsame Bausteine der Screens: Farben, Fonts, Kopf- und Fusszeile.
#pragma once

#include <lvgl.h>
#include <time.h>

#include "../fonts/fonts.hpp"

namespace ui {

// Dunkel, weil das Geraet nachts in der Kueche steht.
constexpr uint32_t COL_BG      = 0x101418;
constexpr uint32_t COL_PANEL   = 0x1A2028;
constexpr uint32_t COL_TEXT    = 0xE6E6E6;
constexpr uint32_t COL_MUTED   = 0x8A929A;
constexpr uint32_t COL_ACCENT  = 0xE0A040;   // warmes Gelb, Brotkruste
constexpr uint32_t COL_ALARM   = 0xD8402A;
constexpr uint32_t COL_OK      = 0x5CB85C;

constexpr int32_t HEADER_H = 40;
constexpr int32_t FOOTER_H = 44;

// Leerer, dunkler Screen ohne Rand und Scrollbalken.
lv_obj_t *make_screen();

// Kopfzeile, auf allen Screens gleich: links optional ein Knopf (Zurueck
// oder Abbruch), dann der Titel; rechts Verbrauch, WLAN-Symbol und Uhr.
// Navigation und Information in einer Zeile. refresh_header() fuehrt die
// rechte Seite nach -- aus dem 1-Hz-Tick jedes Screens aufrufen.
struct Header { lv_obj_t *root; lv_obj_t *title; lv_obj_t *clock; lv_obj_t *wifi; lv_obj_t *watts; lv_obj_t *left_btn; };
Header make_header(lv_obj_t *scr, const char *title, bool back_button);
void   refresh_header(const Header &h);
// Alle Texte der Kopfzeile in einer Farbe (Alarm: weiss auf rot).
void   tint_header(const Header &h, uint32_t color);

// Kraeftiger Knopf ueber die volle Breite.
lv_obj_t *make_big_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, uint32_t color = COL_ACCENT);

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color = COL_TEXT);

// "21:14" oder "--:--" bei ungueltiger Uhr.
const char *clock_text();

// Rueckfrage als Overlay ueber dem aktiven Screen: Titel, Text, ein roter
// Knopf mit ok_text (ruft on_ok) und "Zurueck" (schliesst nur).
void confirm(const char *title, const char *text, const char *ok_text, void (*on_ok)());

}  // namespace ui
