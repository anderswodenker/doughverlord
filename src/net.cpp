#include "net.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sntp.h>

namespace {

constexpr const char *TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

bool synced = false;

String ntfy_server, ntfy_topic;
volatile bool push_busy = false;

struct Push { String title; String text; };

// TLS-Handshake plus POST dauern ein bis zwei Sekunden -- zu lang fuer
// loop(). Eigener Task, der sich nach dem einen Versuch selbst beendet.
void push_task(void *arg)
{
    Push *p = (Push *)arg;
    {
        JsonDocument doc;
        doc["topic"]    = ntfy_topic;
        doc["title"]    = p->title;
        doc["message"]  = p->text;
        doc["priority"] = 5;
        doc["tags"][0]  = "bread";
        String body;
        serializeJson(doc, body);

        // Ohne Zertifikatspruefung: es geht nur ein Alarmtext raus, und ein
        // CA-Bundle kostet Flash und Pflege.
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(8000);
        if (http.begin(client, ntfy_server + "/")) {
            http.addHeader("Content-Type", "application/json");
            const int code = http.POST(body);
            if (code == 200) Serial.printf("[push] gesendet: %s\n", p->title.c_str());
            else             Serial.printf("[push] fehlgeschlagen (%d): %s\n", code, http.errorToString(code).c_str());
            http.end();
        } else {
            Serial.println("[push] URL unbrauchbar");
        }
    }
    delete p;
    push_busy = false;
    vTaskDelete(nullptr);
}

void on_time_sync(struct timeval *tv)
{
    synced = true;
    char buf[32];
    struct tm tm;
    localtime_r(&tv->tv_sec, &tm);
    strftime(buf, sizeof buf, "%d.%m.%Y %H:%M:%S", &tm);
    Serial.printf("[ntp] Uhr gestellt: %s\n", buf);
}

void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[wlan] verbunden, IP %s, RSSI %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("[wlan] getrennt (Grund %u), verbinde neu\n",
                          (unsigned)info.wifi_sta_disconnected.reason);
            break;
        default:
            break;
    }
}

}  // namespace

namespace net {

void begin(const String &ssid, const String &pass)
{
    // Zeitzone gleich mit setzen; SNTP laeuft im Hintergrund, sobald das
    // WLAN steht, und stellt die Uhr danach periodisch nach.
    sntp_set_time_sync_notification_cb(on_time_sync);
    configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);          // Latenz vor Stromsparen; das Geraet haengt am Netzteil
    WiFi.setAutoReconnect(true);
    WiFi.onEvent(on_wifi_event);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("[wlan] verbinde mit \"%s\"\n", ssid.c_str());
}

bool   wifi_connected() { return WiFi.status() == WL_CONNECTED; }
String ip()             { return wifi_connected() ? WiFi.localIP().toString() : String("--"); }
int8_t rssi()           { return wifi_connected() ? (int8_t)WiFi.RSSI() : 0; }
bool   ntp_synced()     { return synced; }

void set_ntfy(const String &server, const String &topic)
{
    ntfy_server = server;
    ntfy_topic  = topic;
    while (ntfy_server.endsWith("/")) ntfy_server.remove(ntfy_server.length() - 1);
}

bool ntfy_configured() { return ntfy_topic.length() > 0; }

bool notify(const String &title, const String &text)
{
    if (!ntfy_configured()) { Serial.printf("[push] kein Topic -- %s\n", title.c_str()); return false; }
    if (!wifi_connected())  { Serial.printf("[push] kein WLAN -- %s\n", title.c_str()); return false; }
    if (push_busy)          { Serial.printf("[push] noch beschaeftigt -- %s\n", title.c_str()); return false; }
    push_busy = true;
    Push *p = new Push{ title, text };
    // 8 KB Stack: TLS braucht seinen Heap, aber wenig Stack.
    if (xTaskCreate(push_task, "push", 8192, p, 1, nullptr) != pdPASS) {
        delete p;
        push_busy = false;
        Serial.println("[push] Task startet nicht");
        return false;
    }
    return true;
}

String status_line()
{
    String s = "wlan=";
    s += wifi_connected() ? ip() : String("--");
    if (wifi_connected()) { s += " rssi="; s += rssi(); }
    s += " ntp=";
    s += synced ? "ok" : "--";
    return s;
}

}  // namespace net
