#include "cli.hpp"

#include <Arduino.h>
#include <sys/time.h>

#include "config.hpp"
#include "display.hpp"
#include "net.hpp"
#include "recipe.hpp"
#include "screenshot.hpp"
#include "session.hpp"
#include "strom.hpp"
#include "ui/screens.hpp"
#include "ui/ui.hpp"

namespace {

void help()
{
    Serial.println("[cli] Befehle:");
    Serial.println("[cli]   s <datei>   Rezept starten, z. B. s bauernbrot.json");
    Serial.println("[cli]   c           Erledigt (Schritt/Runde bestaetigen)");
    Serial.println("[cli]   x           Teig verwerfen");
    Serial.println("[cli]   p           Status");
    Serial.println("[cli]   l           Rezepte auflisten");
    Serial.println("[cli]   f <n>       Zeitraffer-Faktor setzen");
    Serial.println("[cli]   t <unix>    Uhr stellen (bis NTP da ist)");
    Serial.println("[cli]   w <ssid> <passwort>  WLAN in /config.json schreiben und neu starten");
    Serial.println("[cli]   m <host> <port> <user> <passwort> <topic>  Stromzaehler-MQTT in /config.json, Neustart");
    Serial.println("[cli]   m -         Stromzaehler abschalten");
    Serial.println("[cli]   n           Netzstatus");
    Serial.println("[cli]   shot        Screenshot als Base64 (tools/screenshot.py)");
    Serial.println("[cli]   u h|s|z|t|a|l Screen anzeigen: Dashboard, Auswahl, Zutaten, Timer, Abbruch-Rueckfrage, Alarm");
    Serial.println("[cli]   push [text] Test-Push per ntfy");
    Serial.println("[cli]   ntfy <topic>|-  Push-Topic in /config.json");
    Serial.println("[cli]   b <0-255>   Helligkeit setzen (ui::tick holt sie nach 1 s zurueck)");
}

void handle(String line)
{
    line.trim();
    if (!line.length()) return;
    if (line == "shot") { screenshot::dump(); return; }
    if (line.startsWith("push")) {          // Test-Push, Text optional
        String t = line.substring(4); t.trim();
        net::notify("Test vom Doughverlord", t.length() ? t : String("Push funktioniert."));
        return;
    }
    if (line.startsWith("ntfy")) {          // Topic in config.json, gilt sofort
        String t = line.substring(4); t.trim();
        if (t == "-") t = "";
        if (config::save_ntfy(t)) {
            net::set_ntfy(config::get().ntfy_server, t);
            Serial.printf("[cli] ntfy-Topic: %s\n", t.length() ? t.c_str() : "aus");
        } else Serial.println("[cli] konnte config.json nicht schreiben");
        return;
    }
    const char cmd = line[0];
    String arg = line.substring(1);
    arg.trim();

    switch (cmd) {
        case 's': {
            String err;
            const String path = arg.startsWith("/") ? arg : String("/rezepte/") + arg;
            if (!session::start(path.c_str(), err)) Serial.printf("[cli] start: %s\n", err.c_str());
            break;
        }
        case 'c': session::confirm(); break;
        case 'x': session::abort(); break;
        case 'p': session::log_status(); break;
        case 'l':
            for (const String &n : recipe::list_files()) Serial.printf("[cli]   %s\n", n.c_str());
            break;
        case 'f':
            session::set_time_factor((uint32_t)arg.toInt());
            Serial.printf("[cli] Zeitraffer x%lu\n", (unsigned long)session::time_factor());
            break;
        case 't': {
            struct timeval tv = { .tv_sec = (time_t)strtoll(arg.c_str(), nullptr, 10), .tv_usec = 0 };
            settimeofday(&tv, nullptr);
            Serial.printf("[cli] Uhr gestellt, gueltig=%s\n", session::time_valid() ? "ja" : "nein");
            break;
        }
        case 'w': {
            const int sp = arg.indexOf(' ');
            if (sp < 1) { Serial.println("[cli] w <ssid> <passwort>"); break; }
            if (config::save_wifi(arg.substring(0, sp), arg.substring(sp + 1))) {
                Serial.println("[cli] gespeichert, Neustart");
                delay(100);
                ESP.restart();
            }
            break;
        }
        case 'm': {
            config::Mqtt m;
            if (arg != "-") {
                // Fuenf Felder durch Leerzeichen; user/passwort duerfen "-" sein (= leer).
                String f[5];
                int i = 0, from = 0;
                while (i < 5) {
                    const int sp = arg.indexOf(' ', from);
                    f[i++] = sp < 0 ? arg.substring(from) : arg.substring(from, sp);
                    if (sp < 0) break;
                    from = sp + 1;
                }
                if (i < 5 || !f[0].length() || !f[4].length()) {
                    Serial.println("[cli] m <host> <port> <user|-> <passwort|-> <topic>");
                    break;
                }
                m.host  = f[0];
                m.port  = (uint16_t)f[1].toInt();
                m.user  = f[2] == "-" ? "" : f[2];
                m.pass  = f[3] == "-" ? "" : f[3];
                m.topic = f[4];
            }
            if (config::save_mqtt(m)) {
                Serial.println("[cli] gespeichert, Neustart");
                delay(100);
                ESP.restart();
            }
            break;
        }
        case 'b': display::set_brightness((uint8_t)arg.toInt()); break;
        case 'n': Serial.printf("[cli] %s %s ntfy=%s hell=%u\n", net::status_line().c_str(), strom::status_line().c_str(),
                                net::ntfy_configured() ? "ok" : "--", display::brightness()); break;
        case 'u': {   // Screens ohne Touch anspringen (fuer Screenshots)
            if (arg == "h") ui::show_home();
            else if (arg == "s") ui::show_select();
            else if (arg == "t") ui::show_timer();
            else if (arg == "a") { ui::show_timer(); ui::timer::ask_abort(); }
            else if (arg == "l") ui::show_alarm();
            else if (arg == "z") {
                const recipe::Recipe *r = session::current_recipe();
                recipe::Recipe tmp; String err;
                if (!r) {
                    auto files = recipe::list_files();
                    if (!files.empty() && recipe::load((String("/rezepte/") + files[0]).c_str(), tmp, err)) r = &tmp;
                }
                if (r) ui::show_ingredients(*r, !session::active());
            }
            break;
        }
        default: help(); break;
    }
}

}  // namespace

namespace cli {

void poll()
{
    static String buf;
    while (Serial.available()) {
        const char ch = (char)Serial.read();
        if (ch == '\n' || ch == '\r') {
            if (buf.length()) handle(buf);
            buf = "";
        } else if (buf.length() < 120) {
            buf += ch;
        }
    }
}

}  // namespace cli
