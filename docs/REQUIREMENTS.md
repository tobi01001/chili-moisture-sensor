# Anforderungsspezifikation – Bodenfeuchtesensor

Version 0.4 · Status: Entwurf

## 1. Zweck
Batteriebetriebener Bodenfeuchtesensor für Zimmer- und Nutzpflanzen (z. B. Chili). Er misst periodisch die Bodenfeuchte,
meldet sie bei Bedarf an FHEM und liefert eine Gießempfehlung („Gießen: ja/nein“).

## 2. Hardware
| ID | Anforderung |
|----|-------------|
| H1 | ESP32-C3 SuperMini (GPIO0–10, 20, 21 herausgeführt, kein Lader). |
| H2 | Sensor: kapazitiver Bodenfeuchtesensor v2.0 / HW-390 (TLC555, Regler), analog an ADC1-Pin. |
| H3 | Sensor-Versorgung über GPIO schaltbar, nur während der Messung aktiv; im Deep Sleep bleibt der Power-GPIO per Hold auf LOW. |
| H4 | Versorgung 1S-LiPo (3,0–4,2 V) am 5V-Pin; Laden extern. |
| H5 | Zwei gleiche Teilerwiderstände (frei wählbar, 100 kΩ–1 MΩ) vom 5V-Pin auf einen ADC-Pin: erkennt USB und misst die Akkuspannung. Kalibrierfaktor über die Weboberfläche (Eingabe der real gemessenen Spannung). |
| H7 | USB und Akku nie gleichzeitig am 5V-Pin (VBUS und 5V-Pin sind verbunden); Trennschalter/Jumper in der Akkuleitung. Power-LED entfernen. |
| H6 | Taster nach GND an einem RTC-fähigen GPIO (0–5); weckt aus dem Deep Sleep. |

## 3. Funktionale Anforderungen
| ID | Anforderung |
|----|-------------|
| F1 | Akkubetrieb: Timer-Wakeup, messen, ggf. senden, Deep Sleep. Kein Webserver. |
| F2 | Messung mittelt mehrere ADC-Samples. |
| F3 | Umrechnung in 0–100 % über zwei Kalibrierpunkte (trocken/nass). |
| F4 | Gießempfehlung `water` mit Schwelle (Standard 35 %) und Hysterese (Standard 10 %): Setzen unter Schwelle, Zurücksetzen ab Schwelle + Hysterese. |
| F5 | Gegossen-Erkennung: Feuchteanstieg ≥ einstellbarer Wert (Standard 15 %) zwischen zwei Messungen; Reading `watered_ago_h`. |
| F6 | Senden nur bei: erstem Start, Feuchteänderung ≥ Schwelle (Standard 5 %) seit letztem Senden, Wechsel von `water`, erkanntem Gießen oder Heartbeat. Ohne Sendegrund bleibt das WLAN aus. |
| F7 | Heartbeat einstellbar (Standard 360 min). |
| F8 | Push per Telnet an FHEM (Perl-Aufruf `handleMoistureSensor`); Gerätename `Moisture_<crc16 der MAC>`, Kurze 3-Buchstaben-Schlüssel (gleich für WLAN und BLE), von der FHEM-Funktion zu Readings expandiert. Auto-Anlage in FHEM: `moisture`, `raw_mv`, `supply_mv`, `usb`, `water`, `watered_ago_h` (+ einmalig Geräteinfo). |
| F9 | Bei Übertragungsfehler Wiederholung nach einstellbarem, kürzerem Intervall. |
| F10 | Konfigurations-Webserver: Aktivierung durch Taster-Wakeup (Akku/USB) oder dauerhaft bei USB. Erreichbar über WLAN-IP und/oder AP `MoistureSensor-Setup` (192.168.4.1). |
| F11 | Portal im Akkubetrieb endet nach „Speichern“, „Neustart“ oder 5 min; dann Messmodus. |
| F12 | Einstellbar: WLAN, FHEM-Host/Telnet-Port, Messintervall, Wiederholintervall, Heartbeat, Änderungsschwelle, Gießschwelle, Hysterese, Gegossen-Sprung, Kalibrierung (inkl. Klick-Kalibrierung mit Live-Anzeige). |
| F13 | Einstellungen im NVS; Zustand (letzter Wert, Zähler) im RTC-Speicher. |
| F15 | Kalibrierung der Versorgungsspannung: Eingabe der gemessenen Spannung ergibt einen Korrekturfaktor (0,5–1,5). |
| F16 | Übertragungsweg wählbar (Portal): WLAN→FHEM (Standard), Bluetooth oder beides (nur zum Testen). Batteriebetrieb-/USB-Erkennung bleibt unabhängig davon. Bei „nur Bluetooth“ bleibt das WLAN im Messmodus aus. |
| F17 | Bluetooth: nicht verbindbares BTHome-v2-Advertising (Feuchte, Akku %, Spannung, `water`, USB) ohne Gerätenamen-Prefix: lokaler Name nur `<crc>` (31-Byte-Limit; das Gateway-Skript bildet `Moisture_<crc>`), zusätzlich Rohwert, ca. 1,5 s je Meldung. Empfang über Shelly-Gateway-Skript (`fhem/shelly_moisture_gateway.js`, getestet vorgesehen für Shelly Plus Plug S), das per HTTP an FHEM weiterleitet. Kein Rückkanal. |
| F14 | USB-Betrieb: kein Deep Sleep, Messung im Intervall in der Hauptschleife. |

## 4. Nichtfunktionale Anforderungen
| ID | Anforderung |
|----|-------------|
| N1 | Aktive Zeit pro Zyklus ohne Senden < 1 s; mit Senden < 5 s. Ziel: Monate Laufzeit mit 1000–2000 mAh. |
| N2 | Schlanker, gut lesbarer Code; nur Arduino-ESP32-Core. |
| N3 | PlatformIO; Pins per Build-Flag änderbar. |
| N4 | Lizenz: MIT. |
| N5 | Im Dauerbetrieb (USB/Portal) blockiert `loop()` nicht: Messung, Senden und Live-Messwert laufen in einem eigenen Task, die Web-Handler lesen nur den letzten Messwert. Vor dem Deep Sleep darf blockierend gewartet werden. |

## 5. Schnittstelle FHEM
- TCP auf FHEM-Telnet-Port (Standard 7072): `{handleMoistureSensor("Moisture_<crc>", "moi 42: raw 2100: …")}` + `exit`.
- Voraussetzung: `define telnetSensors telnet 7072`, `fhem/99_myUtils_moisture.pm` geladen. Details: docs/FHEM.md.

## 5a. Schnittstelle Bluetooth
BTHome v2, siehe docs/FHEM.md.

## 6. Offen / spätere Erweiterungen
- OTA-Update, MQTT-Alternative.
- Akku-Warnschwelle, Trendberechnung („trocken in ~2 Tagen“).
- Kompensation des Diodenabfalls bei Schottky in der Akkuleitung.
- Mehrere Sensoren.
