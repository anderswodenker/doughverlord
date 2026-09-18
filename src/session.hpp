// Sitzung: der laufende Teig. Welcher Schritt, welche Runde, seit wann --
// und das alles im NVS, damit ein Reset ueber Nacht nichts verliert.
//
// Zeitbasis ist die Unix-Zeit, nicht millis(): nur damit laesst sich nach
// einem Reset ausrechnen, wie weit ein Schritt wirklich ist. Ohne gueltige
// Uhr wird keine Restzeit erfunden -- remaining_s() liefert dann -1.
#pragma once

#include <Arduino.h>
#include <time.h>

#include "recipe.hpp"

namespace session {

enum class State : uint8_t {
    Idle    = 0,   // kein Teig
    Running = 1,   // Timer laeuft
    Waiting = 2,   // wartet auf "Erledigt" -- Alarm, wenn current_step() nicht
                   // offen ist (Timer abgelaufen), sonst schlicht ein offener Schritt
    Done    = 3,   // letzter Schritt bestaetigt
};

// Was seit dem letzten Abholen passiert ist -- fuer Alarm und UI.
enum class Event : uint8_t {
    None,
    Started,    // neuer Schritt bzw. neue Runde laeuft
    Expired,    // Timer abgelaufen -> Alarm
    Finished,   // Rezept durch
};

// Wahr, wenn die Systemuhr plausibel gestellt ist (NTP oder von Hand).
bool time_valid();

// Laedt den gespeicherten Zustand aus dem NVS und das zugehoerige Rezept.
void begin();

State state();
bool  active();                        // Running oder Waiting
const recipe::Recipe *current_recipe();
uint16_t step_index();                 // 0-basiert
uint8_t  round_index();                // 0-basiert
const recipe::Step *current_step();

// Zeitraffer fuer Tests: Dauern werden durch diesen Faktor geteilt.
// Kommt spaeter aus /config.json; bis dahin ueber die serielle Konsole.
void     set_time_factor(uint32_t f);
uint32_t time_factor();

// Startet ein Rezept ("Zusammengemischt -- los").
bool start(const char *path, String &error);
// "Erledigt": schaltet Runde/Schritt weiter.
void confirm();
// Teig verwerfen.
void abort();

// Regelmaessig aus loop() aufrufen: erkennt abgelaufene Timer.
void tick();
// Holt das letzte Ereignis ab und setzt es zurueck.
Event take_event();

// Effektive Dauer einer Runde des aktuellen Schritts (Zeitraffer drin); 0 bei offen.
uint32_t step_duration_s();
// Restzeit des laufenden Schritts in Sekunden; 0 wenn abgelaufen,
// -1 wenn nicht bestimmbar (kein Timer oder Uhr ungueltig).
int32_t remaining_s();
// Voraussichtliches Ende des ganzen Rezepts; 0 wenn nicht bestimmbar.
time_t  eta_end();

void log_status();

}  // namespace session
