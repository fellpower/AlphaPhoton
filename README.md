<p align="center"><img src="assets/alpha-photon-banner.png" alt="Alpha Photon" width="900"></p>

<p align="center"><a href="#english">English</a> · <a href="#deutsch">Deutsch</a></p>

---

<a id="english"></a>

# English

Alpha Photon turns an M5StickC Plus 1.1 into a compact Bluetooth camera remote with autofocus, photo, video, interval, timelapse and astrophotography BULB sequences. It was developed and hardware-tested with a Sony α6400 using its RMT-P1BT-compatible Bluetooth remote mode. No Wi-Fi or phone is required.

> [!NOTE]
> Alpha Photon is an independent community project and is not affiliated with Sony or M5Stack. Product names are used only to describe compatibility.

## Features

- encrypted BLE pairing and automatic reconnection
- autofocus, photo and video start/stop
- flicker-free graphical display with REC timer, focus and battery status
- session photo/clip counters and automatic display dimming
- unlimited interval shooting
- timelapse with configurable interval and image count
- astro BULB with exposure, pause and image count
- event-driven wait for the camera's `ShutterReady` notification
- decoded BLE diagnostics over USB serial

## Hardware and compatibility

<p align="center"><img src="assets/m5stickc-plus-controls.png" alt="Illustrated M5StickC Plus 1.1 with Alpha Photon button assignments" width="850"></p>

<p align="center"><sub>Original project illustration of the M5StickC Plus 1.1 form factor and controls; appearance may vary. See the <a href="https://docs.m5stack.com/en/core/m5stickc_plus">official hardware documentation</a>.</sub></p>

- M5StickC Plus 1.1 (ESP32-PICO-D4, 4 MB flash)
- a compatible camera with Bluetooth remote-control mode
- a USB-C **data** cable

Tested: Sony α6400 / ILCE-6400 with firmware 2.00 or newer. Other models may use the same protocol but are not considered supported until tested on hardware.

## Controls

| Button | Main screen |
|---|---|
| Front button, short press | Video start/stop |
| Side button B, hold | Autofocus |
| Power button C, short press | Take photo |
| Front button, hold 1.2 seconds | Open Tools |
| Power button, long press | Power off |

In Tools: the side button changes the selection/value, the front button confirms/starts, and Power goes back. Press the front button to stop a running sequence.

Available tools: `INTERVAL` (unlimited), `TIMELAPSE` (interval + count) and `ASTRO BULB` (exposure + pause + count).

## Pair the camera

On an α6400:

1. `MENU → Network → Bluetooth Settings → Bluetooth Function → On`
2. `MENU → Network → Bluetooth Remote Ctrl → On`
3. `MENU → Network → Bluetooth Settings → Pairing`
4. Power on Alpha Photon and confirm the camera's pairing dialog.

The bond is stored on the ESP32. Later starts reconnect automatically.

## Install a release (no source build)

Download these two files from the [latest release](https://github.com/fellpower/AlphaPhoton/releases/latest):

- `alpha-photon-m5stickc-plus-1.1-v0.1.0.bin` — complete merged flash image
- matching `.sha256` file — optional integrity check

### 1. Install the USB driver

The M5StickC Plus 1.1 uses an FTDI USB serial interface. Install the driver linked in the [official M5Stack documentation](https://docs.m5stack.com/en/core/m5stickc_plus), then reconnect the stick. On Windows it should appear as `USB Serial Port (COMx)`.

### 2. Install esptool

Install [Python](https://www.python.org/downloads/) and run:

```bash
python -m pip install --upgrade esptool
```

### 3. Find the port

- Windows: Device Manager → Ports, for example `COM4`
- macOS: usually `/dev/cu.usbserial-*`
- Linux: usually `/dev/ttyUSB0`

Close PlatformIO, serial monitors, M5Burner or other programs using that port.

### 4. Flash the merged image

Windows example:

```powershell
python -m esptool --chip esp32 --port COM4 --baud 460800 write_flash 0x0 .\alpha-photon-m5stickc-plus-1.1-v0.1.0.bin
```

macOS/Linux example:

```bash
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 ./alpha-photon-m5stickc-plus-1.1-v0.1.0.bin
```

The release file is a complete 4 MB-layout image and must be written at offset `0x0`.

### Boot/download mode

Normally **no BOOT button is required**. The built-in FTDI interface automatically resets the ESP32 into download mode. The large front button is an application button, not a BOOT button.

If esptool remains at `Connecting...`:

1. close every program using the serial port;
2. unplug USB, power-cycle the stick, and reconnect it with a known data cable;
3. retry at `--baud 115200`;
4. reinstall the FTDI driver if no serial port appears.

Flashing replaces the installed firmware. BLE pairing data may need to be recreated if the flash was erased separately.

## Build from source

Install [PlatformIO](https://platformio.org/) and run:

```powershell
pio run
pio device list
pio run --target upload --upload-port COM4
pio device monitor --port COM4 --baud 115200 --filter time
```

## Astro/BULB

Set the camera to photo mode, `M`, and turn shutter speed past `30″` to `BULB`. Set aperture, ISO and preferably manual focus. Disable silent shooting and continuous/bracketing modes if BULB is unavailable.

The α6400 uses a Bluetooth toggle sequence: one full click opens the shutter, a second closes it. Alpha Photon waits for the configured exposure between those clicks, then waits for `02 A0 00` (`ShutterReady`) before starting the extra pause. This also accommodates in-camera long-exposure noise reduction.

## Protocol and references

- Service `8000ff00-ff00-ffff-ffff-ffffffffffff`
- commands on `FF01`, camera notifications on `FF02`
- [Freemote](https://github.com/coral/freemote)
- [α-Remote](https://github.com/Staacks/alpharemote)
- [Furble](https://github.com/gkoh/furble)
- [Alpha Fairy](https://github.com/frank26080115/alpha-fairy)
- [α6400 Bluetooth remote help](https://helpguide.sony.net/ilc/1810/v1/en/contents/TP0002392816.html)

## License and acknowledgements

Alpha Photon is released under the [MIT License](LICENSE). Copyright © 2026 Alpha Photon contributors. See [NOTICE](NOTICE) for acknowledgements and upstream project references, including Alpha Fairy by Frank Zhao.

<p align="right"><a href="#english">Back to language selection</a></p>

---

<a id="deutsch"></a>

# Deutsch

Alpha Photon verwandelt einen M5StickC Plus 1.1 in eine kompakte Bluetooth-Kamerafernbedienung mit Autofokus, Foto, Video, Intervall, Zeitraffer und Astro-BULB. Entwickelt und praktisch getestet wurde die Firmware mit einer Sony α6400 im RMT-P1BT-kompatiblen Bluetooth-Fernbedienungsmodus. WLAN oder Smartphone sind nicht erforderlich.

> [!NOTE]
> Alpha Photon ist ein unabhängiges Community-Projekt und steht in keiner Verbindung zu Sony oder M5Stack. Produktnamen dienen ausschließlich der Kompatibilitätsbeschreibung.

## Funktionen

- verschlüsselte BLE-Kopplung und automatische Wiederverbindung
- Autofokus, Foto und Video Start/Stopp
- flackerfreie grafische Anzeige mit REC-Timer, Fokus- und Akkustatus
- Foto-/Clip-Zähler und automatische Display-Abdunklung
- unbegrenzter Intervallmodus
- Zeitraffer mit Intervall und Bildanzahl
- Astro-BULB mit Belichtungszeit, Pause und Bildanzahl
- ereignisgesteuertes Warten auf `ShutterReady`
- dekodierte BLE-Diagnose über USB-Serial

## Hardware und Kompatibilität

<p align="center"><img src="assets/m5stickc-plus-controls.png" alt="Illustrierter M5StickC Plus 1.1 mit Alpha-Photon-Tastenbelegung" width="850"></p>

<p align="center"><sub>Eigene Projektillustration der Bauform und Tasten des M5StickC Plus 1.1; das Aussehen kann abweichen. Siehe die <a href="https://docs.m5stack.com/en/core/m5stickc_plus">offizielle Hardware-Dokumentation</a>.</sub></p>

- M5StickC Plus 1.1 (ESP32-PICO-D4, 4 MB Flash)
- kompatible Kamera mit Bluetooth-Fernbedienungsmodus
- USB-C-**Datenkabel**

Getestet: Sony α6400 / ILCE-6400 mit Firmware 2.00 oder neuer. Weitere Modelle gelten erst nach einem Hardwaretest als unterstützt.

## Bedienung

| Taste | Hauptansicht |
|---|---|
| Große Fronttaste kurz | Video Start/Stopp |
| Seitentaste B halten | Autofokus |
| Power-Taste C kurz | Foto |
| Fronttaste 1,2 Sekunden halten | Tools öffnen |
| Power-Taste lang | Ausschalten |

In Tools ändert die Seitentaste Auswahl/Wert, die Fronttaste bestätigt/startet und Power geht zurück. Während einer Sequenz stoppt die Fronttaste.

Verfügbare Modi: `INTERVAL` (unbegrenzt), `TIMELAPSE` (Intervall + Anzahl) und `ASTRO BULB` (Belichtung + Pause + Anzahl).

## Kamera koppeln

1. `MENU → Netzwerk → Bluetooth-Einstlg. → Bluetooth-Funktion → Ein`
2. `MENU → Netzwerk → Bluetooth-Fernbed. → Ein`
3. `MENU → Netzwerk → Bluetooth-Einstlg. → Kopplung`
4. Alpha Photon einschalten und den Kopplungsdialog bestätigen.

Die Bindung bleibt im ESP32 gespeichert; spätere Starts verbinden automatisch.

## Release installieren (ohne Quellcode-Build)

Von der [neuesten Release-Seite](https://github.com/fellpower/AlphaPhoton/releases/latest) herunterladen:

- `alpha-photon-m5stickc-plus-1.1-v0.1.0.bin` — vollständiges, zusammengeführtes Flash-Image
- passende `.sha256`-Datei — optionale Integritätsprüfung

### 1. USB-Treiber installieren

Der M5StickC Plus 1.1 verwendet eine FTDI-USB-Seriell-Schnittstelle. Den Treiber aus der [offiziellen M5Stack-Dokumentation](https://docs.m5stack.com/en/core/m5stickc_plus) installieren und den Stick neu verbinden. Unter Windows erscheint er als `USB Serial Port (COMx)`.

### 2. esptool installieren

[Python](https://www.python.org/downloads/) installieren und ausführen:

```bash
python -m pip install --upgrade esptool
```

### 3. Port ermitteln

- Windows: Geräte-Manager → Anschlüsse, beispielsweise `COM4`
- macOS: meistens `/dev/cu.usbserial-*`
- Linux: meistens `/dev/ttyUSB0`

PlatformIO, serielle Monitore, M5Burner und andere Programme schließen, die den Port verwenden.

### 4. Komplett-Image flashen

Windows:

```powershell
python -m esptool --chip esp32 --port COM4 --baud 460800 write_flash 0x0 .\alpha-photon-m5stickc-plus-1.1-v0.1.0.bin
```

macOS/Linux:

```bash
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 ./alpha-photon-m5stickc-plus-1.1-v0.1.0.bin
```

Die Release-Datei enthält das vollständige 4-MB-Flashlayout und muss an Offset `0x0` geschrieben werden.

### Boot-/Download-Modus

Normalerweise ist **kein BOOT-Taster erforderlich**. Die eingebaute FTDI-Schnittstelle setzt den ESP32 beim Flashen automatisch in den Download-Modus. Die große Fronttaste ist eine Anwendungstaste und keine BOOT-Taste.

Falls esptool bei `Connecting...` stehen bleibt:

1. alle Programme schließen, die den seriellen Port verwenden;
2. USB trennen, Stick aus-/einschalten und mit einem sicheren Datenkabel neu verbinden;
3. erneut mit `--baud 115200` versuchen;
4. den FTDI-Treiber neu installieren, falls kein Port erscheint.

Das Flashen ersetzt die installierte Firmware. Wenn der Flash zuvor separat gelöscht wurde, muss Bluetooth eventuell neu gekoppelt werden.

## Aus Quellcode bauen

[PlatformIO](https://platformio.org/) installieren und ausführen:

```powershell
pio run
pio device list
pio run --target upload --upload-port COM4
pio device monitor --port COM4 --baud 115200 --filter time
```

## Astro/BULB

Kamera auf Fotomodus und `M` stellen, Verschlusszeit über `30″` hinaus auf `BULB` drehen sowie Blende, ISO und vorzugsweise manuellen Fokus einstellen. Falls BULB fehlt, geräuschlose Aufnahme und Serien-/Reihenmodi deaktivieren.

Die α6400 verwendet per Bluetooth eine Toggle-Sequenz: Ein vollständiger Klick öffnet, ein zweiter schließt den Verschluss. Alpha Photon wartet dazwischen die eingestellte Belichtungszeit und danach auf `02 A0 00` (`ShutterReady`). Erst anschließend läuft die zusätzliche Pause. So wird auch die kamerainterne Langzeit-Rauschminderung berücksichtigt.

## Protokoll und Referenzen

- Service `8000ff00-ff00-ffff-ffff-ffffffffffff`
- Befehle über `FF01`, Kamerastatus über `FF02`
- [Freemote](https://github.com/coral/freemote)
- [α-Remote](https://github.com/Staacks/alpharemote)
- [Furble](https://github.com/gkoh/furble)
- [Alpha Fairy](https://github.com/frank26080115/alpha-fairy)
- [α6400: Bluetooth-Fernbedienung](https://helpguide.sony.net/ilc/1810/v1/de/contents/TP0002407921.html)
- [α6400: Bulb-Aufnahme](https://helpguide.sony.net/ilc/1810/v1/de/contents/TP0002274964.html)

## Lizenz und Danksagungen

Alpha Photon wird unter der [MIT-Lizenz](LICENSE) veröffentlicht. Copyright © 2026 Alpha Photon contributors. Danksagungen und Hinweise zu den als Referenz verwendeten Projekten – darunter Alpha Fairy von Frank Zhao – stehen in [NOTICE](NOTICE).

<p align="right"><a href="#english">Zur Sprachauswahl</a></p>
