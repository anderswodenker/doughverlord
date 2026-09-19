// Sauerteig-Timer auf dem WT32-SC01 Plus.
//
// main.cpp verdrahtet nur: Display, SD-Karte, Konfiguration, Netz, Stromzaehler,
// Sitzung, UI. Die Logik steckt in den jeweiligen Modulen.

#include <Arduino.h>
#include <lvgl.h>

#include "cli.hpp"
#include "config.hpp"
#include "display.hpp"
#include "net.hpp"
#include "recipe.hpp"
#include "sd_card.hpp"
#include "session.hpp"
#include "strom.hpp"
#include "ui/ui.hpp"

static void log_board_info(void)
{
    Serial.printf("[sys] Chip:  %s rev %d, %d Kerne @ %lu MHz\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                  (unsigned long)getCpuFrequencyMhz());
    Serial.printf("[sys] Flash: %lu Bytes\n", (unsigned long)ESP.getFlashChipSize());
    Serial.printf("[sys] PSRAM: %lu Bytes (frei: %lu)\n",
                  (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram());
    Serial.printf("[sys] Heap:  %lu Bytes frei\n", (unsigned long)ESP.getFreeHeap());
}

void setup()
{
    Serial.begin(115200);
    // Der native USB des S3 enumeriert erst nach dem Boot neu. Kurz auf einen
    // angehaengten Monitor warten, sonst verpufft der Startbanner ins Leere.
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) delay(10);
    delay(200);
    Serial.println("\n=== WT32-SC01 Plus startet ===");
    log_board_info();

    display::begin();

    // SD-Karte erst nach dem Display: laeuft die Karte nicht, soll das
    // Geraet trotzdem etwas anzeigen und der Heartbeat weiterlaufen.
    if (sdcard::begin()) {
        sdcard::ensure_example_recipe();
        sdcard::list("/");
    }

#ifdef RECIPE_SELFTEST
    recipe::selftest();
#endif

    // Reihenfolge: Konfiguration -> Netz (setzt die Zeitzone) -> Sitzung -> UI.
    const config::Config &cfg = config::load();
    net::begin(cfg.wifi_ssid, cfg.wifi_pass);
    strom::begin(cfg.mqtt);
    session::set_time_factor(cfg.zeitraffer);
    session::begin();
    session::log_status();

    ui::begin();
    Serial.println("[ui] aufgebaut");
}

void loop()
{
    lv_timer_handler();
    cli::poll();
    session::tick();
    strom::tick();
    ui::tick();

    // Ereignisse vorerst nur loggen; Alarm-Screen und Push kommen in Schritt 6.
    switch (session::take_event()) {
        case session::Event::Expired:  Serial.println("[ereignis] Alarm");   break;
        case session::Event::Finished: Serial.println("[ereignis] Fertig");  break;
        default: break;
    }

    // Lebenszeichen: ein spaeter angehaengter Monitor sieht sonst gar nichts,
    // weil der Startbanner beim USB-Reset schon durch ist.
    static uint32_t last_beat = 0;
    if (millis() - last_beat > 3000) {
        last_beat = millis();
        Serial.printf("[beat] t=%lus  heap=%lu  psram=%lu  sd=%s  %s %s\n",
                      (unsigned long)(millis() / 1000),
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getFreePsram(),
                      sdcard::ready() ? "ok" : "fehlt",
                      net::status_line().c_str(), strom::status_line().c_str());
        if (session::active()) session::log_status();
    }

    delay(5);
}
