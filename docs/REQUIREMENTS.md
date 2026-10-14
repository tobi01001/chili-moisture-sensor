# Anforderungsspezifikation – Chili-Feuchtesensor

Version 0.1 · Status: Entwurf

## 1. Zweck
Batteriebetriebener Bodenfeuchtesensor für Chilipflanzen. Er misst periodisch die Bodenfeuchte,
meldet sie an FHEM und liefert eine Gießempfehlung („Gießen: ja/nein“).

## 2. Hardware (Randbedingungen)
| ID | Anforderung |
|----|-------------|
| H1 | Mikrocontroller: ESP32-C3 SuperMini (Deep-Sleep-fähig). |
| H2 | Sensor: kapazitiver Bodenfeuchtesensor (analog, v2.0 / HW-390 mit TLC555 und Spannungsregler), an ADC1-Pin. |
| H3 | Sensor-Versorgung über GPIO schaltbar, nur während der Messung aktiv. |
| H4 | Versorgung: 1S-LiPo/Li-Ion (3,0–4,2 V); das SuperMini hat keinen Lader, Laden extern. Optional Spannungsteiler 2×100 kΩ zur Akkumessung. |
| H5 | Ein Pin (Taster/Jumper nach GND) startet den Konfigurationsmodus. |

## 3. Funktionale Anforderungen
| ID | Anforderung |
|----|-------------|
| F1 | Das Gerät wacht per Timer auf, misst, sendet, geht in Deep Sleep. Kein Dauerbetrieb. |
| F2 | Die Messung mittelt mehrere ADC-Samples (Rauschunterdrückung). |
| F3 | Der Rohwert (mV) wird anhand zweier Kalibrierpunkte (trocken/nass) in 0–100 % umgerechnet. |
| F4 | Gießempfehlung: Feuchte < einstellbarer Schwelle ⇒ `water = 1`, sonst `0`. |
| F5 | Der Akkustand (mV) wird mitgesendet, sofern H4 verbaut ist. |
| F6 | Übertragung per Push an FHEM (kein Polling, da Deep Sleep): HTTP-Request an FHEMWEB, der Readings eines Dummy-Devices setzt (`moisture`, `raw_mv`, `battery_mv`, `water`). |
| F7 | Bei Übertragungsfehler (WLAN/FHEM) erneuter Versuch nach kürzerem, einstellbarem Intervall. |
| F8 | Konfiguration ohne Neuflashen über lokalen Webserver (Access Point `ChiliSensor-Setup`, 192.168.4.1). |
| F9 | Einstellbar: WLAN, FHEM-Host/Port/Device, Intervall, Wiederholintervall, Gießschwelle, Kalibrierung. |
| F10 | Kalibrierung per Klick: „aktueller Wert = trocken/nass“ inkl. Live-Anzeige. |
| F11 | Konfigurationsmodus startet bei fehlender Konfiguration oder gedrücktem Config-Pin beim Reset und endet nach Speichern oder 5 min Timeout. |
| F12 | Einstellungen liegen im NVS und überstehen Deep Sleep, Reset und Akkuwechsel. |

## 4. Nichtfunktionale Anforderungen
| ID | Anforderung |
|----|-------------|
| N1 | Energie: Aktive Zeit pro Zyklus möglichst < 5 s; Ziel: Monate Laufzeit bei 60-min-Intervall mit 1000–2000 mAh. |
| N2 | Schlanker, gut lesbarer Code; keine unnötigen Abhängigkeiten (nur Arduino-ESP32-Core). |
| N3 | Aufbau mit PlatformIO; Pins per Build-Flag änderbar. |
| N4 | Lizenz: MIT. |

## 5. Schnittstelle FHEM
- Transport: `GET http://<host>:<port>/fhem?XHR=1&cmd=setreading <dev> <reading> <wert>;…`
- Voraussetzung in FHEM: `define chili1 dummy` und FHEMWEB ohne CSRF-Token für das Gerät
  (`attr WEB csrfToken none` oder `allowed`-Konzept).
- Readings: `moisture` (%), `raw_mv`, `battery_mv`, `water` (0/1).

## 6. Offen / spätere Erweiterungen
- OTA-Update (wie im ESP32-temperature-sensor-Projekt).
- Alternative Transporte: MQTT, Telnet-Push.
- Mehrere Sensoren pro Gerät (ADC1 hat im C3 nur 5 Kanäle).
- Kurzes Konfigurationsfenster nach Senden (Remote-Parametrierung ohne Taster).
- Akku-Warnschwelle als eigenes Reading.
- Glättung/Trend (z. B. „trocknet in ~2 Tagen aus“).
