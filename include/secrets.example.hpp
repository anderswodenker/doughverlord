// Vorlage: nach include/secrets.hpp kopieren und ausfuellen.
// secrets.hpp ist in .gitignore und landet nicht im Repo.
#pragma once

#define SECRET_WIFI_SSID "meinWLAN"
#define SECRET_WIFI_PASS "geheim"

// Stromzaehler per MQTT (optional; leer lassen = kein Dashboard-Verbrauch).
// Dieselben Werte wie in ~/.config/omarchy/strom.env der Bar-Bruecke.
#define SECRET_MQTT_HOST  ""
#define SECRET_MQTT_PORT  1883
#define SECRET_MQTT_USER  ""
#define SECRET_MQTT_PASS  ""
#define SECRET_MQTT_TOPIC ""
