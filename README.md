# Chili Moisture Sensor

Batteriebetriebener kapazitiver Bodenfeuchtesensor (ESP32-C3 SuperMini) mit Gießempfehlung.
Deep Sleep → messen → nur bei Bedarf an FHEM senden → schlafen. Konfiguration über Weboberfläche.

Anforderungen: [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) · Schwesterprojekt:
[ESP32-temperature-sensor](https://github.com/tobi01001/ESP32-temperature-sensor)

## Verdrahtung (Pins änderbar in `platformio.ini`)
Sensor: Capacitive Soil Moisture Sensor v2.0 (HW-390). Das SuperMini führt GPIO0–10, 20, 21 heraus.

| Bauteil | SuperMini |
|---|---|
| Sensor VCC | GPIO3 (schaltbar) |
| Sensor GND | GND |
| Sensor AOUT | GPIO2 |
| Taster nach GND (weckt aus Deep Sleep) | GPIO0 |
| Versorgungs-Teiler (siehe unten) | GPIO1 |

```
 5V-Pin ──[100k]──┬──[100k]── GND
                  ├── GPIO1
                  └──[100nF]── GND
```
Der Teiler erkennt USB (> 4,5 V am 5V-Pin) und misst im Akkubetrieb die Akkuspannung.

**Versorgung:** Das Board hat keinen Lader. LiPo (3,0–4,2 V) an den **5V-Pin** (Onboard-Regler),
nicht an 3.3 (zu viel Spannung). Laden extern (z. B. TP4056). Bei gleichzeitigem Akku am 5V-Pin
und USB-Anschluss ggf. eine Schottky-Diode in die Akkuleitung setzen.

## Betriebsmodi
| Modus | Verhalten |
|---|---|
| Akku | Aufwachen per Timer → messen → senden **nur** bei Änderung ≥ Schwelle, Gießstatus-Wechsel, „gegossen“ oder Heartbeat → Deep Sleep. Kein Webserver. |
| Taster (Akku oder USB) | Taster drücken weckt das Gerät: WLAN `ChiliSensor-Setup` (192.168.4.1) plus Zugriff über die WLAN-IP. Nach 5 min oder „Neustart“ zurück in den Messmodus. |
| USB | Kein Schlaf. Webserver dauerhaft aktiv (WLAN-IP in der Seriell-Konsole, sonst AP). Messung weiter im Intervall. |

Ohne gespeicherte Konfiguration startet automatisch das Portal.

## Einstellungen (Weboberfläche)
WLAN · FHEM-Host/Port/Device · Messintervall (60 min) · Wiederholung bei Fehler (5 min) ·
Heartbeat (360 min) · Senden ab Änderung (5 %) · Gießen unter (35 %) · Hysterese (10 %, `water`
fällt erst ab 45 % zurück) · Gegossen-Erkennung bei Anstieg um (15 %) · Kalibrierung trocken/nass.

Kalibrieren: Sensor trocken in die Luft halten → „TROCKEN speichern“; bis zur Markierung in Wasser →
„NASS speichern“.

## Bauen & Flashen
```
pio run -t upload
pio device monitor
```

## FHEM
```
define chili1 dummy
attr WEB csrfToken none
```
Der Device-Name ist in der Weboberfläche einstellbar, das Device muss in FHEM existieren.
Readings: `moisture` (%), `raw_mv`, `supply_mv`, `usb`, `water` (1 = gießen), `watered_ago_h`.
```
define n_chili1 notify chili1:water:.1 { Log 1, "Chili 1 gießen!" }
```

## Status
Kompiliert, noch nicht auf Hardware getestet (Kalibrier-Defaults: trocken 3000 mV, nass 1300 mV).

## Lizenz
MIT – siehe [LICENSE](LICENSE).
