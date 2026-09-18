#include "cli.hpp"

#include <Arduino.h>
#include <sys/time.h>

#include "config.hpp"
#include "net.hpp"
#include "recipe.hpp"
#include "session.hpp"

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
    Serial.println("[cli]   n           Netzstatus");
}

void handle(String line)
{
    line.trim();
    if (!line.length()) return;
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
        case 'n': Serial.printf("[cli] %s\n", net::status_line().c_str()); break;
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
