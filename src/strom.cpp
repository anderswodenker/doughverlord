#include "strom.hpp"

#include <ArduinoJson.h>
#include <mqtt_client.h>

#include "net.hpp"

namespace {

config::Mqtt             conf;
esp_mqtt_client_handle_t client  = nullptr;
bool                     started = false;
bool                     is_connected = false;

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
}

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
