# Doughverlord — Sauerteig-Timer auf dem WT32-SC01 Plus

## Kontext

Auf dem WT32-SC01 Plus läuft ein verifiziertes Grundgerüst (Display, Touch, LVGL 9.6
über LovyanGFX). Die aktuelle UI ist reines Diagnosewerkzeug und wird durch die
eigentliche Anwendung ersetzt.

Gebraucht wird ein Timer für Sauerteigbrot: Nach dem Zusammenmischen der Zutaten
führt er durch eine Kette von Schritten, die je nach Brotsorte unterschiedlich
aussieht. Ein einzelner Küchenwecker reicht dafür nicht — die Abfolge ist das
Eigentliche.

Aus der Sache selbst folgen drei Anforderungen, die die Architektur prägen:

- **Die Abläufe dauern Stunden bis über Nacht.** Ein Reset oder Stromausfall darf
  einen laufenden Teig nicht verlieren.
- **Niemand steht daneben.** Der Timer muss rufen, nicht nur anzeigen.
- **Gelesen wird aus der Küche quer durch den Raum.** Die Restzeit ist das,
  was zählt.

### Entschieden

| | |
|---|---|
| Rezepte | JSON-Dateien auf der MicroSD-Karte |
| Schrittwechsel | erst nach Bestätigung am Gerät |
| Alarm | Vollbild auf dem Display + Push aufs Handy |
| Netz | WLAN mit NTP |
| Layout | Große Restzeit, Kontext in schmalen Zeilen oben und unten |
| Umfang | ein Teig zur Zeit |

### Noch offen

**Der Backprozess selbst.** Welche Brotsorten, welche Schritte, welche Dauern —
das ist das Handwerk des Users und noch nicht beschrieben. Das blockiert die
Umsetzung **nicht**: Rezepte sind Daten auf der SD-Karte, keine Firmware. Die
Schrittarten unten sind so gewählt, dass sie den üblichen Sauerteig-Ablauf
abdecken (feste Dauern, wiederkehrende Intervalle, Entscheidungen nach
Augenschein). Ausgeliefert wird ein Beispielrezept als Vorlage; die echten
Rezepte schreibt der User anschließend selbst oder gemeinsam mit einem Agenten.

## Datenmodell

Ein Rezept ist eine Datei `/rezepte/<name>.json` auf der SD-Karte:

```json
{
  "name": "Bauernbrot",
  "zutaten": [
    { "menge": "300 g", "was": "Weizenmehl 550" },
    { "menge": "200 g", "was": "Roggenmehl 1050" },
    { "menge": "350 g", "was": "Wasser, lauwarm" },
    { "menge": "100 g", "was": "Anstellgut, aktiv" }
  ],
  "schritte": [
    { "name": "Autolyse",          "dauer": "45min" },
    { "name": "Salz einarbeiten",  "offen": true,
      "hinweis": "bis der Teig sich glatt anfühlt",
      "zutaten": [ { "menge": "12 g", "was": "Salz" } ] },
    { "name": "Dehnen und Falten", "dauer": "30min", "runden": 4 },
    { "name": "Stockgare",         "dauer": "3h" },
    { "name": "Formen",            "offen": true },
    { "name": "Stückgare",         "dauer": "12h" },
    { "name": "Backen",            "dauer": "45min" }
  ]
}
```

`menge` und `was` sind bewusst getrennt: so lassen sich die Mengen rechtsbündig
untereinander setzen, was eine Liste beim Abwiegen deutlich lesbarer macht. (Und
falls später einmal eine doppelte Menge gebraucht wird, ist die Struktur schon da
— gebaut wird das jetzt nicht.)

Ein Schritt darf **eigene Zutaten** tragen. Das ist kein Beiwerk: wenn der Alarm
für „Salz einarbeiten" losgeht, gehören die 12 g direkt auf den Alarm-Screen und
nicht in eine Liste, die man erst suchen muss.

Drei Schrittarten decken den Ablauf ab:

- **Feste Dauer** — `dauer` läuft ab, dann Alarm.
- **Wiederkehrend** — `dauer` + `runden`: viermal 30 min mit Alarm dazwischen,
  angezeigt als „Runde 2 von 4". Bildet Dehnen und Falten ab, ohne dass der User
  vier Einträge schreiben muss.
- **Offen** — `offen: true`, kein Timer. Der Schritt wartet auf Bestätigung.
  Für alles, was nach Augenschein statt nach Uhr entschieden wird.

`dauer` wird als `"45min"`, `"3h"`, `"1h30min"` geschrieben — lesbar, und der
Parser ist trivial. `hinweis` ist optional und erscheint klein unter dem
Schrittnamen.

Jeder Schritt wartet nach Ablauf auf Bestätigung; der nächste startet erst dann.
Damit bleibt die geplante Endzeit ehrlich, auch wenn der User erst eine halbe
Stunde später in die Küche kommt.

## Persistenz — der kritische Teil

Der Sitzungszustand gehört ins **NVS** (`Preferences`), nicht auf die SD-Karte:
wenige Bytes, Wear-Levelling eingebaut, unabhängig davon ob die Karte steckt.

Gespeichert wird bei **jedem Zustandswechsel**, nicht sekündlich — sonst wird der
Flash über Nacht unnötig geschrieben:

```
rezept       Dateiname
schritt      Index
runde        Index
start        Unix-Zeit des laufenden Schritts
zustand      läuft | wartet_auf_bestätigung | fertig
```

**Deshalb ist NTP tragend, nicht kosmetisch:** `millis()` beginnt nach einem Reset
wieder bei null. Nur mit absoluter Zeit lässt sich nach einem Stromausfall
ausrechnen, wie weit die Stückgare wirklich ist. Ohne gültige Zeit darf die App
keine Restzeit erfinden — dann zeigt sie „Zeit wird synchronisiert" und rechnet
nach, sobald NTP antwortet.

## Oberfläche

Fünf Screens, LVGL:

**1. Rezeptauswahl** — Liste der JSON-Dateien von der SD-Karte, groß genug zum
Antippen. Läuft bereits ein Teig, wird stattdessen direkt Screen 3 gezeigt.

**2. Zutaten** — erscheint nach der Auswahl, vor dem ersten Schritt. Mengen
rechtsbündig, darunter ein Knopf `Zusammengemischt — los`. Das ist der
Startpunkt des Ablaufs und entspricht dem tatsächlichen Hergang: erst wiegen und
mischen, dann läuft die Uhr. Der Screen bleibt während des Ablaufs über die
Schrittliste erreichbar, zum Nachschlagen.

```
┌──────────────────────────────────────────────────────┐
│ Bauernbrot · Zutaten                      21:14 Uhr  │
├──────────────────────────────────────────────────────┤
│    300 g   Weizenmehl 550                            │
│    200 g   Roggenmehl 1050                           │
│    350 g   Wasser, lauwarm                           │
│    100 g   Anstellgut, aktiv                         │
│     12 g   Salz         (bei „Salz einarbeiten")     │
├──────────────────────────────────────────────────────┤
│         ┌────────────────────────────┐               │
│         │  Zusammengemischt — los    │               │
│         └────────────────────────────┘               │
└──────────────────────────────────────────────────────┘
```

Zutaten, die erst zu einem späteren Schritt gehören, stehen mit diesem Vermerk
dabei — man wiegt sie gern gleich mit ab, darf sie aber nicht zu früh
einrühren.

**3. Laufender Timer** — das gewählte Layout:

```
┌──────────────────────────────────────────────────────┐
│ Bauernbrot · Schritt 3/7         ● WLAN   21:14 Uhr  │
├──────────────────────────────────────────────────────┤
│               Dehnen und Falten                      │
│                  28:41                               │
│  ████████████████████░░░░░░░░░░░░░  Runde 2 von 4    │
├──────────────────────────────────────────────────────┤
│ Danach: Stockgare 3 h         Brot fertig ca. 06:20  │
└──────────────────────────────────────────────────────┘
```

Die Restzeit trägt den Screen. „Brot fertig ca." summiert die verbleibenden
Schritte auf die aktuelle Uhrzeit — bei Übernacht-Führung der eigentlich
nützliche Wert. Ein Tippen aufs Zentrum öffnet die vollständige Schrittliste als
Overlay, damit der Überblick nicht verloren geht.

**4. Vollbild-Alarm** — großer Schrittname, kräftige Farbe, pulsierend, Display
auf volle Helligkeit. Trägt der nun anstehende Schritt Zutaten, stehen sie hier
mit Menge; trägt er einen `hinweis`, steht der darunter. Ein `Erledigt`-Knopf
über die volle Breite, damit man ihn mit Teig an den Fingern trifft. Bleibt
stehen, bis bestätigt wird.

**5. Status** — WLAN, Zeitsynchronisation, SD-Karte, Push-Ziel. Klein, aber es
erspart die Fehlersuche über die serielle Konsole.

### Helligkeit

Nach 60 s ohne Berührung auf ~15 % dimmen, bei Berührung und bei Alarm auf volle
Helligkeit. Grund ist der nächtliche Betrieb in der Küche, **nicht** Einbrennen:
das ist ein IPS-LCD, kein OLED. (Die aktuelle `CLAUDE.md` behauptet an einer
Stelle das Gegenteil — beim Umsetzen mitkorrigieren.)

## Aufbau

Aus der einen `main.cpp` werden getrennte Einheiten. `main.cpp` behält nur
`setup()`/`loop()` und die Verdrahtung.

| Datei | Aufgabe |
|---|---|
| `src/LGFX_WT32SC01Plus.hpp` | **unverändert** — am Gerät verifiziert |
| `src/recipe.{hpp,cpp}` | JSON von SD lesen, Schrittliste aufbauen, Dauern parsen |
| `src/session.{hpp,cpp}` | Ablaufzustand, Fortschaltlogik, NVS-Persistenz |
| `src/net.{hpp,cpp}` | WLAN, NTP, Push |
| `src/ui/*.{hpp,cpp}` | die fünf Screens, je eine Datei |
| `src/config.{hpp,cpp}` | `/config.json` von SD: WLAN-Zugang, Push-Ziel |

**Zugangsdaten gehören auf die SD-Karte**, nicht in den Quelltext — dann kann das
Repo bedenkenlos öffentlich werden.

Neue Abhängigkeiten: `bblanchon/ArduinoJson` für Rezepte und Konfiguration. SD
und WLAN kommen aus dem Arduino-Core. Kein Konflikt mit dem Display: die SD hängt
an SPI (CLK 39, MISO 38, MOSI 40, CS 41), das Panel am Parallelbus.

In `include/lv_conf.h` muss **Montserrat 48** dazu (aktuell nur 16/20/24) — sonst
gibt es keine große Restzeit. Die Datei wird weiterhin aus dem versionsgleichen
Template erzeugt, nicht von Hand gepflegt.

### Push

Empfehlung **ntfy**: ein HTTPS-POST, eine App auf dem Handy, kein Broker. Da
bereits ein MQTT-Broker für `omarchy-strom` läuft, ist MQTT die naheliegende
Alternative — dann läuft der Alarm über Home Assistant. Beides hängt hinter
`net::notify(titel, text)` und ist später austauschbar; die Entscheidung kostet
nichts, wenn sie erst beim Umsetzen fällt.

## Umsetzung

Jeder Schritt ist für sich auf dem Gerät prüfbar — bei Embedded-Arbeit ist
das wichtiger als anderswo, weil Fehler sonst erst nach Stunden auffallen.

1. **SD-Karte lesen** — Karte mounten, Dateien auflisten, auf die serielle
   Konsole ausgeben. Prüfen: steckt eine Karte, wird sie erkannt.
2. **Rezept-Parser** — JSON einlesen, Zutaten und Schrittliste im Log ausgeben,
   Endzeit ausrechnen. Noch ohne UI.
3. **Sitzungslogik + NVS** — Fortschalten, Persistenz. Prüfen: mitten im Ablauf
   `pio run -t upload` auslösen; der Teig muss den Reset überleben.
4. **WLAN + NTP** — Zugang aus `/config.json`, Uhrzeit holen, Status anzeigen.
   Verhalten ohne Netz mitprüfen.
5. **Screens 1 bis 3** — Auswahl, Zutaten, laufender Timer, mit echten Daten.
6. **Alarm + Push** — Vollbild-Screen samt Schritt-Zutaten, `net::notify()`,
   Helligkeitssteuerung.
7. **Schrittliste als Overlay**, Status-Screen, Feinschliff.
8. **`CLAUDE.md` nachziehen** — Architektur, JSON-Format, die Einbrenn-Korrektur.

### Testmodus

Ein Zeitraffer ist keine Spielerei, sondern die Voraussetzung dafür, das Ding
überhaupt testen zu können: eine zwölfstündige Stückgare ist sonst nicht prüfbar.
Ein Faktor in `/config.json` (`"zeitraffer": 60`) lässt einen 12-h-Schritt in
12 Minuten ablaufen. Im laufenden Timer sichtbar markieren, damit niemand den
Modus versehentlich mit echtem Teig benutzt.

## Verifikation

- **Rezept-Parser**: Beispielrezept einlesen, Zutaten, Schrittliste und
  berechnete Endzeit gegen Handrechnung prüfen. Fehlerhaftes JSON, fehlende
  SD-Karte und ein Rezept ganz ohne Zutatenblock müssen eine lesbare Meldung
  ergeben, keinen Absturz.
- **Persistenz**: Ablauf starten, mitten in einem langen Schritt neu flashen.
  Restzeit muss stimmen — inklusive der Zeit, die das Gerät aus war.
- **Ohne Netz**: WLAN-Zugang absichtlich falsch setzen. Die App muss benutzbar
  bleiben und den fehlenden Zeitbezug ehrlich anzeigen.
- **Vollständiger Durchlauf** im Zeitraffer, mit allen drei Schrittarten, bis zum
  Ende. Push auf dem Handy gegenprüfen.
- **Dauerlauf** über Nacht mit dem Heartbeat auf der seriellen Konsole: Heap darf
  nicht wandern. Serielle Ausgabe mit `pio device monitor` lesen — `cat` liefert
  auf diesem Board nichts (siehe `CLAUDE.md`).
