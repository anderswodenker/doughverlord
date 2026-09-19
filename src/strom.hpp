// Hausverbrauch vom Stromzaehler per MQTT -- fuers Dashboard.
//
// Abonniert das Tasmota-SML-Telegramm (JSON mit Power_curr in W und
// Total_in in kWh, so wie es auch die omarchy-strom-Bruecke liest). Der
// esp_mqtt-Client laeuft in einem eigenen Task und verbindet von selbst
// neu; hier wird nur der letzte Messwert festgehalten.
#pragma once

#include <Arduino.h>

#include "config.hpp"

namespace strom {

void begin(const config::Mqtt &m);   // host leer = bleibt aus
void tick();                          // aus loop(): startet den Client, sobald WLAN steht

bool     configured();
bool     connected();     // Broker-Verbindung steht
bool     valid();         // Messwert da und juenger als STALE_S
int      watts();
float    kwh();
uint32_t age_s();         // Sekunden seit dem letzten Messwert (UINT32_MAX ohne)

constexpr uint32_t STALE_S = 90;   // wie das Bar-Widget

// Verlauf der letzten 24 h als Minutenmittel, aeltester Wert zuerst,
// -1 = keine Daten. Liegt im RAM, wird alle 5 min nach /strom.bin auf der
// SD gesichert und beim Start zurueckgeholt; braucht eine gueltige Uhr.
constexpr size_t HIST_N = 24 * 60;
void     history(int16_t *out);      // fuellt HIST_N Werte
uint32_t samples();                  // zaehlt eingearbeitete Messwerte -- fuer "hat sich was geaendert?"

// Fuer Heartbeat und CLI: "strom=1234W" / "strom=alt" / "strom=--"
String status_line();

}  // namespace strom
