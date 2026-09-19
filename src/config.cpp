#include "config.hpp"

#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>

#include "sd_card.hpp"
#include "secrets.hpp"

// Aeltere secrets.hpp kennen den MQTT-Block noch nicht -- dann bleibt er leer
// und wird ueber /config.json auf der Karte gepflegt.
#ifndef SECRET_MQTT_HOST
#define SECRET_MQTT_HOST ""
#endif
#ifndef SECRET_MQTT_PORT
#define SECRET_MQTT_PORT 1883
#endif
#ifndef SECRET_MQTT_USER
#define SECRET_MQTT_USER ""
#endif
#ifndef SECRET_MQTT_PASS
#define SECRET_MQTT_PASS ""
#endif
#ifndef SECRET_MQTT_TOPIC
#define SECRET_MQTT_TOPIC ""
#endif

namespace {

constexpr const char *CONFIG_PATH = "/config.json";

config::Config cfg;

void defaults()
{
    cfg.wifi_ssid  = SECRET_WIFI_SSID;
    cfg.wifi_pass  = SECRET_WIFI_PASS;
    cfg.zeitraffer = 1;
    cfg.ntfy_topic = "";
    cfg.ntfy_server = "https://ntfy.sh";
    cfg.mqtt.host  = SECRET_MQTT_HOST;
    cfg.mqtt.port  = SECRET_MQTT_PORT;
    cfg.mqtt.user  = SECRET_MQTT_USER;
    cfg.mqtt.pass  = SECRET_MQTT_PASS;
    cfg.mqtt.topic = SECRET_MQTT_TOPIC;
}

bool write()
{
    JsonDocument doc;
    doc["wlan"]["ssid"]     = cfg.wifi_ssid;
    doc["wlan"]["passwort"] = cfg.wifi_pass;
    doc["zeitraffer"]       = cfg.zeitraffer;
    doc["ntfy_topic"]       = cfg.ntfy_topic;
    doc["ntfy_server"]      = cfg.ntfy_server;
    doc["mqtt"]["host"]     = cfg.mqtt.host;
    doc["mqtt"]["port"]     = cfg.mqtt.port;
    doc["mqtt"]["user"]     = cfg.mqtt.user;
    doc["mqtt"]["passwort"] = cfg.mqtt.pass;
    doc["mqtt"]["topic"]    = cfg.mqtt.topic;

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
    if (doc["ntfy_server"].is<const char *>())      cfg.ntfy_server = doc["ntfy_server"].as<const char *>();
    if (!cfg.ntfy_server.length()) cfg.ntfy_server = "https://ntfy.sh";
    if (cfg.zeitraffer == 0) cfg.zeitraffer = 1;

    JsonObject mq = doc["mqtt"];
    if (mq["host"].is<const char *>())     cfg.mqtt.host  = mq["host"].as<const char *>();
    if (mq["port"].is<uint16_t>())         cfg.mqtt.port  = mq["port"].as<uint16_t>();
    if (mq["user"].is<const char *>())     cfg.mqtt.user  = mq["user"].as<const char *>();
    if (mq["passwort"].is<const char *>()) cfg.mqtt.pass  = mq["passwort"].as<const char *>();
    if (mq["topic"].is<const char *>())    cfg.mqtt.topic = mq["topic"].as<const char *>();
    if (cfg.mqtt.port == 0) cfg.mqtt.port = 1883;

    // Aeltere Karten haben den Block noch nicht: leer nachtragen, damit man
    // am Rechner sieht, wo die Zugangsdaten hingehoeren.
    if (mq.isNull() && write()) Serial.printf("[config] mqtt-Block in %s nachgetragen\n", CONFIG_PATH);

    Serial.printf("[config] geladen: WLAN \"%s\", Zeitraffer x%lu, ntfy %s, mqtt %s\n",
                  cfg.wifi_ssid.c_str(), (unsigned long)cfg.zeitraffer,
                  cfg.ntfy_topic.length() ? cfg.ntfy_topic.c_str() : "aus",
                  cfg.mqtt.host.length() ? (cfg.mqtt.host + ":" + cfg.mqtt.port + " " + cfg.mqtt.topic).c_str() : "aus");
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

bool save_ntfy(const String &topic)
{
    if (!sdcard::ready()) return false;
    cfg.ntfy_topic = topic;
    return write();
}

bool save_mqtt(const Mqtt &m)
{
    if (!sdcard::ready()) return false;
    cfg.mqtt = m;
    if (cfg.mqtt.port == 0) cfg.mqtt.port = 1883;
    return write();
}

}  // namespace config
