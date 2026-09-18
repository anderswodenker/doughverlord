// WLAN und Zeit: verbindet sich im Hintergrund, holt die Uhrzeit per NTP,
// verbindet nach Abbruch von selbst neu. Ohne Netz laeuft die App weiter --
// nur die Uhr bleibt dann ungueltig, bis das Netz wiederkommt.
#pragma once

#include <Arduino.h>

namespace net {

void begin(const String &ssid, const String &pass);

bool   wifi_connected();
String ip();
int8_t rssi();
bool   ntp_synced();        // Uhr mindestens einmal per NTP gestellt

// Eine Zeile fuer Heartbeat und Status-Screen.
String status_line();

}  // namespace net
