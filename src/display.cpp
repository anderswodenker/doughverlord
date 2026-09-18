#include "display.hpp"

#include <lvgl.h>

#include "LGFX_WT32SC01Plus.hpp"

namespace {

LGFX_WT32SC01Plus lcd;

// Querformat: das Panel ist physisch 320x480, gedreht wird daraus 480x320.
constexpr int32_t SCREEN_W = 480;
constexpr int32_t SCREEN_H = 320;

// Teilbuffer ueber 40 Zeilen -- LVGL zeichnet stueckweise und flusht haeufiger.
constexpr uint32_t BUF_LINES = 40;
lv_color_t draw_buf[SCREEN_W * BUF_LINES];

uint8_t  current_brightness = 200;
uint32_t last_touch_ms      = 0;

uint32_t tick_cb(void) { return millis(); }

void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
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

void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    uint16_t x, y;
    if (lcd.getTouch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
        last_touch_ms = millis();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

namespace display {

void begin()
{
    lcd.init();
    lcd.setRotation(1);         // Querformat, USB-Buchse links
    lcd.setBrightness(current_brightness);
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

    last_touch_ms = millis();
}

void set_brightness(uint8_t v)
{
    if (v == current_brightness) return;
    current_brightness = v;
    lcd.setBrightness(v);
}

uint8_t  brightness() { return current_brightness; }
uint32_t idle_ms()    { return millis() - last_touch_ms; }

}  // namespace display
