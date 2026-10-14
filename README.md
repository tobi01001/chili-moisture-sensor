# Chili Moisture Sensor

Batteriebetriebener kapazitiver Bodenfeuchtesensor (ESP32-C3 SuperMini) mit Gießempfehlung.
Aufwachen per Deep Sleep → messen → Push an FHEM → schlafen. Konfiguration über Web-Oberfläche.

Anforderungen: [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) · Schwesterprojekt:
[ESP32-temperature-sensor](https://github.com/tobi01001/ESP32-temperature-sensor)

## Verdrahtung (Standard-Pins, änderbar in `platformio.ini`)
| Sensor / Bauteil | ESP32-C3 SuperMini |
|---|---|
| Sensor VCC | GPIO3 (schaltbar) |
| Sensor GND | GND |
| Sensor AOUT | GPIO2 |
| Akku + → 100 kΩ → GPIO4 → 100 kΩ → GND (optional) | GPIO4 |
| Config-Taster nach GND | GPIO10 |
| Versorgung | Das Board hat **keinen Lader**: LiPo (3,0–4,2 V) an den 3.3-Pin *oder* geregelte 5 V an 5V; Laden extern (z. B. TP4056) |

Sensor: Capacitive Soil Moisture Sensor v2.0 (HW-390, TLC555 + Spannungsregler). Pins 0–4 und 10 sind auf dem SuperMini herausgeführt.

> Kein GPIO9 für den Taster verwenden (Boot-Strapping-Pin).

## Bauen & Flashen
```
pio run -t upload
pio device monitor
```

## Erste Inbetriebnahme / Konfiguration
1. Ohne Konfiguration (oder mit Config-Pin auf GND beim Reset) startet das WLAN `ChiliSensor-Setup`.
2. Mit `http://192.168.4.1` verbinden, WLAN, FHEM und Intervall eintragen.
3. Kalibrieren: Sensor trocken in die Luft halten → „TROCKEN speichern“; in ein Glas Wasser
   (bis zur Markierung) → „NASS speichern“.
4. Speichern – das Gerät startet neu und arbeitet im Zyklus. Nach 5 min ohne Eingabe läuft es normal weiter.

## FHEM
```
define chili1 dummy
attr chili1 readingList moisture raw_mv battery_mv water
attr WEB csrfToken none
```
Das Gerät setzt `moisture` (%), `raw_mv`, `battery_mv`, `water` (1 = gießen).
Beispiel-Benachrichtigung:
```
define n_chili1 notify chili1:water:.1 { Log 1, "Chili 1 gießen!" }
```
(Statt `Log` z. B. `set Telegram message …`.)

## Status
Erster schlanker Entwurf – noch nicht auf Hardware getestet (Kalibrier-Defaults: trocken 3000 mV, nass 1300 mV).

## Lizenz
MIT – siehe [LICENSE](LICENSE).
