// Geraetekonfiguration aus /config.json auf der SD-Karte.
//
// Fehlt die Datei, wird sie mit den Vorgaben aus include/secrets.hpp
// angelegt -- danach ist die Karte die Wahrheit und laesst sich am
// Rechner editieren, ohne neu zu flashen.
#pragma once

#include <Arduino.h>

namespace config {

struct Config {
    String   wifi_ssid;
    String   wifi_pass;
    uint32_t zeitraffer = 1;    // Dauern durch diesen Faktor teilen (nur zum Testen)
    String   ntfy_topic;        // leer = kein Push
};

// Liest /config.json; legt sie bei Bedarf an. Liefert immer eine brauchbare
// Konfiguration -- notfalls die Vorgaben.
const Config &load();
const Config &get();

// Schreibt neue WLAN-Zugangsdaten in /config.json (Rest bleibt erhalten).
bool save_wifi(const String &ssid, const String &pass);

}  // namespace config
