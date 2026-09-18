// WT32-SC01 Plus -- Grundgeruest: Display + Touch + LVGL 9 laufen lassen.
//
// Der Screen zeigt bewusst einen Farbtest (R/G/B) und die rohen Touch-
// Koordinaten an: damit laesst sich auf einen Blick pruefen, ob Farbreihen-
// folge und Touch-Achsen zur Rotation passen.

#include <Arduino.h>
#include <lvgl.h>

#include "LGFX_WT32SC01Plus.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "net.hpp"
#include "recipe.hpp"
#include "sd_card.hpp"
#include "session.hpp"

static LGFX_WT32SC01Plus lcd;

// Querformat: das Panel ist physisch 320x480, gedreht wird daraus 480x320.
static constexpr int32_t SCREEN_W = 480;
static constexpr int32_t SCREEN_H = 320;

// Teilbuffer ueber 40 Zeilen -- LVGL zeichnet stueckweise und flusht haeufiger.
static constexpr uint32_t BUF_LINES = 40;
static lv_color_t draw_buf[SCREEN_W * BUF_LINES];

static lv_obj_t *lbl_touch  = nullptr;
static lv_obj_t *lbl_clicks = nullptr;
static uint32_t  click_count = 0;

// ---------------------------------------------------------------- LVGL-Glue

static uint32_t tick_cb(void) { return millis(); }

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t w = lv_area_get_width(area);
    const int32_t h = lv_area_get_height(area);

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    // rgb565_t hat dasselbe Speicherlayout wie lv_color16_t; LovyanGFX
    // uebernimmt das Byte-Swapping zum Panel hin.
    lcd.writePixels(reinterpret_cast<lgfx::rgb565_t *>(px_map), w * h);
    lcd.endWrite();

    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    if (lcd.getTouch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// -------------------------------------------------------------------- UI

static void btn_clicked_cb(lv_event_t *e)
{
    (void)e;
    click_count++;
    lv_label_set_text_fmt(lbl_clicks, "Klicks: %lu", (unsigned long)click_count);
    Serial.printf("[ui] Button geklickt (%lu)\n", (unsigned long)click_count);
}

static void slider_bl_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    const int32_t v = lv_slider_get_value(slider);
    lcd.setBrightness((uint8_t)v);
}

// Ein Farbbalken mit Beschriftung -- zum Pruefen der Farbreihenfolge.
static void make_color_bar(lv_obj_t *parent, const char *text, uint32_t rgb)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 90, 44);
    lv_obj_set_style_bg_color(bar, lv_color_hex(rgb), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_center(lbl);
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "WT32-SC01 Plus  -  LVGL " LVGL_VERSION_INFO);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Farbtest: muss von links nach rechts rot, gruen, blau, weiss zeigen.
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, 400, 60);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    make_color_bar(row, "ROT",   0xFF0000);
    make_color_bar(row, "GRUEN", 0x00FF00);
    make_color_bar(row, "BLAU",  0x0000FF);
    make_color_bar(row, "WEISS", 0xFFFFFF);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 160, 56);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 30, 30);
    lv_obj_add_event_cb(btn, btn_clicked_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Antippen");
    lv_obj_center(btn_lbl);

    lbl_clicks = lv_label_create(scr);
    lv_label_set_text(lbl_clicks, "Klicks: 0");
    lv_obj_set_style_text_color(lbl_clicks, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
    lv_obj_align(lbl_clicks, LV_ALIGN_LEFT_MID, 30, 80);

    lbl_touch = lv_label_create(scr);
    lv_label_set_text(lbl_touch, "Touch: --");
    lv_obj_set_style_text_color(lbl_touch, lv_color_hex(0x8AB4F8), LV_PART_MAIN);
    lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_LEFT, 30, -16);

    lv_obj_t *sl_lbl = lv_label_create(scr);
    lv_label_set_text(sl_lbl, "Helligkeit");
    lv_obj_set_style_text_color(sl_lbl, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
    lv_obj_align(sl_lbl, LV_ALIGN_RIGHT_MID, -40, 10);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 180);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -25, 45);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, 200, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_bl_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ------------------------------------------------------------------ Setup

static void log_board_info(void)
{
    Serial.printf("[sys] Chip:  %s rev %d, %d Kerne @ %lu MHz\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                  (unsigned long)getCpuFrequencyMhz());
    Serial.printf("[sys] Flash: %lu Bytes\n", (unsigned long)ESP.getFlashChipSize());
    Serial.printf("[sys] PSRAM: %lu Bytes (frei: %lu)\n",
                  (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram());
    Serial.printf("[sys] Heap:  %lu Bytes frei\n", (unsigned long)ESP.getFreeHeap());
}

void setup()
{
    Serial.begin(115200);
    // Der native USB des S3 enumeriert erst nach dem Boot neu. Kurz auf einen
    // angehaengten Monitor warten, sonst verpufft der Startbanner ins Leere.
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) delay(10);
    delay(200);
    Serial.println("\n=== WT32-SC01 Plus startet ===");
    log_board_info();

    lcd.init();
    lcd.setRotation(1);         // Querformat, USB-Buchse links
    lcd.setBrightness(200);
    Serial.printf("[lcd] init ok: %dx%d\n", lcd.width(), lcd.height());

    lv_init();
    lv_tick_set_cb(tick_cb);

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    build_ui();
    Serial.println("[lvgl] UI aufgebaut");

    // SD-Karte erst nach dem Display: laeuft die Karte nicht, soll das
    // Geraet trotzdem etwas anzeigen und der Heartbeat weiterlaufen.
    if (sdcard::begin()) {
        sdcard::ensure_example_recipe();
        sdcard::list("/");
    }

#ifdef RECIPE_SELFTEST
    recipe::selftest();
#endif

    // Bis die Rezeptauswahl steht: alle Rezepte einmal einlesen und loggen,
    // damit der Parser am echten Geraet gegen die Handrechnung gehalten wird.
    for (const String &name : recipe::list_files()) {
        recipe::Recipe r;
        String err;
        const String path = String("/rezepte/") + name;
        if (recipe::load(path.c_str(), r, err)) recipe::log(r);
        else Serial.printf("[rezept] %s: %s\n", path.c_str(), err.c_str());
    }

    // Reihenfolge: Konfiguration -> Netz (setzt die Zeitzone) -> Sitzung.
    const config::Config &cfg = config::load();
    net::begin(cfg.wifi_ssid, cfg.wifi_pass);
    session::set_time_factor(cfg.zeitraffer);
    session::begin();
    session::log_status();
}

void loop()
{
    lv_timer_handler();
    cli::poll();
    session::tick();

    // Ereignisse vorerst nur loggen; Alarm-Screen und Push kommen in Schritt 6.
    switch (session::take_event()) {
        case session::Event::Expired:  Serial.println("[ereignis] Alarm");   break;
        case session::Event::Finished: Serial.println("[ereignis] Fertig");  break;
        default: break;
    }

    // Lebenszeichen: ein spaeter angehaengter Monitor sieht sonst gar nichts,
    // weil der Startbanner beim USB-Reset schon durch ist.
    static uint32_t last_beat = 0;
    if (millis() - last_beat > 3000) {
        last_beat = millis();
        uint16_t tx, ty;
        const bool touched = lcd.getTouch(&tx, &ty);
        Serial.printf("[beat] t=%lus  heap=%lu  psram=%lu  klicks=%lu  touch=%s  sd=%s  %s\n",
                      (unsigned long)(millis() / 1000),
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getFreePsram(),
                      (unsigned long)click_count,
                      touched ? "ja" : "nein",
                      sdcard::ready() ? "ok" : "fehlt",
                      net::status_line().c_str());
        if (session::active()) session::log_status();
    }

    // Rohe Touch-Koordinaten anzeigen (unabhaengig von LVGL-Widgets).
    static uint32_t last = 0;
    if (millis() - last > 100) {
        last = millis();
        uint16_t x, y;
        if (lcd.getTouch(&x, &y)) {
            lv_label_set_text_fmt(lbl_touch, "Touch: x=%u  y=%u", x, y);
        }
    }

    delay(5);
}
