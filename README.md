# WT32-SC01 Plus — LVGL-Grundgerüst

Display + Touch + LVGL 9 auf dem WT32-SC01 Plus (ESP32-S3-WROVER).

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

## Was das Grundgerüst zeigt

- Farbtest R/G/B/Weiß — prüft Farbreihenfolge und `invert`-Einstellung
- Button mit Klickzähler — prüft LVGL-Eventpfad
- rohe Touch-Koordinaten — prüft Touch-Achsen gegen die Rotation
- Helligkeitsregler — prüft die PWM-Hintergrundbeleuchtung
- Chip-/Flash-/PSRAM-Infos auf der seriellen Konsole

## Dateien

- `src/LGFX_WT32SC01Plus.hpp` — LovyanGFX-Boardkonfiguration (die eigentliche Fummelarbeit)
- `src/main.cpp` — LVGL-Anbindung (Flush, Touch, Tick) und Demo-UI
- `include/lv_conf.h` — LVGL-Konfiguration
