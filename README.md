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
| Sensor AOUT | GPIO4 |
| Taster nach GND (weckt aus Deep Sleep) | GPIO0 |
| Versorgungs-Teiler (siehe unten) | GPIO1 |

```
                         ┌─────────────────────────── 5V-Pin ──┐
 Akku + ──[Schalter]─────┤                                     │ SuperMini
 (oder USB, nie beides)  │                                     │
                       [100k]                                  │
                         ├────────────────────────── GPIO1 ────┤ (ADC: Versorgung)
                       [100k]        [100nF]                   │
                         │             │                       │
 Akku − ─────────────────┴─────────────┴────────────── GND ────┤
                                                               │
 Sensor VCC ◄──────────────────────────────────────── GPIO3 ───┤ (schaltbar)
 Sensor AOUT ─────────────────────────────────────────► GPIO4 ─┤ (ADC: Feuchte)
 Sensor GND ───────────────────────────────────────────  GND ──┤
 Taster ── GND ──────────────────────────────────────── GPIO0 ─┘ (weckt, Portal)
```
Zusammengefasst:

| Verbindung | von | nach |
|---|---|---|
| Sensor-Versorgung | Sensor VCC | GPIO3 |
| Sensor-Signal | Sensor AOUT | GPIO4 |
| Masse | Sensor GND, Taster, Akku − | GND |
| Taster | GPIO0 | GND (Pullup intern) |
| Versorgungs-Teiler | 5V-Pin → 100k → **GPIO1** → 100k → GND, 100 nF von GPIO1 nach GND | |

Teiler im Detail:
```
 5V-Pin ──[100k]──┬──[100k]── GND
                  ├── GPIO1
                  └──[100nF]── GND
```
Der Teiler besteht aus **zwei gleichen Widerständen** (Faktor 2, Wert frei: 100 kΩ bis 1 MΩ; größer = weniger
Dauerstrom, 2×1 MΩ ≈ 2 µA). Der 100-nF-Kondensator puffert den ADC bei hohen Widerstandswerten. Er erkennt USB
(> 4,5 V am 5V-Pin) und misst im Akkubetrieb die Akkuspannung.

**Erlaubte / verbotene GPIOs (ESP32-C3 SuperMini):**

| Funktion | Erlaubt | Verboten |
|---|---|---|
| Versorgungsspannung messen (`PIN_SUPPLY_ADC`) | GPIO0, 1, 3, 4 (ADC1); GPIO2 nur mit hochohmigem Teiler ohne Pull-down | GPIO5 (ADC2, mit WiFi/BLE nicht nutzbar), GPIO6–10, 20, 21 (kein ADC) |
| Sensor-VCC treiben (`PIN_SENSOR_POWER1/2`, zwei GPIOs parallel) | GPIO3, 4, 5, 6, 7, 10 (sowie 0/1, falls frei) | GPIO2, 8, 9 (Strapping-Pins: Pegel beim Reset bestimmt den Boot-Modus; GPIO8 = LED, GPIO9 = BOOT-Taster), GPIO18/19 (USB), GPIO20/21 (UART0) |
| Sensor AOUT messen (`PIN_SENSOR_ADC`) | GPIO0, 1, 3, 4 (ADC1); GPIO2 nur mit Vorsicht | GPIO5 (ADC2), GPIO6–10, 20, 21 (kein ADC) |

Jeder Pin darf nur für eine Funktion verwendet werden. Die Standardbelegung steht in `platformio.ini`.
**Hinweis:** Die aktuelle Standardbelegung nutzt GPIO9 für `PIN_SENSOR_POWER2`; wegen des Strapping-Risikos sollte stattdessen ein erlaubter Pin (z. B. GPIO5) gewählt werden.

**ADC-Spannungsbereich:** ESP32-C3 ADC1 ist auf GPIO0–4 verfügbar; der maximale Messbereich liegt bei der höchsten
Abschwächung bei ungefähr 2,5 V. Prüfe vor dem Betrieb die maximale Spannung an Sensor-AOUT und am ADC-Abgriff des
Versorgungsteilers. Mit dem beschriebenen 1:1-Teiler liegen bei 5 V am 5V-Pin bereits 2,5 V am ADC an; höhere
USB-Spannung kann den Messbereich überschreiten. Falls einer der ADC-Eingänge mehr als etwa 2,5 V erreicht, muss
die betreffende Signalspannung hardwareseitig reduziert werden. Der Arduino-Core initialisiert den ADC beim Aufruf
von `analogReadMilliVolts()`; eine separate ADC-Initialisierung ist nicht erforderlich.

**Kalibrierung der Versorgungsspannung** (gleicht Widerstandstoleranz und ADC-Offset aus):
1. Multimeter auf Gleichspannung (V=) stellen. Rote Messspitze an den **5V-Pin** des SuperMini, schwarze an **GND**
   (Akku oder USB angeschlossen, Gerät läuft).
2. Portal öffnen (Taster drücken oder USB; `http://192.168.4.1/` bzw. WLAN-IP).
3. Im Abschnitt „Sensor calibration“ den Wert **in Millivolt** in das Feld *Supply voltage measured with a
   multimeter (mV)* eintragen (z. B. 4,012 V → `4012`) und *Calibrate supply voltage* drücken.
4. Der Korrekturfaktor (0,5–1,5) wird gespeichert und unter „Live“ angezeigt.

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

## Einstellungen (Weboberfläche, englisch)
Übertragung (WLAN, Bluetooth oder beides) · WLAN · FHEM-Host/Telnet-Port · Messintervall (60 min) · Wiederholung bei Fehler (5 min) ·
Heartbeat (360 min) · Senden ab Änderung (5 %) · Gießen unter (35 %) · Hysterese (10 %, `water`
fällt erst ab 45 % zurück) · Gegossen-Erkennung bei Anstieg um (15 %) · Kalibrierung trocken/nass ·
Einschwingzeit des Sensors (1000 ms, 100–5000; nach einer Änderung neu kalibrieren).

Kalibrieren: Sensor trocken in die Luft halten → „TROCKEN speichern“; bis zur Markierung in Wasser →
„NASS speichern“.

## Bauen & Flashen
```
pio run -t upload          # Entwicklung: Env supermini-debug (Serial-Log, Portal bei jedem Start)
pio device monitor
pio run -e supermini -t upload   # Produktiv: kein Log, Portal nur per Taster
```

## FHEM
Push per Telnet wie beim Temperatursensor. Der Gerätename `Chili_<crc16 der MAC>` wird automatisch gebildet,
FHEM legt das Device beim ersten Kontakt selbst an (Funktion `handleChiliSensor` in
[fhem/99_myUtils_chili.pm](fhem/99_myUtils_chili.pm)).
```
define telnetSensors telnet 7072
```
Einrichtung, Readings, Beispiele und Sicherheitshinweise: **[docs/FHEM.md](docs/FHEM.md)**.

Alternativ ohne WLAN im Sensor: Bluetooth (BTHome) über ein Shelly-Gateway-Skript ([fhem/shelly_chili_gateway.js](fhem/shelly_chili_gateway.js)), siehe FHEM.md.

## Status
Kompiliert und läuft (WLAN, BLE, Portal); Messung mit Sensor noch nicht getestet (Kalibrier-Defaults: trocken 2350 mV, nass 970 mV).

**Umbenennung (v0.4):** Der Sensor hieß zuvor „Chili“. Neue Namen: FHEM-Device `Moisture_<crc>`, AP `MoistureSensor-Setup`,
Funktion `handleMoistureSensor`. Die Einstellungen (NVS-Namespace) wurden umbenannt und müssen einmalig neu eingegeben
werden. Alte `Chili_*`-Devices in FHEM können gelöscht werden.

## Lizenz
MIT – siehe [LICENSE](LICENSE).
