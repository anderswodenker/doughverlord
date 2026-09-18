#include "sd_card.hpp"

#include <FS.h>
#include <SD.h>
#include <SPI.h>

namespace {

constexpr int PIN_SCK  = 39;
constexpr int PIN_MISO = 38;
constexpr int PIN_MOSI = 40;
constexpr int PIN_CS   = 41;

constexpr const char *RECIPE_DIR = "/rezepte";

// Per board_build.embed_txtfiles in platformio.ini eingebettet; der Linker
// haengt ein Nullbyte an, deshalb reicht _start als C-String.
extern "C" const char example_recipe[] asm("_binary_sd_rezepte_bauernbrot_json_start");

SPIClass sd_spi(FSPI);
bool mounted = false;

const char *card_type_name(sdcard_type_t t)
{
    switch (t) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default:        return "unbekannt";
    }
}

}  // namespace

namespace sdcard {

bool begin()
{
    // Manche Karten antworten ohne Pull-up auf MISO nicht auf CMD0.
    pinMode(PIN_MISO, INPUT_PULLUP);
    sd_spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // format_if_empty = true: eine Karte ohne lesbares Dateisystem wird mit
    // FAT formatiert. Gewollt -- die Karte ist nur fuer dieses Geraet da.
    for (int attempt = 1; attempt <= 3 && !mounted; attempt++) {
        mounted = SD.begin(PIN_CS, sd_spi, 4000000, "/sd", 5, true);
        if (!mounted) {
            Serial.printf("[sd] Mount-Versuch %d fehlgeschlagen\n", attempt);
            SD.end();
            delay(250);
        }
    }
    if (!mounted) {
        Serial.println("[sd] keine Karte erkannt oder Mount fehlgeschlagen");
        return false;
    }

    const uint64_t total_mb = SD.totalBytes() / (1024 * 1024);
    const uint64_t used_mb  = SD.usedBytes()  / (1024 * 1024);
    Serial.printf("[sd] Karte gemountet: %s, %llu MB gesamt, %llu MB belegt\n",
                  card_type_name(SD.cardType()), total_mb, used_mb);
    return true;
}

bool ready() { return mounted; }

void list(const char *path, uint8_t depth)
{
    if (!mounted) return;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[sd] %s: kein Verzeichnis\n", path);
        return;
    }

    if (depth == 0) Serial.printf("[sd] Inhalt von %s:\n", path);

    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        Serial.printf("[sd] %*s%s%s", depth * 2 + 2, "", f.name(),
                      f.isDirectory() ? "/" : "");
        if (!f.isDirectory()) Serial.printf("  (%lu Bytes)", (unsigned long)f.size());
        Serial.println();

        if (f.isDirectory() && depth < 3) {
            String sub = String(path);
            if (!sub.endsWith("/")) sub += "/";
            sub += f.name();
            list(sub.c_str(), depth + 1);
        }
    }
    dir.close();
}

void ensure_example_recipe()
{
    if (!mounted) return;

    if (!SD.exists(RECIPE_DIR)) {
        if (!SD.mkdir(RECIPE_DIR)) {
            Serial.printf("[sd] konnte %s nicht anlegen\n", RECIPE_DIR);
            return;
        }
        Serial.printf("[sd] %s angelegt\n", RECIPE_DIR);
    }

    // Nur eingreifen, wenn wirklich nichts drin ist: eigene Rezepte des Users
    // duerfen nie ueberschrieben werden.
    File dir = SD.open(RECIPE_DIR);
    const bool empty = dir && !dir.openNextFile();
    dir.close();
    if (!empty) return;

    const String path = String(RECIPE_DIR) + "/bauernbrot.json";
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[sd] konnte %s nicht schreiben\n", path.c_str());
        return;
    }
    const size_t len = strlen(example_recipe);
    const size_t written = f.print(example_recipe);
    f.close();
    Serial.printf("[sd] Beispielrezept nach %s geschrieben (%u/%u Bytes)\n",
                  path.c_str(), (unsigned)written, (unsigned)len);
}

}  // namespace sdcard
