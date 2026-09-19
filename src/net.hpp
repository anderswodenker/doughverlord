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

// Push per ntfy (JSON-POST an server/, damit Umlaute im Titel gehen).
// Laeuft in einem eigenen Task, blockiert die UI nicht; ohne Topic
// oder WLAN passiert nichts ausser einer Logzeile.
void set_ntfy(const String &server, const String &topic);
bool notify(const String &title, const String &text);
bool ntfy_configured();

// Eine Zeile fuer Heartbeat und Status-Screen.
String status_line();

}  // namespace net
