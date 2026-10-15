# FHEM-Anbindung

Der Sensor sendet per **Telnet-Push** (wie [ESP32-temperature-sensor](https://github.com/tobi01001/ESP32-temperature-sensor)):
eine Perl-Zeile, die in FHEM die Funktion `handleChiliSensor()` aufruft. Der Gerätename wird aus einer
CRC-16-Prüfsumme der MAC-Adresse gebildet (`Chili_<crc>`). FHEM legt unbekannte Geräte automatisch als
`dummy` an und füllt die Readings. Es ist nichts pro Sensor zu konfigurieren.

## Einrichtung (einmalig)

1. **Telnet-Port** in FHEM anlegen (Port 7072, wie beim Temperatursensor – dort ggf. schon vorhanden):
   ```
   define telnetSensors telnet 7072
   ```
2. **Funktion installieren:** `fhem/99_myUtils_chili.pm` in das FHEM-Verzeichnis `FHEM/` kopieren, dann
   `reload 99_myUtils_chili.pm` (oder `rereadcfg`).
3. In der Weboberfläche des Sensors FHEM-Host und Telnet-Port eintragen.
4. Nach dem ersten Senden erscheint das Device `Chili_<crc>` (Raum `Chili`). Der Name steht auch oben auf der
   Konfigurationsseite des Sensors.

## Protokoll
```
{handleChiliSensor("Chili_12345", "moisture 42: raw_mv 2100: supply_mv 3850: usb 0: water 0")}
exit
```
Paare `reading wert`, getrennt durch `:`.

## Readings
| Reading | Bedeutung | Gesendet |
|---|---|---|
| `moisture` | Bodenfeuchte in % | bei jeder Meldung |
| `raw_mv` | Rohwert des Sensors in mV | bei jeder Meldung |
| `supply_mv` | Versorgungs-/Akkuspannung in mV | bei jeder Meldung |
| `usb` | 1 = USB-Versorgung erkannt | bei jeder Meldung |
| `water` | 1 = Gießen empfohlen (mit Hysterese) | bei jeder Meldung |
| `watered_ago_h` | Stunden seit erkannter Bewässerung | nach erkanntem Gießen |
| `fw_version`, `mac_address`, `interval_min`, `heartbeat_min` | Geräteinfo | einmal nach Power-On |
| `state` | `water` oder `ok` | bei jeder Meldung |

Der Sensor sendet nur bei Änderung, Gießstatus-Wechsel oder als Heartbeat. Jede Meldung aktualisiert den
Zeitstempel der Readings. Damit lässt sich ein ausgefallener Sensor erkennen, z. B. mit `watchdog` (Zeit =
Heartbeat + Reserve).

## Beispiele
```
# Gießhinweis (Telegram o. ä. statt Log)
define n_chili notify Chili_.*:water:.1 { Log 1, "$NAME gießen!" }

# Sensor tot, wenn 8 h nichts kam (Heartbeat 6 h)
define wd_chili watchdog Chili_12345 08:00:00 SAME { Log 1, "Chili_12345 meldet sich nicht" }
```

## Hinweise zur Sicherheit
- Der Telnet-Port führt Perl-Code aus. **Nur im vertrauenswürdigen LAN** erreichbar machen, nicht ins Internet
  weiterleiten. Wenn du ein Passwort setzt, kann der Sensor sich aktuell nicht anmelden.
- `handleChiliSensor` akzeptiert nur Gerätenamen `Chili_<Ziffern>` und nur die oben genannten Readings mit
  plausiblen Werten; alles andere wird ignoriert (Log 3).
- Beim automatischen Anlegen wird `save` ausgeführt.

## Bluetooth über Shelly-Gateway (Alternative zu WLAN)

Im Portal „Übertragung“ auf *Bluetooth* stellen. Der Sensor sendet dann ohne WLAN ein BTHome-v2-Advertising
(Name `Chili_<crc>`). Ein Shelly (z. B. Plus Plug S, Bluetooth aktiv) empfängt es per Skript und leitet es
an FHEM weiter. Die FHEM-Seite (`handleChiliSensor`) ist dieselbe wie bei WLAN, das Device wird automatisch angelegt.

Einrichtung:
1. FHEM: eigenen HTTP-Eingang anlegen (nur LAN!):
   ```
   define WEBchili FHEMWEB 8084 global
   attr WEBchili csrfToken none
   ```
2. Shelly: *Settings → Bluetooth* aktivieren. *Scripts → Create script*, Inhalt aus
   `fhem/shelly_chili_gateway.js` einfügen, `FHEM`-URL anpassen, speichern, Start und „Run on startup“ aktivieren.
3. Reichweite: Shelly in Funkreichweite (ca. 10 m) des Sensors. Der Sensor sendet jede Meldung ca. 1,5 s lang
   wiederholt (kein Rückkanal); das Skript filtert Duplikate.

Über BLE kommen `moisture`, `supply_mv`, `usb`, `water`. `raw_mv`, `watered_ago_h` und die Geräteinfo
werden nur über WLAN übertragen. Der Sensor hat kein Wake-up-Konfigurationsportal über BLE: Taster drücken öffnet
weiter den AP `ChiliSensor-Setup`.
