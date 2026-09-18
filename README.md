# Sauerteig-Timer auf dem WT32-SC01 Plus

Ein Timer, der nach dem Zusammenmischen durch die Schrittkette eines
Sauerteigrezepts führt. Rezepte als JSON auf der SD-Karte, Zustand im NVS
(überlebt Reset und Stromausfall), Uhr per NTP. Läuft auf dem WT32-SC01 Plus
(ESP32-S3, 3.5"-Touch, LVGL 9 über LovyanGFX). Konzept: [`PLAN.md`](PLAN.md),
Details für Agenten: [`CLAUDE.md`](CLAUDE.md).

## Hardware

| Funktion | Details |
|---|---|
| MCU | ESP32-S3-WROVER, MAC `34:85:18:B1:6D:14` |
| Display | 3.5" IPS, ST7796, 320×480, **8-Bit-Parallelbus (i80)** — kein SPI |
| Touch | FT6336U (FT5x06-kompatibel), I²C |
| USB | nativer USB des S3 → `/dev/ttyACM0` (kein externer UART-Chip) |

### Pinbelegung

```
LCD  WR=47  DC/RS=0  RST=4  CS=fest GND  RD=n.c.  Backlight=45 (PWM)
     D0=9  D1=46  D2=3  D3=8  D4=18  D5=17  D6=16  D7=15
Touch SDA=6  SCL=5  INT=7   (I²C-Port 1, Adresse 0x38)
```

Weil das Panel am Parallelbus hängt, ist **LovyanGFX** die Bibliothek der Wahl —
TFT_eSPI kann i80 auf dem S3 nur mit Klimmzügen.

## Benutzung

```bash
pio run                 # bauen
pio run -t upload       # flashen
pio device monitor      # serielle Ausgabe
pio run -t upload -t monitor
```

`pio` liegt in `~/.local/bin` (installiert per `uv tool install platformio`).

### Serielle Ausgabe: nicht mit `cat` mitlesen

`cat /dev/ttyACM0` liefert **nichts**. Der USB-Serial/JTAG des ESP32-S3 sendet nur,
wenn der Host DTR gesetzt hat — `pio device monitor` (pyserial) macht das, `cat`
nicht. Wer das nicht weiß, sucht den Fehler stundenlang in der Firmware.

Dazu kommt: der native USB enumeriert beim Reset neu, der Startbanner ist also
durch, bevor ein Monitor sich anhängen kann. Deshalb gibt die Firmware alle drei
Sekunden eine `[beat]`-Zeile aus — daran sieht man auch bei spätem Anhängen sofort,
ob das Board lebt.

### udev-Regel

`/etc/udev/rules.d/70-esp32-serial.rules` gibt den Zugriff auf `/dev/ttyACM0` frei.
Die Nummer **muss unter 73** liegen: `73-seat-late.rules` wertet den `uaccess`-Tag
aus, eine höher nummerierte Regel setzt ihn zu spät.

## Einrichten

1. `include/secrets.example.hpp` nach `include/secrets.hpp` kopieren, WLAN eintragen.
2. MicroSD als FAT32 formatieren und einstecken. Beim ersten Start legt die
   Firmware `/config.json` (WLAN, `zeitraffer`, `ntfy_topic`) und
   `/rezepte/bauernbrot.json` als Vorlage an.
3. `pio run -t upload`.

Rezepte: eine Datei je Brot in `/rezepte/`, Format siehe `PLAN.md` bzw.
`sd/rezepte/bauernbrot.json`. Schrittarten: feste `dauer`, `dauer` + `runden`,
oder `offen: true`.

## Dateien

Übersicht in `CLAUDE.md`, Abschnitt „Aufbau".
