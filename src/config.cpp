#include "config.hpp"

#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>

#include "sd_card.hpp"
#include "secrets.hpp"

namespace {

constexpr const char *CONFIG_PATH = "/config.json";

config::Config cfg;

void defaults()
{
    cfg.wifi_ssid  = SECRET_WIFI_SSID;
    cfg.wifi_pass  = SECRET_WIFI_PASS;
    cfg.zeitraffer = 1;
    cfg.ntfy_topic = "";
}

bool write()
{
    JsonDocument doc;
    doc["wlan"]["ssid"]     = cfg.wifi_ssid;
    doc["wlan"]["passwort"] = cfg.wifi_pass;
    doc["zeitraffer"]       = cfg.zeitraffer;
    doc["ntfy_topic"]       = cfg.ntfy_topic;

    SD.remove(CONFIG_PATH);   // FILE_WRITE haengt sonst an
    File f = SD.open(CONFIG_PATH, FILE_WRITE);
    if (!f) { Serial.printf("[config] konnte %s nicht schreiben\n", CONFIG_PATH); return false; }
    serializeJsonPretty(doc, f);
    f.close();
    return true;
}

}  // namespace

namespace config {

const Config &load()
{
    defaults();
    if (!sdcard::ready()) {
        Serial.println("[config] keine SD-Karte -- Vorgaben aus secrets.hpp");
        return cfg;
    }

    File f = SD.open(CONFIG_PATH, FILE_READ);
    if (!f) {
        if (write()) Serial.printf("[config] %s mit Vorgaben angelegt\n", CONFIG_PATH);
        return cfg;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[config] %s unlesbar (%s) -- Vorgaben aus secrets.hpp\n", CONFIG_PATH, err.c_str());
        return cfg;
    }

    // Jeder Schluessel einzeln optional: eine config.json mit nur
    // "zeitraffer" drin behaelt das WLAN aus den Vorgaben.
    if (doc["wlan"]["ssid"].is<const char *>())     cfg.wifi_ssid  = doc["wlan"]["ssid"].as<const char *>();
    if (doc["wlan"]["passwort"].is<const char *>()) cfg.wifi_pass  = doc["wlan"]["passwort"].as<const char *>();
    if (doc["zeitraffer"].is<uint32_t>())           cfg.zeitraffer = doc["zeitraffer"].as<uint32_t>();
    if (doc["ntfy_topic"].is<const char *>())       cfg.ntfy_topic = doc["ntfy_topic"].as<const char *>();
    if (cfg.zeitraffer == 0) cfg.zeitraffer = 1;

    Serial.printf("[config] geladen: WLAN \"%s\", Zeitraffer x%lu, ntfy %s\n",
                  cfg.wifi_ssid.c_str(), (unsigned long)cfg.zeitraffer,
                  cfg.ntfy_topic.length() ? cfg.ntfy_topic.c_str() : "aus");
    return cfg;
}

const Config &get() { return cfg; }

bool save_wifi(const String &ssid, const String &pass)
{
    if (!sdcard::ready()) return false;
    cfg.wifi_ssid = ssid;
    cfg.wifi_pass = pass;
    return write();
}

}  // namespace config
