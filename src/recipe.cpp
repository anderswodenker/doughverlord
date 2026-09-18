#include "recipe.hpp"

#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>

#include "sd_card.hpp"

namespace {

constexpr const char *RECIPE_DIR = "/rezepte";

bool read_ingredients(JsonArrayConst arr, std::vector<recipe::Ingredient> &out,
                      const char *wo, String &error)
{
    for (JsonObjectConst z : arr) {
        const char *menge = z["menge"];
        const char *was   = z["was"];
        if (!was || !*was) {
            error = String("Zutat ohne 'was' in ") + wo;
            return false;
        }
        out.push_back({ menge ? menge : "", was });
    }
    return true;
}

bool from_doc(JsonObjectConst doc, recipe::Recipe &out, String &error)
{
    recipe::Recipe r;

    const char *name = doc["name"];
    if (!name || !*name) { error = "Rezept hat keinen 'name'"; return false; }
    r.name = name;

    JsonArrayConst zutaten = doc["zutaten"];
    if (zutaten.isNull()) {
        Serial.printf("[rezept] %s: kein Zutatenblock -- geht, ist aber ungewoehnlich\n", name);
    } else if (!read_ingredients(zutaten, r.zutaten, "Zutatenliste", error)) {
        return false;
    }

    JsonArrayConst schritte = doc["schritte"];
    if (schritte.isNull() || schritte.size() == 0) {
        error = "Rezept hat keine 'schritte'";
        return false;
    }

    size_t nr = 0;
    for (JsonObjectConst s : schritte) {
        nr++;
        recipe::Step step;

        const char *sname = s["name"];
        if (!sname || !*sname) {
            error = String("Schritt ") + nr + " hat keinen 'name'";
            return false;
        }
        step.name    = sname;
        step.hinweis = s["hinweis"] | "";
        step.offen   = s["offen"] | false;

        const char *dauer = s["dauer"];
        if (step.offen) {
            if (dauer) {
                error = String("Schritt '") + sname + "' ist offen und hat trotzdem eine 'dauer'";
                return false;
            }
        } else {
            if (!dauer || !recipe::parse_duration(dauer, step.dauer_s) || step.dauer_s == 0) {
                error = String("Schritt '") + sname + "': 'dauer' fehlt oder unlesbar";
                if (dauer) error += String(" (\"") + dauer + "\")";
                return false;
            }
            const int runden = s["runden"] | 1;
            if (runden < 1 || runden > 99) {
                error = String("Schritt '") + sname + "': 'runden' muss 1..99 sein";
                return false;
            }
            step.runden = (uint8_t)runden;
        }

        JsonArrayConst sz = s["zutaten"];
        if (!sz.isNull() && !read_ingredients(sz, step.zutaten, sname, error)) return false;

        r.schritte.push_back(std::move(step));
    }

    out = std::move(r);
    return true;
}

// Zeigt bei JSON-Fehlern, wo es hakt: ArduinoJson liefert nur den Fehlertyp.
String json_error(DeserializationError err)
{
    if (err == DeserializationError::NoMemory) return "JSON zu gross fuer den Speicher";
    if (err == DeserializationError::EmptyInput) return "Datei ist leer";
    return String("JSON unlesbar: ") + err.c_str();
}

}  // namespace

namespace recipe {

uint32_t Recipe::timer_gesamt_s() const
{
    uint32_t sum = 0;
    for (const Step &s : schritte) sum += s.gesamt_s();
    return sum;
}

size_t Recipe::offene_schritte() const
{
    size_t n = 0;
    for (const Step &s : schritte) if (s.offen) n++;
    return n;
}

bool parse_duration(const char *text, uint32_t &out_s)
{
    if (!text) return false;
    uint32_t total = 0;
    bool any = false;
    const char *p = text;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (!isdigit((unsigned char)*p)) return false;

        uint32_t n = 0;
        while (isdigit((unsigned char)*p)) {
            n = n * 10 + (uint32_t)(*p - '0');
            if (n > 100000) return false;
            p++;
        }
        while (*p == ' ') p++;

        if (*p == 'h')                            { total += n * 3600; p += 1; }
        else if (p[0] == 'm' && p[1] == 'i' && p[2] == 'n') { total += n * 60; p += 3; }
        else if (*p == 'm')                       { total += n * 60;   p += 1; }
        else if (*p == 's')                       { total += n;        p += 1; }
        else return false;
        any = true;
    }

    if (!any) return false;
    out_s = total;
    return true;
}

String format_duration(uint32_t s)
{
    const uint32_t h   = s / 3600;
    const uint32_t min = (s % 3600) / 60;
    const uint32_t sec = s % 60;
    String out;
    if (h)   { out += h;   out += " h"; }
    if (min) { if (out.length()) out += " "; out += min; out += " min"; }
    if (sec || !out.length()) { if (out.length()) out += " "; out += sec; out += " s"; }
    return out;
}

std::vector<String> list_files()
{
    std::vector<String> names;
    if (!sdcard::ready()) return names;

    File dir = SD.open(RECIPE_DIR);
    if (!dir || !dir.isDirectory()) return names;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        String n = f.name();
        if (!f.isDirectory() && n.endsWith(".json") && !n.startsWith(".")) names.push_back(n);
    }
    dir.close();
    std::sort(names.begin(), names.end(),
              [](const String &a, const String &b) { return a.compareTo(b) < 0; });
    return names;
}

bool load(const char *path, Recipe &out, String &error)
{
    if (!sdcard::ready()) { error = "SD-Karte nicht bereit"; return false; }

    File f = SD.open(path, FILE_READ);
    if (!f) { error = String("Datei nicht gefunden: ") + path; return false; }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { error = json_error(err); return false; }

    if (!from_doc(doc.as<JsonObjectConst>(), out, error)) return false;
    out.datei = path;
    return true;
}

bool parse(const char *json, Recipe &out, String &error)
{
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json);
    if (err) { error = json_error(err); return false; }
    return from_doc(doc.as<JsonObjectConst>(), out, error);
}

void log(const Recipe &r)
{
    Serial.printf("[rezept] %s  (%s)\n", r.name.c_str(), r.datei.c_str());
    Serial.println("[rezept] Zutaten:");
    for (const Ingredient &z : r.zutaten)
        Serial.printf("[rezept]   %8s  %s\n", z.menge.c_str(), z.was.c_str());

    Serial.println("[rezept] Schritte:");
    size_t nr = 0;
    for (const Step &s : r.schritte) {
        nr++;
        String dauer;
        if (s.offen)             dauer = "offen";
        else if (s.runden > 1)   dauer = String(s.runden) + " x " + format_duration(s.dauer_s);
        else                     dauer = format_duration(s.dauer_s);
        Serial.printf("[rezept]   %2u. %-20s %s", (unsigned)nr, s.name.c_str(), dauer.c_str());
        for (const Ingredient &z : s.zutaten)
            Serial.printf("  +%s %s", z.menge.c_str(), z.was.c_str());
        if (s.hinweis.length()) Serial.printf("  \"%s\"", s.hinweis.c_str());
        Serial.println();
    }
    Serial.printf("[rezept] Timerzeit gesamt: %s, %u offene Schritte\n",
                  format_duration(r.timer_gesamt_s()).c_str(), (unsigned)r.offene_schritte());
}

#ifdef RECIPE_SELFTEST
void selftest()
{
    struct { const char *text; uint32_t erwartet; bool ok; } dur[] = {
        { "45min",     45 * 60,            true  },
        { "3h",        3 * 3600,           true  },
        { "1h30min",   5400,               true  },
        { "1h 30min",  5400,               true  },
        { "90s",       90,                 true  },
        { "2m",        120,                true  },
        { "",          0,                  false },
        { "abc",       0,                  false },
        { "30",        0,                  false },
        { "1h30",      0,                  false },
        { "3 Stunden", 0,                  false },
    };
    for (auto &t : dur) {
        uint32_t s = 0;
        const bool ok = parse_duration(t.text, s);
        const bool pass = (ok == t.ok) && (!ok || s == t.erwartet);
        Serial.printf("[test] dauer \"%s\" -> %s %lu  %s\n", t.text, ok ? "ok" : "fehler",
                      (unsigned long)s, pass ? "PASS" : "FAIL");
    }

    struct { const char *name; const char *json; } bad[] = {
        { "kaputtes JSON",     "{ \"name\": \"X\", \"schritte\": [ { \"name\": " },
        { "leer",              "" },
        { "ohne name",         "{ \"schritte\": [ { \"name\": \"A\", \"dauer\": \"1h\" } ] }" },
        { "ohne schritte",     "{ \"name\": \"X\" }" },
        { "dauer unlesbar",    "{ \"name\": \"X\", \"schritte\": [ { \"name\": \"A\", \"dauer\": \"bald\" } ] }" },
        { "dauer fehlt",       "{ \"name\": \"X\", \"schritte\": [ { \"name\": \"A\" } ] }" },
        { "offen mit dauer",   "{ \"name\": \"X\", \"schritte\": [ { \"name\": \"A\", \"offen\": true, \"dauer\": \"1h\" } ] }" },
        { "zutat ohne was",    "{ \"name\": \"X\", \"zutaten\": [ { \"menge\": \"1 g\" } ], \"schritte\": [ { \"name\": \"A\", \"dauer\": \"1h\" } ] }" },
    };
    for (auto &t : bad) {
        Recipe r; String err;
        const bool ok = parse(t.json, r, err);
        Serial.printf("[test] %-18s -> %s: %s\n", t.name, ok ? "FAIL (angenommen)" : "PASS", err.c_str());
    }

    {
        Recipe r; String err;
        const bool ok = parse("{ \"name\": \"Minimal\", \"schritte\": [ { \"name\": \"A\", \"dauer\": \"1h\" } ] }", r, err);
        Serial.printf("[test] ohne zutatenblock -> %s\n", ok ? "PASS" : (String("FAIL: ") + err).c_str());
    }
}
#endif

}  // namespace recipe
