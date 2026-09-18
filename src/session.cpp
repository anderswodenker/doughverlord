#include "session.hpp"

#include <Preferences.h>
#include <sys/time.h>

namespace {

// Alles vor 2024 ist keine echte Uhrzeit, sondern der Reset-Wert der RTC.
constexpr time_t TIME_PLAUSIBLE = 1704067200;   // 2024-01-01 00:00 UTC

constexpr const char *NVS_NS = "teig";

Preferences     prefs;
recipe::Recipe  rezept;
bool            rezept_ok = false;

session::State  zustand   = session::State::Idle;
uint16_t        schritt   = 0;
uint8_t         runde     = 0;
time_t          startzeit     = 0;        // Unix-Zeit des Runden-Starts, 0 = unbekannt
uint32_t        start_ms  = 0;        // millis() beim Start, nur bis zum naechsten Reset
bool            start_ms_ok = false;

uint32_t        faktor    = 1;
session::Event  ereignis  = session::Event::None;

time_t now() { return time(nullptr); }

const recipe::Step *step_at(uint16_t i)
{
    return (rezept_ok && i < rezept.schritte.size()) ? &rezept.schritte[i] : nullptr;
}

// Effektive Dauer einer Runde, Zeitraffer beruecksichtigt.
uint32_t eff_dauer(const recipe::Step &s)
{
    const uint32_t d = s.dauer_s / faktor;
    return d ? d : 1;
}

String hhmm(time_t t)
{
    char buf[8];
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, sizeof buf, "%H:%M", &tm);
    return buf;
}

void save()
{
    prefs.putUChar("zustand", (uint8_t)zustand);
    prefs.putUShort("schritt", schritt);
    prefs.putUChar("runde", runde);
    prefs.putLong64("start", (int64_t)startzeit);
    if (zustand == session::State::Idle) prefs.remove("rezept");
    else                                 prefs.putString("rezept", rezept.datei);
}

void set_state(session::State s)
{
    zustand = s;
    save();
}

// Beginnt die aktuelle Runde des aktuellen Schritts.
void begin_round()
{
    const recipe::Step *s = step_at(schritt);
    if (!s) return;

    if (s->offen) {
        startzeit = 0;
        start_ms_ok = false;
        set_state(session::State::Waiting);
        Serial.printf("[teig] Schritt %u/%u %s: offen, wartet auf Erledigt\n",
                      schritt + 1, (unsigned)rezept.schritte.size(), s->name.c_str());
        ereignis = session::Event::Started;
        return;
    }

    startzeit       = session::time_valid() ? now() : 0;
    start_ms    = millis();
    start_ms_ok = true;
    set_state(session::State::Running);

    const uint32_t d = eff_dauer(*s);
    Serial.printf("[teig] Schritt %u/%u %s", schritt + 1, (unsigned)rezept.schritte.size(), s->name.c_str());
    if (s->runden > 1) Serial.printf(", Runde %u von %u", runde + 1, s->runden);
    Serial.printf(": %s", recipe::format_duration(d).c_str());
    if (faktor > 1) Serial.printf(" (Zeitraffer x%lu)", (unsigned long)faktor);
    if (startzeit) Serial.printf(", fertig um %s", hhmm(startzeit + d).c_str());
    else       Serial.print(", Uhr ungueltig -- Startzeit unbekannt");
    Serial.println();
    ereignis = session::Event::Started;
}

void advance()
{
    const recipe::Step *s = step_at(schritt);
    if (s && !s->offen && runde + 1 < s->runden) {
        runde++;
        begin_round();
        return;
    }

    runde = 0;
    schritt++;
    if (schritt >= rezept.schritte.size()) {
        startzeit = 0;
        set_state(session::State::Done);
        Serial.printf("[teig] %s fertig -- alle %u Schritte bestaetigt\n",
                      rezept.name.c_str(), (unsigned)rezept.schritte.size());
        ereignis = session::Event::Finished;
        return;
    }
    begin_round();
}

// Sekunden seit Rundenstart; -1 wenn nicht bestimmbar.
int32_t elapsed_s()
{
    if (startzeit && session::time_valid()) {
        const time_t e = now() - startzeit;
        return e < 0 ? 0 : (int32_t)e;
    }
    if (start_ms_ok) return (int32_t)((millis() - start_ms) / 1000);
    return -1;
}

}  // namespace

namespace session {

bool time_valid() { return now() >= TIME_PLAUSIBLE; }

void begin()
{
    prefs.begin(NVS_NS, false);

    zustand = (State)prefs.getUChar("zustand", (uint8_t)State::Idle);
    schritt = prefs.getUShort("schritt", 0);
    runde   = prefs.getUChar("runde", 0);
    startzeit   = (time_t)prefs.getLong64("start", 0);
    const String datei = prefs.isKey("rezept") ? prefs.getString("rezept", "") : "";

    if (zustand == State::Idle) {
        Serial.println("[teig] kein Teig gespeichert");
        return;
    }
    if (zustand == State::Done) {
        // Bleibt stehen, bis der User es wegdrueckt -- das Rezept selbst
        // wird dafuer nicht mehr gebraucht.
        Serial.printf("[teig] wiederhergestellt: %s ist fertig\n", datei.c_str());
        return;
    }

    String err;
    rezept_ok = recipe::load(datei.c_str(), rezept, err);
    if (!rezept_ok || !step_at(schritt)) {
        Serial.printf("[teig] gespeicherter Teig (%s, Schritt %u) nicht wiederherstellbar: %s\n",
                      datei.c_str(), schritt + 1, rezept_ok ? "Schritt ausserhalb des Rezepts" : err.c_str());
        zustand = State::Idle;
        rezept_ok = false;
        save();
        return;
    }

    Serial.printf("[teig] wiederhergestellt: %s, Schritt %u/%u, Runde %u, Zustand %u\n",
                  rezept.name.c_str(), schritt + 1, (unsigned)rezept.schritte.size(),
                  runde + 1, (unsigned)zustand);
    if (zustand == State::Running && !startzeit)
        Serial.println("[teig] Startzeit war unbekannt -- Restzeit nach Reset nicht bestimmbar");
}

State state()  { return zustand; }
bool  active() { return zustand == State::Running || zustand == State::Waiting; }
const recipe::Recipe *current_recipe() { return rezept_ok ? &rezept : nullptr; }
uint16_t step_index()  { return schritt; }
uint8_t  round_index() { return runde; }
const recipe::Step *current_step() { return active() ? step_at(schritt) : nullptr; }

void     set_time_factor(uint32_t f) { faktor = f ? f : 1; }
uint32_t time_factor()               { return faktor; }

bool start(const char *path, String &error)
{
    if (active()) { error = "Es laeuft schon ein Teig"; return false; }

    recipe::Recipe r;
    if (!recipe::load(path, r, error)) return false;

    rezept    = std::move(r);
    rezept_ok = true;
    schritt   = 0;
    runde     = 0;
    Serial.printf("[teig] %s gestartet\n", rezept.name.c_str());
    begin_round();
    return true;
}

void confirm()
{
    if (zustand != State::Waiting) {
        Serial.println("[teig] Erledigt ignoriert -- nichts wartet");
        return;
    }
    advance();
}

void abort()
{
    if (zustand == State::Idle) return;
    Serial.printf("[teig] %s verworfen\n", rezept_ok ? rezept.name.c_str() : "Teig");
    zustand   = State::Idle;
    rezept_ok = false;
    schritt = runde = 0;
    startzeit = 0;
    save();
}

void tick()
{
    if (zustand != State::Running) return;

    // Uhr wurde inzwischen gestellt: Startzeit nachtragen, solange millis()
    // seit dem Start noch weiterlaeuft.
    if (!startzeit && start_ms_ok && time_valid()) {
        startzeit = now() - (time_t)((millis() - start_ms) / 1000);
        save();
        Serial.printf("[teig] Uhr jetzt gueltig, Startzeit nachgetragen: %s\n", hhmm(startzeit).c_str());
    }

    const int32_t rest = remaining_s();
    if (rest == 0) {
        const recipe::Step *s = step_at(schritt);
        set_state(State::Waiting);
        Serial.printf("[teig] ALARM: %s abgelaufen\n", s ? s->name.c_str() : "?");
        ereignis = Event::Expired;
    }
}

Event take_event()
{
    const Event e = ereignis;
    ereignis = Event::None;
    return e;
}

uint32_t step_duration_s()
{
    const recipe::Step *s = current_step();
    return (s && !s->offen) ? eff_dauer(*s) : 0;
}

int32_t remaining_s()
{
    if (zustand != State::Running) return -1;
    const recipe::Step *s = step_at(schritt);
    if (!s || s->offen) return -1;
    const int32_t e = elapsed_s();
    if (e < 0) return -1;
    const int32_t d = (int32_t)eff_dauer(*s);
    return e >= d ? 0 : d - e;
}

time_t eta_end()
{
    if (!active() || !time_valid()) return 0;

    int64_t rest = 0;
    if (zustand == State::Running) {
        const int32_t r = remaining_s();
        if (r < 0) return 0;
        rest += r;
    }
    // Restliche Runden des laufenden Schritts, dann alle folgenden Schritte.
    const recipe::Step *cur = step_at(schritt);
    if (cur && !cur->offen) rest += (int64_t)eff_dauer(*cur) * (cur->runden - runde - 1);
    for (size_t i = schritt + 1; i < rezept.schritte.size(); i++) {
        const recipe::Step &s = rezept.schritte[i];
        if (!s.offen) rest += (int64_t)eff_dauer(s) * s.runden;
    }
    return now() + (time_t)rest;
}

void log_status()
{
    if (!active()) {
        Serial.printf("[teig] Zustand: %s\n", zustand == State::Done ? "fertig" : "kein Teig");
        return;
    }
    const recipe::Step *s = step_at(schritt);
    Serial.printf("[teig] %s, Schritt %u/%u %s", rezept.name.c_str(), schritt + 1,
                  (unsigned)rezept.schritte.size(), s ? s->name.c_str() : "?");
    if (s && s->runden > 1) Serial.printf(", Runde %u/%u", runde + 1, s->runden);
    if (zustand == State::Waiting) {
        Serial.print(", wartet auf Erledigt");
    } else {
        const int32_t r = remaining_s();
        if (r < 0) Serial.print(", Restzeit unbekannt");
        else       Serial.printf(", Rest %s", recipe::format_duration(r).c_str());
    }
    const time_t eta = eta_end();
    if (eta) Serial.printf(", Brot fertig ca. %s", hhmm(eta).c_str());
    Serial.printf(", Uhr %s\n", time_valid() ? hhmm(now()).c_str() : "ungueltig");
}

}  // namespace session
