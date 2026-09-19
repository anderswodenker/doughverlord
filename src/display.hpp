// Display und Touch: LovyanGFX-Panel, LVGL-Anbindung (Flush, Touch, Tick),
// Helligkeit. Alles Hardware-nahe lebt hier, die Screens in src/ui/.
#pragma once

#include <Arduino.h>

namespace display {

void begin();
void set_brightness(uint8_t v);
uint8_t brightness();

constexpr uint8_t BRIGHT_FULL = 200;
constexpr uint8_t BRIGHT_DIM  = 30;    // ~15 %: nachts in der Kueche noch lesbar

// Millisekunden seit der letzten Beruehrung -- fuer das Dimmen.
uint32_t idle_ms();

}  // namespace display
