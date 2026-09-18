#include "net.hpp"

#include <WiFi.h>
#include <esp_sntp.h>

namespace {

constexpr const char *TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

bool synced = false;

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
