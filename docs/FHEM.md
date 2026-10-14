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
