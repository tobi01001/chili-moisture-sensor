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
Der Teiler besteht aus **zwei gleichen Widerständen** (Faktor 2, Wert frei: 100 kΩ bis 1 MΩ; größer = weniger
Dauerstrom, 2×1 MΩ ≈ 2 µA). Der 100-nF-Kondensator puffert den ADC bei hohen Widerstandswerten. Er erkennt USB
(> 4,5 V am 5V-Pin) und misst im Akkubetrieb die Akkuspannung. **Kalibrierung:** In der Weboberfläche die mit
dem Multimeter gemessene Spannung eintragen, daraus wird ein Korrekturfaktor berechnet (Toleranz, ADC-Offset).

**Versorgung (laut [SuperMini-Schaltplan](https://make.net.za/wp-content/datasheets/ESP32-C3%20SuperMini%20Schematic.pdf)):**
Der 5V-Pin liegt direkt am USB-VBUS und über eine Schottky-Diode (BAT60J) am 3,3-V-Regler (ME6211). Ein LiPo
(3,0–4,2 V) am 5V-Pin versorgt das Board also über Diode und Regler; der Pin zeigt dann die Akkuspannung.
Das Board hat **keinen Lader**, Laden erfolgt extern. **USB und Akku niemals gleichzeitig anschließen**
(VBUS und Akku sind am Pin verbunden). Dafür einen Schalter/Jumper in die Akkuleitung setzen.
Die **rote Power-LED** (an VSYS) zieht dauerhaft Strom und sollte entfernt werden. Der ME6211 braucht selbst
rund 50 µA.

## Betriebsmodi
| Modus | Verhalten |
|---|---|
| Akku | Aufwachen per Timer → messen → senden **nur** bei Änderung ≥ Schwelle, Gießstatus-Wechsel, „gegossen“ oder Heartbeat → Deep Sleep. Kein Webserver. |
| Taster (Akku oder USB) | Taster drücken weckt das Gerät: WLAN `ChiliSensor-Setup` (192.168.4.1) plus Zugriff über die WLAN-IP. Nach 5 min oder „Neustart“ zurück in den Messmodus. |
| USB | Kein Schlaf. Webserver dauerhaft aktiv (WLAN-IP in der Seriell-Konsole, sonst AP). Messung weiter im Intervall. |

Ohne gespeicherte Konfiguration startet automatisch das Portal.

## Einstellungen (Weboberfläche)
WLAN · FHEM-Host/Telnet-Port · Messintervall (60 min) · Wiederholung bei Fehler (5 min) ·
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
Push per Telnet wie beim Temperatursensor. Der Gerätename `Chili_<crc16 der MAC>` wird automatisch gebildet,
FHEM legt das Device beim ersten Kontakt selbst an (Funktion `handleChiliSensor` in
[fhem/99_myUtils_chili.pm](fhem/99_myUtils_chili.pm)).
```
define telnetSensors telnet 7072
```
Einrichtung, Readings, Beispiele und Sicherheitshinweise: **[docs/FHEM.md](docs/FHEM.md)**.

## Status
Kompiliert, noch nicht auf Hardware getestet (Kalibrier-Defaults: trocken 3000 mV, nass 1300 mV).

## Lizenz
MIT – siehe [LICENSE](LICENSE).
