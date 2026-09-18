# WT32-SC01 Plus — Kontext für Agenten

Embedded-Projekt auf einem WT32-SC01 Plus (ESP32-S3 mit 3.5"-Touchdisplay).
Stand: **Grundgerüst läuft verifiziert auf der Hardware** — Display, Touch,
LVGL, serielle Ausgabe sind alle am echten Gerät geprüft, nicht nur kompiliert.

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

## Aufbau

- `src/LGFX_WT32SC01Plus.hpp` — LovyanGFX-Boardkonfiguration (die Fummelarbeit, erledigt)
- `src/main.cpp` — LVGL-Anbindung (Flush, Touch, Tick) und Demo-UI
- `include/lv_conf.h` — LVGL-Konfiguration
- `platformio.ini` — Board-Overrides; `esp32-s3-devkitc-1` als Basis, da PlatformIO
  das WT32-SC01 Plus nicht kennt

## Nächster Schritt: Sauerteig-Timer

**Der Plan steht in [`PLAN.md`](PLAN.md) — dort anfangen.** Er ist mit dem User
abgestimmt und enthält Datenmodell, JSON-Format der Rezepte, Oberfläche,
Dateiaufteilung, Umsetzungsreihenfolge und Verifikation.

Kurzfassung: ein Timer für Sauerteigbrot, der nach dem Zusammenmischen durch die
je nach Brotsorte unterschiedliche Kette von Schritten führt. Rezepte liegen als
JSON auf der SD-Karte, jeder Schritt wartet auf Bestätigung am Gerät, Alarm als
Vollbild plus Push aufs Handy, WLAN mit NTP.

Die Demo-UI in `main.cpp` (Farbbalken, Button, Touch-Anzeige, Helligkeitsregler)
ist reines Diagnosewerkzeug und wird dabei ersetzt.

**Noch offen:** der Backprozess selbst — welche Brotsorten, welche Schritte,
welche Dauern. Das ist das Handwerk des Users und blockiert die Umsetzung nicht,
weil Rezepte Daten sind und keine Firmware. Beim Erstellen der echten Rezepte
nachfragen statt erfinden.

**Später, optional:** Hausstrom über MQTT, interessant erst für den Moment, in
dem das Brot in den Ofen kommt. Anbindung an die vorhandene `omarchy-strom`-Bridge
des Users; Broker-Adresse und Topics sind noch nicht erfragt.
