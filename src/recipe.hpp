// Rezepte: JSON von der SD-Karte lesen und in eine Schrittliste uebersetzen.
//
// Format siehe PLAN.md bzw. sd/rezepte/bauernbrot.json. Drei Schrittarten:
//   feste Dauer   { "dauer": "45min" }
//   wiederkehrend { "dauer": "30min", "runden": 4 }
//   offen         { "offen": true }            -- wartet nur auf Bestaetigung
#pragma once

#include <Arduino.h>
#include <vector>

namespace recipe {

struct Ingredient {
    String menge;   // "300 g" -- bewusst Text, wird nur angezeigt
    String was;     // "Weizenmehl 550"
};

struct Step {
    String   name;
    String   hinweis;        // optional, leer wenn nicht gesetzt
    uint32_t dauer_s = 0;    // Sekunden je Runde; 0 bei offenen Schritten
    uint8_t  runden  = 1;
    bool     offen   = false;
    std::vector<Ingredient> zutaten;   // nur, was erst in diesem Schritt dazukommt

    uint32_t gesamt_s() const { return offen ? 0 : dauer_s * runden; }
};

struct Recipe {
    String name;
    String datei;    // Pfad auf der Karte, z. B. /rezepte/bauernbrot.json
    std::vector<Ingredient> zutaten;
    std::vector<Step>       schritte;

    // Summe aller Timer-Dauern; offene Schritte zaehlen nicht.
    uint32_t timer_gesamt_s() const;
    size_t   offene_schritte() const;
};

// "45min", "3h", "1h30min", "90s" -- Leerzeichen zwischen den Teilen erlaubt.
bool parse_duration(const char *text, uint32_t &out_s);

// "1 h 30 min", "45 min", "12 h" -- fuer Log und spaeter die UI.
String format_duration(uint32_t s);

// Alle Rezeptdateien in /rezepte, nur die Dateinamen.
std::vector<String> list_files();

// Liest und prueft ein Rezept. Bei false steht in `error` eine Meldung, die
// man auf dem Display zeigen kann; `out` ist dann unveraendert.
bool load(const char *path, Recipe &out, String &error);

// Dasselbe aus einem String -- fuer Tests ohne Karte.
bool parse(const char *json, Recipe &out, String &error);

void log(const Recipe &r);

#ifdef RECIPE_SELFTEST
void selftest();
#endif

}  // namespace recipe
