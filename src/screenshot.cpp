#include "screenshot.hpp"

#include <Arduino.h>
#include <lvgl.h>

namespace {

constexpr uint32_t W = 480, H = 320;

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void write_b64(const uint8_t *p, size_t n)
{
    char line[80];
    size_t li = 0;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t b0 = p[i];
        const uint32_t b1 = i + 1 < n ? p[i + 1] : 0;
        const uint32_t b2 = i + 2 < n ? p[i + 2] : 0;
        const uint32_t v  = (b0 << 16) | (b1 << 8) | b2;
        line[li++] = B64[(v >> 18) & 63];
        line[li++] = B64[(v >> 12) & 63];
        line[li++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        line[li++] = i + 2 < n ? B64[v & 63] : '=';
        if (li >= 76) { line[li++] = '\n'; Serial.write((uint8_t *)line, li); li = 0; }
    }
    if (li) { line[li++] = '\n'; Serial.write((uint8_t *)line, li); }
}

}  // namespace

namespace screenshot {

void dump()
{
    const uint32_t stride = W * 2;
    const uint32_t size   = stride * H;
    uint8_t *buf = (uint8_t *)ps_malloc(size + 64);
    if (!buf) { Serial.println("[shot] kein PSRAM"); return; }

    lv_draw_buf_t db;
    lv_draw_buf_init(&db, W, H, LV_COLOR_FORMAT_RGB565, stride, buf, size);
    const lv_result_t res = lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &db);
    if (res != LV_RESULT_OK) { Serial.println("[shot] snapshot fehlgeschlagen"); free(buf); return; }

    Serial.printf("[shot] begin %lu %lu rgb565\n", (unsigned long)W, (unsigned long)H);
    write_b64(buf, size);
    Serial.println("[shot] end");
    free(buf);
}

}  // namespace screenshot
