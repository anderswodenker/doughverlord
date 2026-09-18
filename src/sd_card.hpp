// MicroSD-Karte des WT32-SC01 Plus: SPI-Bus, Mount, Verzeichnislisting.
//
// Die Karte haengt an einem eigenen SPI (CLK 39, MISO 38, MOSI 40, CS 41)
// und kommt dem Panel am Parallelbus nicht in die Quere.
#pragma once

#include <Arduino.h>

namespace sdcard {

// Mountet die Karte. Liefert false, wenn keine steckt oder sie nicht lesbar
// ist -- die App muss dann ohne Rezepte auskommen, aber weiterlaufen.
bool begin();

bool ready();

// Listet ein Verzeichnis rekursiv auf die serielle Konsole.
void list(const char *path, uint8_t depth = 0);

// Legt /rezepte samt Beispielrezept an, falls das Verzeichnis fehlt oder
// leer ist. So ist auf einer frisch formatierten Karte sofort etwas da,
// ohne dass sie dafuer aus dem Slot muss.
void ensure_example_recipe();

}  // namespace sdcard
