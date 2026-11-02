# esp32-rotary-hue-dimmer

Een draaiknop voor Philips Hue op de **Elecrow CrowPanel 1.28" Rotary Display**. De knop bedient de `StudeerZone` rechtstreeks via de Hue Bridge. Geschreven in C met ESP-IDF en LVGL 9.

![Schermen](docs/preview.png)

## Bediening

| Lampen | Actie | Resultaat |
|---|---|---|
| Uit | Ring draaien | Er gebeurt niets |
| Uit | Knop indrukken | Aan op 30% |
| Aan | Ring rechtsom | Feller, tot 100% |
| Aan | Ring linksom | Dimmer, tot 1%; de lampen blijven aan |
| Aan | Knop indrukken | Uit |

Wijzigingen uit de Hue-app verschijnen ook op de knop. Het bolletje op het scherm toont de status: geel is verbinden, rood is een fout, groen is aan en grijs is uit.

## Wat je ziet

- **Wifi-symbool:** klein symbool bovenaan in het midden. Groen met streepjes voor de signaalsterkte (3 vanaf -60 dBm, 2 vanaf -70, 1 vanaf -80), donkerrood zonder wifi. Het toont alleen de wifi; de verbinding met de bridge zie je aan het bolletje.
- **Ledring in de voet:** brandt zacht groen zolang de lampen aan zijn. Zijn de lampen uit of is er geen verbinding met de bridge, dan is de ring uit.

## Benodigdheden

- ESP-IDF **v5.5.3**, bijvoorbeeld via de ESP-IDF-extensie in VS Code
- Een USB-C-kabel die data overbrengt
- Een 2,4 GHz-wifinetwerk waarop de bridge bereikbaar is

LVGL en de schermdriver worden bij de eerste build automatisch gedownload.

## Instellen

Kopieer `main/credentials.example.h` naar `main/credentials.h` en vul je gegevens in. Dit bestand gaat niet mee naar git. Na een wijziging moet je opnieuw flashen.

Een sleutel maak je zo aan. Druk op de knop van de bridge en voer binnen 30 seconden uit:

```bash
curl -sk -X POST https://172.16.100.4/api -d '{"devicetype":"hue_dimmer#esp32"}'
```

De waarde van `username` is de sleutel.

## Bouwen en flashen

**In VS Code:** open deze map, kies onderaan target `esp32s3` en de poort van het bord, en klik op het vlammetje (Build, Flash and Monitor).

**In de terminal of via Claude Code:**

```bash
. ~/.espressif/tools/activate_idf_v5.5.3.sh  # installatie via EIM
ls /dev/cu.*                               # zoek de poort van het bord
idf.py -p /dev/cu.JOUW_POORT flash monitor # afsluiten met Ctrl+]
```

Lukt het uploaden niet? Houd BOOT ingedrukt, druk kort op RESET, laat BOOT los en probeer het opnieuw.

## Instellingen aanpassen

Alle keuzes staan in `main/app_config.h`:

| Instelling | Standaard | Betekenis |
|---|---|---|
| `DIM_ON_PERCENT` | 30 | Helderheid na aanzetten |
| `DIM_STEP_PERCENT` | 2 | Procenten per klik van de ring |
| `HUE_ZONE_NAME` | `StudeerZone` | Exacte naam in de Hue-app |
| `HUE_SEND_INTERVAL_MS` | 500 | Minimale tijd tussen twee opdrachten |
| `ENCODER_COUNTS_PER_STEP` | 4 | Tellingen per voelbare klik |
| `ENCODER_REVERSE` | 0 | Op 1 als de draairichting omgekeerd is |
| `BACKLIGHT_PERCENT` | 60 | Helderheid van het scherm |
| `WIFI_ICON_INTERVAL_MS` | 3000 | Zo vaak wordt de signaalsterkte opnieuw gemeten |
| `BASE_LED_GREEN` | 10 | Helderheid van het groen in de ledring, 0 t/m 255 |

De achtergrondkleur is `C_BACKGROUND` in `main/ui.c`. Gebruik `0x0d1117` voor de tint van GitHub. Daar staan ook de kleuren van het wifi-symbool (`C_WIFI_ON`, `C_WIFI_OFF`) en de grenzen voor de streepjes (`WIFI_RSSI_1` t/m `WIFI_RSSI_3`).

## Opbouw

| Bestand | Inhoud |
|---|---|
| `main/main.c` | Opstarten en de hoofdlus |
| `main/dimmer.c` | De regels van de bediening |
| `main/input.c` | Ring en drukknop |
| `main/ui.c` | Het scherm |
| `main/display_port.c` | Schermdriver en de LVGL-taak |
| `main/wifi.c` | Wifi en signaalsterkte |
| `main/base_leds.c` | Ledring in de voet (5 WS2812-LEDs op GPIO 48) |
| `main/hue.c` | Verzoeken naar de bridge en de eventstream |
| `main/hue_json.c`, `main/hue_sse.c` | Antwoorden en de eventstream uitpakken |
| `sdkconfig.defaults` | Vaste instellingen voor PSRAM, flash, TLS, wifi en lettertypes |

LVGL is niet thread-safe: een aanroep buiten de LVGL-taak moet tussen `display_lock()` en `display_unlock()` staan.

## Bekende beperkingen van versie 1

- **Certificaat:** de firmware controleert het certificaat van de bridge nog niet. Je Python-app doet dat ook niet (`verify=False`). De verbinding is wel versleuteld, maar een ander apparaat op je netwerk zou zich als bridge kunnen voordoen en zo de sleutel kunnen zien.
- **Touch:** wordt nog niet gebruikt.
- **Instellingen:** wifi en sleutel staan vast in de firmware.

## Problemen oplossen

| Op het scherm | Betekenis | Oplossing |
|---|---|---|
| Geel, `verbinden...` | Nog geen wifi of eerste contact | Controleer `WIFI_SSID` en `WIFI_PASSWORD`, en dat het netwerk 2,4 GHz is |
| Wifi-symbool donkerrood | Geen wifi | Controleer `WIFI_SSID` en `WIFI_PASSWORD`, en dat het netwerk 2,4 GHz is |
| Rood, `bridge onbereikbaar` | Geen verbinding met de bridge | Controleer `HUE_BRIDGE_IP` en of het wifinetwerk de bridge kan bereiken |
| Rood, `sleutel geweigerd` | De sleutel klopt niet | Controleer `HUE_APP_KEY` |
| Rood, `zone niet gevonden` | Naam komt niet voor op de bridge | Controleer `HUE_ZONE_NAME`; hoofdletters tellen mee |
| Tekst in spiegelbeeld | Spiegeling van het scherm klopt niet | Pas `esp_lcd_panel_mirror` aan in `main/display_port.c` |
| Ring werkt omgekeerd | Draairichting | Zet `ENCODER_REVERSE` op 1 |
| Ring dimt te snel of te traag | Andere telling per klik | Pas `ENCODER_COUNTS_PER_STEP` aan (bijvoorbeeld 2) |
