# WT32-SC01 Plus — Kontext für Agenten

Embedded-Projekt auf einem WT32-SC01 Plus (ESP32-S3 mit 3.5"-Touchdisplay):
ein **Sauerteig-Timer**, der nach dem Zusammenmischen durch die Schrittkette
eines Rezepts führt. Plan und Begründungen in [`PLAN.md`](PLAN.md).

## Stand (2026-09-19)

Schritte 1–5 aus `PLAN.md` sind **fertig und am Gerät verifiziert**: SD-Karte,
Rezept-Parser, Sitzungslogik mit NVS-Persistenz, WLAN/NTP/`config.json`,
Screens 1–3 (Auswahl, Zutaten, laufender Timer). Die Demo-UI ist weg.

Dazu (außerhalb des Plans, 2026-09-19): **Dashboard als Startscreen** ohne
laufenden Teig — Uhr, Datum, Hausverbrauch vom Stromzähler per MQTT
(`src/strom.cpp`, `esp_mqtt` aus dem IDF, kein Zusatz-Lib), Knopf „Backen" zur
Rezeptauswahl; am Gerät mit dem echten Broker verifiziert (Wert alle ~15 s).
Die Zugangsdaten stehen in `~/Development/strom/app.py` des Users und sind per
CLI `m …` in der `config.json` der Karte. Außerdem **Abbruch über die UI**: das
X im Timer-Header fragt nach (`ui::confirm()`) und führt zurück aufs Dashboard —
damit ist der Teil von Schritt 7 erledigt.

**Schritt 6 ist fertig** (2026-09-19, am Gerät im Zeitraffer verifiziert):
Vollbild-Alarm (`screen_alarm.cpp`), Push per **ntfy** (`net::notify()`, JSON-POST
in eigenem Task), Dimmen nach 60 s auf `BRIGHT_DIM`, Alarm hält volle Helligkeit.
Das Push-Topic in der `config.json` ist noch ein **Wegwerf-Topic vom Test**
(`teigtimer-test-…`) — der User trägt seins per CLI `ntfy <topic>` ein.

**Offen:** Schritt 7 (Schrittliste als Overlay, Status-Screen), Schritt 8 (Doku).
Dimmen bleibt drin, auch wenn es ein IPS ist: das Gerät steht nachts in der Küche.

## Hardware (am 2026-09-18 per esptool ausgelesen, nicht geraten)

- ESP32-S3, Revision v0.2, **16 MB Quad-Flash, 2 MB eingebettetes PSRAM**
- MAC `34:85:18:b1:6d:14`
- Display ST7796, 320×480, an einem **8-Bit-Parallelbus (i80)** — kein SPI.
  Deshalb LovyanGFX; TFT_eSPI kann i80 auf dem S3 nur mit Klimmzügen.
- Touch FT6336U über I²C, mit dem FT5x06-Treiber angesprochen
- Verbindung über den **nativen USB des S3** → `/dev/ttyACM0`, kein UART-Chip

Die Pinbelegung steht in `src/LGFX_WT32SC01Plus.hpp` und ist am Gerät bestätigt
(Farbtest und Touch stimmen). **Nicht ohne Not anfassen.**

## Toolchain

PlatformIO liegt **nicht** systemweit, sondern isoliert:

```bash
export PATH="$HOME/.local/bin:$PATH"   # pio, installiert per: uv tool install platformio --python 3.12
pio run                # bauen
pio run -t upload      # flashen
pio device monitor     # serielle Ausgabe
```

Arch' System-Python (3.14) ist bewusst unangetastet — PlatformIO läuft unter
einem eigenen Python 3.12 aus dem uv-Tool-Env.

## Drei Fallen, die schon Zeit gekostet haben

1. **`cat /dev/ttyACM0` liefert nichts.** Der USB-Serial/JTAG des S3 sendet nur,
   wenn der Host DTR setzt. `pio device monitor` (pyserial) tut das, `cat` nicht.
   Das Symptom sieht exakt aus wie eine hängende Firmware — ist es aber nicht.
   Zum Gegenprüfen: pyserial öffnen und `dtr = True` setzen.

2. **Der Startbanner ist beim Anhängen immer schon durch,** weil der native USB
   beim Reset neu enumeriert. Deshalb gibt `loop()` alle 3 s eine `[beat]`-Zeile
   mit Uptime/Heap/PSRAM/Klicks aus. Diesen Heartbeat bitte erhalten.

3. **udev-Regel muss unter 73 nummeriert sein.** `/etc/udev/rules.d/70-esp32-serial.rules`
   gibt `/dev/ttyACM0` frei. `73-seat-late.rules` wertet den `uaccess`-Tag aus —
   eine `99-`Regel setzt ihn zu spät und wirkt nie. Neu anwenden mit
   `sudo udevadm trigger --action=add` (ein `change`-Event genügt nicht).
   `sudo` verlangt hier ein Passwort, ein Agent kann es nicht selbst ausführen:
   dem User den Befehl zum Ausführen geben.

## Grafik-Stack

- LovyanGFX 1.2.29, LVGL 9.6.0
- `include/lv_conf.h` ist aus dem **versionsgleichen** `lv_conf_template.h` erzeugt
  und gepatcht. Bei einem LVGL-Update wieder so vorgehen, nicht von Hand pflegen.
- LVGL 9.6 kennt **kein `LV_COLOR_DEPTH` mehr** — das Farbformat kommt aus
  `LV_COLOR_FORMAT_DEFAULT` (steht auf `RGB565`).
- Der Cast im Flush-Callback auf `lgfx::rgb565_t*` ist korrekt, weil dieser Typ
  bitidentisch zu LVGL's `RGB565` ist (`b:5, g:6, r:5`). Kein Byte-Swap nötig.

- Die eingebauten `lv_font_montserrat_*` können **keine Umlaute** (nur ASCII
  + `°` + Symbole). Deshalb eigene Fonts in `src/fonts/` (`font_ms_16..48`
  mit Latin-1, `font_ms_96` nur Ziffern für die Restzeit), erzeugt von
  `tools/gen_fonts.sh` per `npx lv_font_conv` aus dem Montserrat, das LVGL
  mitbringt. Eingebaut bleibt nur `montserrat_16` wegen `LV_SYMBOL_*`.
- `lv_snapshot` rendert nur den Screen, nicht die System-Layer — ein
  Performance-Monitor wäre im Screenshot unsichtbar (ist deshalb abgeschaltet).

## Aufbau

| Datei | Aufgabe |
|---|---|
| `src/LGFX_WT32SC01Plus.hpp` | LovyanGFX-Boardkonfig — **unverändert lassen** |
| `src/display.{hpp,cpp}` | LVGL-Anbindung (Flush, Touch, Tick), Helligkeit, `idle_ms()` |
| `src/sd_card.{hpp,cpp}` | SD an SPI (CLK 39, MISO 38, MOSI 40, CS 41), Listing, Beispielrezept anlegen |
| `src/recipe.{hpp,cpp}` | JSON → `Recipe`/`Step`/`Ingredient`, `parse_duration("1h30min")`, `format_duration()` |
| `src/session.{hpp,cpp}` | Zustandsautomat (läuft/wartet/fertig), Runden, NVS, Restzeit, ETA, Zeitraffer, Ereignisse |
| `src/config.{hpp,cpp}` | `/config.json` von SD: WLAN, `zeitraffer`, `ntfy_topic`, `ntfy_server`, `mqtt`; legt sie aus `secrets.hpp` an, trägt fehlenden `mqtt`-Block nach |
| `src/net.{hpp,cpp}` | WLAN mit Auto-Reconnect, SNTP (Zeitzone Berlin), Statuszeile, `notify()` per ntfy in eigenem Task |
| `src/strom.{hpp,cpp}` | Stromzähler per MQTT (`esp_mqtt`, eigener Task): letzter Wert, Alter, Status; `Power_curr`/`Total_in` aus dem ersten Objekt, das sie hat |
| `src/ui/` | `ui.cpp` (Navigation, 1-Hz-Refresh, Dimmen), `common.cpp` (Farben, Header, Buttons, `confirm()`-Overlay), `screen_*.cpp` (`home` = Dashboard, `alarm` = Vollbild-Alarm) |
| `src/cli.{hpp,cpp}` | serielle Kommandos, siehe unten |
| `src/screenshot.{hpp,cpp}` | Screen als Base64 über USB |
| `include/secrets.hpp` | WLAN-Zugang, **gitignored**; Vorlage `secrets.example.hpp` |
| `sd/rezepte/bauernbrot.json` | Beispielrezept; per `embed_txtfiles` in der Firmware, wird auf leere Karten geschrieben |
| `tools/` | `gen_fonts.sh`, `screenshot.py` |

`main.cpp` verdrahtet nur noch. Reihenfolge in `setup()`: Display → SD →
Config → Net (setzt TZ) → Strom → Session → UI.

### Verhalten, das nicht offensichtlich ist

- Sitzungszustand liegt im NVS (`Preferences`, Namespace `teig`) und wird nur
  bei Zustandswechseln geschrieben. Startzeit ist Unix-Zeit; ohne gültige Uhr
  (vor 2024) wird `startzeit = 0` gespeichert, Restzeit läuft dann über
  `millis()` und die Startzeit wird nachgetragen, sobald NTP antwortet.
- Die RTC hält die Uhr über einen Reset (auch `pio run -t upload`), nur ein
  Stromausfall verliert sie.
- `Waiting` heißt Alarm, wenn `current_step()->offen == false` (Timer abgelaufen),
  sonst ein offener Schritt. Kein eigener Zustand dafür.
- `zeitraffer` aus `config.json` teilt alle Dauern; im Timer-Screen rot markiert.
- `config.json` fehlt → wird aus `secrets.hpp` angelegt. Danach gilt die Karte.
- Navigation: ohne Teig Dashboard (Home) → „Backen" → Auswahl → Zutaten → Timer;
  `ui::tick()` erzwingt Timer bei laufendem Teig und Home ohne. Nach „Brot
  fertig" landet man auf dem Dashboard.
- Der MQTT-Callback läuft im Task von `esp_mqtt`; der Messwert wird unter
  einem `portMUX` kopiert. Werte älter als 90 s gelten als veraltet (wie das
  Bar-Widget). Der Client startet erst, wenn WLAN steht (`strom::tick()`).
- Screens setzen `refresh()` **nach** `lv_screen_load()` ab — die Aktiv-Prüfung
  in `refresh()` greift sonst und der Screen bleibt leer.
- Alarm ist kein Zustand, sondern `ui::alarm_active()` = `Waiting` und Schritt
  nicht offen; `ui::tick()` schaltet danach um. Der Push hängt am Ereignis
  `Expired`/`Finished` in `loop()` — nach einem Reset mit schon abgelaufenem
  Timer feuert `Expired` erneut, also auch der Push (gewollt).
- `net::notify()` postet JSON an `ntfy_server/` (nicht an `/topic`), damit
  Umlaute im Titel gehen; TLS ohne Zertifikatsprüfung (`setInsecure`).
  Ein Push zur Zeit; läuft noch einer, wird der neue nur geloggt.
- Dimmen sitzt in `ui::tick()`: `idle_ms() > 60 s` und kein Alarm →
  `BRIGHT_DIM`, sonst `BRIGHT_FULL`. Die erste Berührung im Dunkeln kommt
  auch als Klick an — bei 15 % ist der Screen aber lesbar.
- `ui::confirm()` legt das Overlay auf den **aktiven Screen** (nicht
  `lv_layer_top()`), damit es mit einem Screenwechsel durch `ui::tick()`
  verschwindet und nicht über dem nächsten Screen hängen bleibt.

## Am Gerät arbeiten, ohne hinzuschauen

Serielles CLI (Zeile + `\n` über `/dev/ttyACM0`, DTR muss gesetzt sein):

```
s <datei>   Rezept starten, z. B. s bauernbrot.json
c           Erledigt          x  Teig verwerfen        p  Status
l           Rezepte listen    f <n>  Zeitraffer (nur RAM, bis Reset)
t <unix>    Uhr stellen       w <ssid> <pass>  WLAN in config.json, Neustart
n           Status (Netz, Strom, ntfy, Helligkeit)
u h|s|z|t|a|l  Screen anspringen (Dashboard/Auswahl/Zutaten/Timer/Abbruch-Rückfrage/Alarm)
m <host> <port> <user|-> <pass|-> <topic>   Stromzähler-MQTT in config.json, Neustart
m -         Stromzähler aus
ntfy <topic>|-  Push-Topic in config.json (gilt sofort)     push [text]  Test-Push
b <0-255>   Helligkeit setzen (ui::tick holt sie nach 1 s zurück)
shot        Screenshot als Base64
```

`tools/screenshot.py out.png ["u h"]` holt den Screenshot (≈1,2 s); das optionale
Kommando geht 150 ms vorher raus — so kriegt man auch Screens, die `ui::tick()`
eine Sekunde später wieder wegschaltet (Dashboard bei laufendem Teig). Python dafür:
`~/.local/share/uv/tools/platformio/bin/python` (hat pyserial und Pillow).
Rezept-Parser-Selbsttest: `PLATFORMIO_BUILD_FLAGS="-DRECIPE_SELFTEST" pio run -t upload`.

Zum Verifizieren nach Änderungen hat sich bewährt: Skript, das per pyserial
Kommandos schickt, Board per RTS resettet und `[teig]`-Zeilen mitliest —
so wurden Resets mitten im Schritt und der ganze Ablauf im Zeitraffer geprüft.

## Weiter mit Schritt 7

Plan in `PLAN.md`, Abschnitt „Umsetzung". Schrittliste als Overlay: im Timer
tippt man aufs Zentrum (`center_cb`), das zeigt bisher die Zutaten. Status-Screen:
`net::status_line()`, `strom::status_line()`, `net::ntfy_configured()`,
`sdcard::ready()` liefern alles, was draufgehört. Zum Prüfen des Pushs ohne
Handy: `curl -s "https://ntfy.sh/<topic>/json?poll=1&since=15m"`.

**Noch offen bleibt der Backprozess selbst** — welche Brotsorten, Schritte,
Dauern. Das ist das Handwerk des Users. Beim Erstellen der echten Rezepte
nachfragen statt erfinden; `bauernbrot.json` ist nur eine Vorlage.

**Später, optional:** Hausstrom über MQTT, interessant erst für den Moment, in
dem das Brot in den Ofen kommt. Anbindung an die vorhandene `omarchy-strom`-Bridge
des Users; Broker-Adresse und Topics sind noch nicht erfragt.
