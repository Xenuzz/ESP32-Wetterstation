# ESP32-Wetterstation

ESP32-Wetterstation mit ILI9341-Display, DWD-Daten (Bright Sky API) und blinkendem "Wetter Alarm"-Pop-up für Unwetterwarnungen.

## Voraussetzungen

### Hardware
- ESP32-Board (z. B. AZ-Touch ESP32)
- 2,4" ILI9341-TFT (optional mit XPT2046-Touchmodul)
- Optional: Gehäuse und Sensorik entsprechend deiner Hardware

> **Ohne Touch unterwegs?** Das Projekt funktioniert vollständig mit einem reinen ILI9341-Display. Die Touch-Leitungen können unverdrahtet bleiben, solange `TOUCH_ENABLED` in der Konfiguration ausgeschaltet ist (siehe Abschnitt [Konfiguration](#konfiguration)).

### Software & Bibliotheken
1. **Arduino IDE** (Version 2.x empfohlen) oder eine kompatible Toolchain
2. **ESP32 Board Package** von Espressif Systems über den Boardverwalter installieren
3. Folgende Bibliotheken über den Arduino Library Manager installieren:
   - Mini Grafx (Daniel Eichhorn)
   - ESP32 WeatherStation (Zihatec)
   - Json Streaming Parser (Daniel Eichhorn)
   - simpleDSTadjust (neptune2)
   - ArduinoJson (Benoît Blanchon)
   - Optional bei Nutzung des Touchpanels: **XPT2046_Touchscreen** (Paul Stoffregen). Diese Bibliothek wird nur benötigt, wenn du `TOUCH_ENABLED` aktivierst.

## Schaltplan
Der folgende Schaltplan zeigt die typischen Verbindungen des AZ-Touch-ESP32 (oder eines vergleichbaren ESP32 mit ILI9341-Display
und XPT2046-Touchcontroller).
Bei anderen Boards können die Pins variieren – passe sie in [`settings.h`](settings.h) entsprechend an.
Wenn dein Display **keinen** Touch besitzt, lasse die Touch-Anschlüsse (rechte Spalte) einfach unverdrahtet.
Für Display-only-Aufbauten reicht die Verdrahtung der linken beiden Spalten.

```mermaid
graph LR
    subgraph ESP32
        ESP32_5V[5V / VIN]
        ESP32_3V3[3V3]
        ESP32_GND[GND]
        GPIO23[GPIO23 (MOSI)]
        GPIO19[GPIO19 (MISO)]
        GPIO18[GPIO18 (SCK)]
        GPIO5[GPIO5 (TFT_CS)]
        GPIO4[GPIO4 (TFT_DC)]
        GPIO22[GPIO22 (TFT_RST)]
        GPIO15[GPIO15 (TFT_LED)]
        GPIO14[GPIO14 (TOUCH_CS)]
        GPIO2[GPIO2 (TOUCH_IRQ)]
    end

    subgraph Display
        TFT_VCC[VCC]
        TFT_LED[TFT LED]
        TFT_GND[GND]
        TFT_MOSI[MOSI]
        TFT_MISO[MISO]
        TFT_SCK[SCK]
        TFT_CS[CS]
        TFT_DC[DC]
        TFT_RST[RST]
    end

    subgraph Touchcontroller
        TOUCH_VCC[VCC]
        TOUCH_GND[GND]
        TOUCH_MOSI[T_DIN]
        TOUCH_MISO[T_DO]
        TOUCH_SCK[T_CLK]
        TOUCH_CS[T_CS]
        TOUCH_IRQ[T_IRQ]
    end

    ESP32_5V --> TFT_VCC
    ESP32_3V3 --> TOUCH_VCC
    GPIO23 --> TFT_MOSI
    GPIO23 --> TOUCH_MOSI
    GPIO19 --> TFT_MISO
    GPIO19 --> TOUCH_MISO
    GPIO18 --> TFT_SCK
    GPIO18 --> TOUCH_SCK
    GPIO5 --> TFT_CS
    GPIO4 --> TFT_DC
    GPIO22 --> TFT_RST
    GPIO15 --> TFT_LED
    GPIO14 --> TOUCH_CS
    GPIO2 --> TOUCH_IRQ
    ESP32_GND --- GND_SHARED((GND))
    TFT_GND --- GND_SHARED
    TOUCH_GND --- GND_SHARED
```

| ESP32 | Display (ILI9341) | Touch (XPT2046) | Hinweis |
|-------|-------------------|-----------------|---------|
| 5V / VIN | VCC | – | Display-Versorgung (abhängig vom Modul ggf. 5 V)
| 3V3 | – | VCC | Touchcontroller benötigt 3,3 V
| GND | GND | GND | Gemeinsame Masse für alle Komponenten
| GPIO23 | MOSI | T_DIN | SPI-Daten vom ESP32 zu Display & Touch
| GPIO19 | MISO | T_DO | SPI-Daten vom Touch zum ESP32
| GPIO18 | SCK | T_CLK | Gemeinsame SPI-Taktleitung
| GPIO5 | CS | – | Chip-Select des Displays (`TFT_CS`)
| GPIO4 | DC | – | Daten/Kommando (`TFT_DC`)
| GPIO22 | RST | – | Reset des Displays (`TFT_RST`)
| GPIO15 | LED | – | Hintergrundbeleuchtung steuerbar (Standard in `settings.h`)
| GPIO14 | – | T_CS | Chip-Select des Touchcontrollers (`TOUCH_CS`) – nur anschließen, wenn Touch genutzt wird
| GPIO2 / 27 | – | T_IRQ | Touch-Interrupt (abhängig von Platinenrevision) – nur anschließen, wenn Touch genutzt wird

> **Hinweis:** Auf manchen AZ-Touch-Revisionen liegt der Touch-Interrupt auf GPIO27 oder die Hintergrundbeleuchtung ist fest auf
> 3,3 V verdrahtet. Passe in diesem Fall die Definitionen `TOUCH_IRQ` bzw. `TFT_LED` in `settings.h` an. Ohne Touchmodul musst du
> nur die Display-Leitungen verbinden.

## Konfiguration
Alle anpassbaren Werte findest du in [`settings.h`](settings.h):

1. **WLAN-Zugangsdaten** setzen
   ```cpp
   #define WIFI_SSID "dein-wlan"
   #define WIFI_PASS "dein-passwort"
   #define WIFI_HOSTNAME "Wetterstation"
   ```
2. **Standort für Bright Sky** definieren – hiermit legst du fest, für welchen Ort der Deutsche Wetterdienst abgefragt wird. Für Bright Sky ist **kein API-Key nötig**.
   ```cpp
   const double BRIGHT_SKY_LATITUDE = 52.2620;
   const double BRIGHT_SKY_LONGITUDE = 12.2922;
   const char* BRIGHT_SKY_TIMEZONE = "Europe/Berlin";
   String DISPLAYED_CITY_NAME = "Ziesar";
   ```
3. **Anzeigeoptionen** wie Einheiten (`IS_METRIC`), 12/24-Stunden-Uhr (`IS_STYLE_12HR`) oder Wochentags-/Monatsnamen bei Bedarf anpassen.
4. **Update-Intervalle** (`UPDATE_INTERVAL_SECS`) und Energiesparmodus (`SLEEP_INTERVAL_SECS`) nach Wunsch einstellen.
5. **Touch-Unterstützung steuern:**
   - **Touch aktivieren:** Setze `#define TOUCH_ENABLED 1`, verbinde die Touch-Pins laut Tabelle und installiere die Bibliothek *XPT2046_Touchscreen*.
   - **Touch deaktivieren:** Lasse die Touch-Pins offen, setze `#define TOUCH_ENABLED 0` (Standard) – damit entfällt der Bibliotheksbedarf und der Sketch überspringt jegliche Touch-Abfragen.

> Tipp: Wenn du einen anderen Touch-Controller-Pin oder eine alternative Hardware-Version nutzt, passe die Pin-Definitionen am Ende der `settings.h` an und aktiviere den Touch nur bei Bedarf über `TOUCH_ENABLED`.

## Kompilieren & Hochladen auf den ESP32
1. Projektverzeichnis in der Arduino IDE öffnen (`esp32-weather-station-color.ino`).
2. Unter **Werkzeuge → Board** das passende ESP32-Board auswählen (z. B. *ESP32 Dev Module* oder das zu deinem Kit passende Profil).
3. Den richtigen **COM-/TTY-Port** wählen.
4. Mit dem Haken-Symbol die Kompilierung testen; fehlende Bibliotheken installiert die IDE bei Bedarf nach.
5. Mit dem Pfeil-Symbol hochladen. Während des Uploads ggf. den **Boot-Taster** des ESP32 drücken, falls dein Board dies erfordert.

Nach dem ersten Start verbindet sich das Gerät mit deinem WLAN, ruft die aktuellen Wetterdaten über die Bright Sky API ab und zeigt laufend Wetterinfos sowie aktive DWD-Warnungen im Popup "Wetter Alarm" an.

## Aktualisierung & Versionsverwaltung
Falls du das Projekt via Git verwaltest, kannst du nach erfolgtem Upload die Änderungen wie gewohnt committen und auf dein Remote-Repository pushen:
```bash
git add .
git commit -m "Aktualisiere Konfiguration"
git push origin <branch>
```
Ersetze `<branch>` durch den gewünschten Branch-Namen (z. B. `main` oder `work`).

## Release Notes
Nutze den folgenden Text als Grundlage für deine Release-Beschreibung, um die wichtigsten Anpassungen der aktuellen Version hervorzuheben:

> **ESP32-Wetterstation – Aktualisierung**
>
> * Umstellung der Wetterdatenbeschaffung von OpenWeatherMap auf die Bright-Sky-API des Deutschen Wetterdienstes inklusive neuer Parser, Datenmodelle sowie Mapping auf die bestehenden Wetter-Icons.
> * Integration eines blinkenden Pop-ups **„Wetter Alarm“**, das bei aktiven DWD-Warnungen automatisch erscheint und alle relevanten Details (Ereignis, Zeitraum, Schweregrad und Handlungsempfehlungen) anzeigt.
> * Erweiterte Konfigurierbarkeit in `settings.h`: WLAN-Setup, Standortdaten, Anzeigeoptionen sowie ein neuer Schalter `TOUCH_ENABLED`, mit dem sich das XPT2046-Touchpanel komfortabel aktivieren oder deaktivieren lässt.
> * Aktualisierte Dokumentation mit detailliertem Schaltplan, optionaler Touch-Verdrahtung, Installationsanleitung der benötigten Bibliotheken und Hinweisen zum Kompilieren, Hochladen und zum Git-Workflow.

Passe Formulierungen oder Umfang nach Bedarf an (z. B. um Versionsnummern oder eigene Erweiterungen zu ergänzen).
