# ESP32-Wetterstation

ESP32-Wetterstation mit ILI9341-Display, DWD-Daten (Bright Sky API) und blinkendem "Wetter Alarm"-Pop-up für Unwetterwarnungen.

## Voraussetzungen

### Hardware
- ESP32-Board (z. B. AZ-Touch ESP32)
- 2,4" ILI9341-TFT mit Touch (XPT2046)
- Optional: Gehäuse und Sensorik entsprechend deiner Hardware

### Software & Bibliotheken
1. **Arduino IDE** (Version 2.x empfohlen) oder eine kompatible Toolchain
2. **ESP32 Board Package** von Espressif Systems über den Boardverwalter installieren
3. Folgende Bibliotheken über den Arduino Library Manager installieren:
   - Mini Grafx (Daniel Eichhorn)
   - ESP32 WeatherStation (Zihatec)
   - Json Streaming Parser (Daniel Eichhorn)
   - simpleDSTadjust (neptune2)
   - ArduinoJson (Benoît Blanchon)
   - XPT2046_Touchscreen (Paul Stoffregen)

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

> Tipp: Wenn du einen anderen Touch-Controller-Pin oder eine alternative Hardware-Version nutzt, passe die Pin-Definitionen am Ende der `settings.h` an.

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
