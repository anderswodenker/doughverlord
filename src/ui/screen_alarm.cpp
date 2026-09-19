// Screen 4: der Alarm. Ein Timer ist abgelaufen -- Schrittname gross und
// pulsierend, darunter was jetzt ansteht (naechste Runde oder naechster
// Schritt mit seinen Zutaten und dem Hinweis), "Erledigt" ueber die volle
// Breite. Bleibt stehen, bis bestaetigt wird.
#include "../session.hpp"
#include "common.hpp"
#include "screens.hpp"
#include "ui.hpp"

namespace {

lv_obj_t *scr       = nullptr;
ui::Header header{};
lv_obj_t *lbl_step  = nullptr;
lv_obj_t *lbl_now   = nullptr;
lv_obj_t *lbl_items = nullptr;
lv_obj_t *lbl_hint  = nullptr;
lv_anim_t pulse;

void confirm_cb(lv_event_t *)
{
    session::confirm();
    ui::show_timer();   // ui::tick() schaltet ohnehin um; so geht es ohne Verzoegerung
}

void pulse_cb(void *obj, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

void build()
{
    scr = ui::make_screen();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x3A1410), 0);   // dunkles Rot: faellt auf, blendet nachts nicht
    header = ui::make_header(scr, "Zeit ist um", false);
    lv_obj_set_style_bg_color(header.root, lv_color_hex(ui::COL_ALARM), 0);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_size(body, lv_pct(100), 320 - ui::HEADER_H - 72);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, ui::HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_ver(body, 4, 0);
    lv_obj_set_style_pad_row(body, 4, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(body, false);

    lbl_step = ui::make_label(body, "", &font_ms_48, ui::COL_ACCENT);
    lv_label_set_long_mode(lbl_step, LV_LABEL_LONG_MODE_WRAP);   // lange Namen auf zwei Zeilen
    lv_obj_set_width(lbl_step, 448);
    lv_obj_set_style_text_align(lbl_step, LV_TEXT_ALIGN_CENTER, 0);

    lbl_now = ui::make_label(body, "", &font_ms_28);
    lv_label_set_long_mode(lbl_now, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(lbl_now, 448);
    lv_obj_set_style_text_align(lbl_now, LV_TEXT_ALIGN_CENTER, 0);

    lbl_items = ui::make_label(body, "", &font_ms_24, ui::COL_ACCENT);
    lv_label_set_long_mode(lbl_items, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(lbl_items, 448);
    lv_obj_set_style_text_align(lbl_items, LV_TEXT_ALIGN_CENTER, 0);

    lbl_hint = ui::make_label(body, "", &font_ms_20, ui::COL_MUTED);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(lbl_hint, 448);
    lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *btn = ui::make_big_button(scr, "Erledigt", confirm_cb);
    lv_obj_set_size(btn, 448, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Pulsieren: der Schrittname atmet zwischen 45 % und 100 % Deckung.
    lv_anim_init(&pulse);
    lv_anim_set_var(&pulse, lbl_step);
    lv_anim_set_exec_cb(&pulse, pulse_cb);
    lv_anim_set_values(&pulse, LV_OPA_100, LV_OPA_40);
    lv_anim_set_duration(&pulse, 700);
    lv_anim_set_playback_duration(&pulse, 700);
    lv_anim_set_repeat_count(&pulse, LV_ANIM_REPEAT_INFINITE);
}

}  // namespace

namespace ui::alarm {

// Was nach "Erledigt" ansteht -- als Ueberschrift, Zutaten und Hinweis.
void describe_next(String &head, String &items, String &hint)
{
    head = items = hint = "";
    const recipe::Recipe *r = session::current_recipe();
    const recipe::Step   *s = session::current_step();
    if (!r || !s) return;
    if (session::round_index() + 1 < s->runden) {
        head = String("Runde ") + (session::round_index() + 1) + " von " + s->runden
             + " vorbei · Jetzt: Runde " + (session::round_index() + 2);
        return;
    }
    const uint16_t i = session::step_index();
    if (i + 1 >= r->schritte.size()) { head = "Das war der letzte Schritt"; return; }
    const recipe::Step &n = r->schritte[i + 1];
    head = String("Jetzt: ") + n.name;
    for (const recipe::Ingredient &z : n.zutaten) {
        if (items.length()) items += "  ·  ";
        items += z.menge + " " + z.was;
    }
    hint = n.hinweis;
}

void show()
{
    if (!scr) build();
    lv_screen_load(scr);
    lv_anim_start(&pulse);
    refresh();
}

void refresh()
{
    if (!scr || lv_screen_active() != scr) return;
    ui::refresh_header(header);
    ui::tint_header(header, 0xFFFFFF);

    const recipe::Recipe *r = session::current_recipe();
    const recipe::Step   *s = session::current_step();
    if (!r || !s) { lv_label_set_text(lbl_step, "Kein Teig"); return; }

    lv_label_set_text_fmt(header.title, "Zeit ist um · %s", r->name.c_str());
    lv_label_set_text(lbl_step, s->name.c_str());

    String head, items, hint;
    describe_next(head, items, hint);
    lv_label_set_text(lbl_now, head.c_str());
    lv_label_set_text(lbl_items, items.c_str());
    lv_label_set_text(lbl_hint, hint.c_str());
    lv_obj_set_hidden(lbl_items, items.isEmpty());
    lv_obj_set_hidden(lbl_hint, hint.isEmpty());
}

}  // namespace ui::alarm
