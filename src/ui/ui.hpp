// Die Screens des Sauerteig-Timers. ui::begin() baut alles auf und zeigt
// je nach Sitzungszustand das Dashboard (Uhr, Stromverbrauch, "Backen")
// oder den laufenden Timer; ui::tick() haelt Uhr und Restzeit aktuell.
#pragma once

#include "recipe.hpp"

namespace ui {

void begin();
void tick();   // aus loop(); intern auf 1 Hz gedrosselt

void show_home();
void show_select();
void show_ingredients(const recipe::Recipe &r, bool startable);
void show_timer();
void show_alarm();
bool alarm_active();   // Timer abgelaufen, wartet auf Erledigt

}  // namespace ui
