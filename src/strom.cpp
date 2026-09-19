#include "strom.hpp"

#include <ArduinoJson.h>
#include <algorithm>
#include <FS.h>
#include <SD.h>
#include <mqtt_client.h>

#include "net.hpp"
#include "sd_card.hpp"

namespace {

config::Mqtt             conf;
esp_mqtt_client_handle_t client  = nullptr;
bool                     started = false;
bool                     is_connected = false;

// ---- 24-h-Verlauf: Ringpuffer ueber Unix-Minuten ----
constexpr const char *HIST_PATH  = "/strom.bin";
constexpr uint32_t    HIST_MAGIC = 0x53545231;   // "STR1"

int16_t  hist[strom::HIST_N];   // Index = Unix-Minute % HIST_N
uint32_t hist_minute = 0;       // Unix-Minute des juengsten Buckets, 0 = leer
uint32_t acc_sum = 0;           // laufender Bucket
uint16_t acc_n   = 0;
uint32_t last_at_ms = 0;        // at_ms des zuletzt eingearbeiteten Messwerts
uint32_t sample_count = 0;
uint32_t saved_minute = 0;

bool clock_ok() { return time(nullptr) > 1704067200; }   // nach 2024-01-01
uint32_t now_minute() { return (uint32_t)(time(nullptr) / 60); }

void hist_clear()
{
    for (int16_t &v : hist) v = -1;
    hist_minute = 0;
    acc_sum = 0; acc_n = 0;
}

void hist_add(int w, uint32_t minute)
{
    if (hist_minute == 0 || minute < hist_minute || minute >= hist_minute + strom::HIST_N) {
        // leer, Uhr zurueckgesprungen oder laenger als 24 h nichts: neu anfangen
        hist_clear();
        hist_minute = minute;
    } else if (minute != hist_minute) {
        // Buckets ohne Messwert bleiben -1 (Luecke im Graph statt Nulllinie)
        for (uint32_t m = hist_minute + 1; m < minute; m++) hist[m % strom::HIST_N] = -1;
        hist_minute = minute;
        acc_sum = 0; acc_n = 0;
    }
    acc_sum += (uint32_t)(w < 0 ? 0 : w);
    acc_n++;
    hist[minute % strom::HIST_N] = (int16_t)std::min<uint32_t>(32767, acc_sum / acc_n);   // laufendes Mittel sofort sichtbar
}

void hist_save()
{
    if (!sdcard::ready()) return;
    SD.remove(HIST_PATH);
    File f = SD.open(HIST_PATH, FILE_WRITE);
    if (!f) return;
    f.write((const uint8_t *)&HIST_MAGIC, sizeof HIST_MAGIC);
    f.write((const uint8_t *)&hist_minute, sizeof hist_minute);
    f.write((const uint8_t *)hist, sizeof hist);
    f.close();
    saved_minute = hist_minute;
}

void hist_load()
{
    hist_clear();
    if (!sdcard::ready()) return;
    File f = SD.open(HIST_PATH, FILE_READ);
    if (!f) return;
    uint32_t magic = 0, minute = 0;
    const bool ok = f.read((uint8_t *)&magic, sizeof magic) == sizeof magic && magic == HIST_MAGIC
                 && f.read((uint8_t *)&minute, sizeof minute) == sizeof minute
                 && f.read((uint8_t *)hist, sizeof hist) == sizeof hist;
    f.close();
    if (!ok) { hist_clear(); Serial.println("[strom] /strom.bin unbrauchbar, Verlauf leer"); return; }
    hist_minute = minute;
    // Der letzte Bucket ist ein Mittel aus unbekannt vielen Werten; nicht weiterrechnen.
    acc_sum = 0; acc_n = 0;
    Serial.printf("[strom] Verlauf geladen, Stand vor %lu min\n",
                  clock_ok() ? (unsigned long)(now_minute() - minute) : 0UL);
}

// Der Messwert kommt aus dem MQTT-Task; kurz gesperrt lesen und schreiben.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
struct Reading { int watts = 0; float kwh = 0; uint32_t at_ms = 0; bool have = false; } reading;

// Das Telegramm heisst bei Tasmota nach dem Zaehler ("E320": {...}); den
// Namen nicht festnageln, sondern das erste Objekt mit Power_curr nehmen.
bool parse(const char *data, int len)
{
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return false;

    for (JsonPair kv : doc.as<JsonObject>()) {
        JsonObject o = kv.value().as<JsonObject>();
        if (o.isNull() || !o["Power_curr"].is<float>()) continue;
        const int   w   = (int)floorf(o["Power_curr"].as<float>());
        const float kwh = o["Total_in"].as<float>();
        portENTER_CRITICAL(&lock);
        reading.watts = w;
        reading.kwh   = kwh;
        reading.at_ms = millis();
        reading.have  = true;
        portEXIT_CRITICAL(&lock);
        return true;
    }
    return false;
}

void on_event(void *, esp_event_base_t, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
    switch ((esp_mqtt_event_id_t)id) {
        case MQTT_EVENT_CONNECTED:
            is_connected = true;
            esp_mqtt_client_subscribe(client, conf.topic.c_str(), 0);
            Serial.printf("[strom] verbunden mit %s, abonniere %s\n", conf.host.c_str(), conf.topic.c_str());
            break;
        case MQTT_EVENT_DISCONNECTED:
            is_connected = false;
            Serial.println("[strom] getrennt, verbinde neu");
            break;
        case MQTT_EVENT_ERROR: {
            const esp_mqtt_error_codes_t *e = ev->error_handle;
            if (e && e->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
                Serial.printf("[strom] Broker lehnt ab (Code %d) -- Zugangsdaten pruefen\n", (int)e->connect_return_code);
            else if (e && e->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
                Serial.printf("[strom] %s:%u nicht erreichbar (errno %d)\n", conf.host.c_str(), conf.port, e->esp_transport_sock_errno);
            else
                Serial.println("[strom] Fehler");
            break;
        }
        case MQTT_EVENT_DATA:
            // Ein Telegramm passt in einen Puffer; gestueckelte ignorieren.
            if (ev->current_data_offset == 0 && ev->data_len == ev->total_data_len) {
                if (!parse(ev->data, ev->data_len))
                    Serial.printf("[strom] Telegramm ohne Power_curr (%d Bytes)\n", ev->data_len);
            }
            break;
        default:
            break;
    }
}

}  // namespace

namespace strom {

void begin(const config::Mqtt &m)
{
    conf = m;
    hist_load();
    if (!configured()) { Serial.println("[strom] kein MQTT-Host konfiguriert -- aus"); return; }

    esp_mqtt_client_config_t c = {};
    c.host        = conf.host.c_str();
    c.port        = conf.port;
    c.username    = conf.user.length() ? conf.user.c_str() : nullptr;
    c.password    = conf.pass.length() ? conf.pass.c_str() : nullptr;
    c.client_id   = "teig-timer";
    c.buffer_size = 2048;          // Tasmota-Telegramm liegt bei einigen hundert Bytes
    c.reconnect_timeout_ms = 10000;
    client = esp_mqtt_client_init(&c);
    if (!client) { Serial.println("[strom] Client-Init fehlgeschlagen"); return; }
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, on_event, nullptr);
}

void tick()
{
    // Erst starten, wenn das WLAN steht -- sonst spammt der Client Fehler.
    if (client && !started && net::wifi_connected()) {
        started = esp_mqtt_client_start(client) == ESP_OK;
        Serial.printf("[strom] Client %s\n", started ? "gestartet" : "startet nicht");
    }

    // Neuen Messwert in den Verlauf einarbeiten -- nur mit gueltiger Uhr,
    // weil der Ringpuffer ueber Unix-Minuten laeuft.
    portENTER_CRITICAL(&lock);
    const Reading r = reading;
    portEXIT_CRITICAL(&lock);
    if (r.have && r.at_ms != last_at_ms && clock_ok()) {
        last_at_ms = r.at_ms;
        hist_add(r.watts, now_minute());
        sample_count++;
        // Alle 5 min sichern: ein Reflash oder Stromausfall kostet dann hoechstens 5 min Verlauf.
        if (hist_minute != saved_minute && hist_minute % 5 == 0) hist_save();
    }
}

void history(int16_t *out)
{
    // Relativ zur aktuellen Minute, nicht zum letzten Messwert: eine
    // Funkstille wandert so als Luecke nach links durch.
    const uint32_t now = clock_ok() ? now_minute() : hist_minute;
    for (size_t i = 0; i < HIST_N; i++) {
        const uint32_t m = now - (HIST_N - 1) + i;
        const bool have = hist_minute && m <= hist_minute && m + HIST_N > hist_minute;
        out[i] = have ? hist[m % HIST_N] : -1;
    }
}

uint32_t samples() { return sample_count; }

bool configured() { return conf.host.length() > 0 && conf.topic.length() > 0; }
bool connected()  { return is_connected; }

uint32_t age_s()
{
    portENTER_CRITICAL(&lock);
    const bool have = reading.have;
    const uint32_t at = reading.at_ms;
    portEXIT_CRITICAL(&lock);
    return have ? (millis() - at) / 1000 : UINT32_MAX;
}

bool  valid() { return age_s() <= STALE_S; }
int   watts() { portENTER_CRITICAL(&lock); const int w = reading.watts; portEXIT_CRITICAL(&lock); return w; }
float kwh()   { portENTER_CRITICAL(&lock); const float k = reading.kwh; portEXIT_CRITICAL(&lock); return k; }

String status_line()
{
    if (!configured()) return "strom=aus";
    if (!is_connected) return "strom=--";
    if (!valid())      return "strom=alt";
    return String("strom=") + watts() + "W";
}

}  // namespace strom
