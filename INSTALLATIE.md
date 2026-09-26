# Victron VE.Direct touchscreen dashboard — installatiegids

Stap voor stap van een kaal CYD-bordje naar een werkend dashboard. Overzicht en uitleg van de pagina's: zie [README.md](README.md).

## 1. Wat heb je nodig

- ESP32-2432S028R ("Cheap Yellow Display") met USB-kabel
- Victron MPPT en/of SmartShunt met VE.Direct-poort
- VE.Direct-kabel(s) (4-pins JST-PH 2.0), of losse draden naar de stekker
- Arduino IDE met **ESP32-core 3.x**

Je kunt alles eerst **zonder Victron-apparatuur** proberen: het dashboard start dan zelf in DEMO-modus.

## 2. Software voorbereiden

1. Arduino IDE → Boards Manager → **esp32 (Espressif)**, versie 3.x.
2. Library Manager → installeer **TFT_eSPI** en **XPT2046_Touchscreen**.
3. Configureer TFT_eSPI (éénmalig, zie hoofdstuk 3).
4. Open `VictronDashboardCYD/VictronDashboardCYD.ino` (de sketch staat in de submap met dezelfde naam, dat eist de Arduino IDE). Kies als board *ESP32 Dev Module*. Is de sketch te groot: Tools → Partition Scheme → **Huge APP**.

## 3. TFT_eSPI instellen (verplicht)

TFT_eSPI leest zijn instellingen uit de library-map, niet uit de sketch.

1. Ga naar `Documents\Arduino\libraries\TFT_eSPI`.
2. Hernoem `User_Setup.h` naar `User_Setup.h.orig` (bewaren als reserve).
3. Kopieer `User_Setup_CYD.h` uit de hoofdmap van de repo naar die map en hernoem het naar `User_Setup.h`.
4. Controleer in `User_Setup_Select.h` dat de regel `#include <User_Setup.h>` **niet** uitgecommentarieerd is.
5. Herstart de Arduino IDE.

Belangrijk voor dit bordje: **ST7789**-driver, kleurvolgorde **BGR**, geen inversie.

## 4. Uploaden en eerste start

1. Sluit het bordje aan en kies de juiste COM-poort.
2. Upload de sketch.
3. Bij de eerste start vraagt het scherm om **touchscreen-kalibratie**: tik het kruisje linksboven en daarna rechtsonder. Dit wordt bewaard.
4. Het opstartscherm zoekt maximaal 15 seconden naar VE.Direct-data en toont ondertussen de bedrading. Komt er data binnen, dan gaat hij meteen door naar **LIVE**. Anders start hij in **DEMO**.

Tip: open de Serial Monitor op 115200 baud. De sketch logt uitgebreid wat hij doet (opstartstappen, touch, backlight, pagina's).

## 5. Bedrading

VE.Direct is 3,3V-serieel. **TX en RX kruisen**: TX van de Victron gaat naar RX van de ESP32.

| Signaal | ESP32 GPIO |
|---|---|
| MPPT TX → ESP32 RX | 35 |
| SmartShunt TX → ESP32 RX | 22 |
| ESP32 TX → RX van beide apparaten | 27 (Y-splitsing) |
| GND van alle drie | GND (gemeenschappelijk, verplicht) |

- De **+V**-pin van de VE.Direct-stekker blijft **los** (alleen voor Victron-accessoires).
- GPIO 27 is alleen nodig om instellingen uit de apparaten te lezen (APPARATEN-pagina). Zonder die draad werken de live-waarden gewoon.
- Werkt maar één apparaat? Dan toont het dashboard alleen wat er is; de rest wordt grijs of "--".
- Andere pinnen nodig? Pas `VEDIRECT_MPPT_RX`, `VEDIRECT_SHUNT_RX` en `VEDIRECT_SHARED_TX` aan in `VictronDashboardCYD/config.h`. De bedrading-tekst op het opstartscherm past vanzelf mee.

## 6. Instellen naar jouw installatie

In `VictronDashboardCYD/config.h`:

- **Accutype**: `BATTERY_LIFEPO4` of `BATTERY_LEAD` (ook later te wisselen in Instellingen)
- **Capaciteit**: `BATTERY_CAPACITY_AH`
- **Verbruik-kleuren**: `LOAD_GREEN_W`, `LOAD_YELLOW_W`, `LOAD_ORANGE_W`
- **Screensaver/scherm uit**: `DIM_TIMEOUT_MS` (30 s), `OFF_TIMEOUT_MS` (5 min)
- **Alarm**: `ALARM_ENABLED` (luidspreker aan/uit)
- **Apparaatnamen**: `MPPT_MODEL`, `SHUNT_MODEL`

## 7. Bediening

- Swipe links/rechts: volgende/vorige pagina
- Tik op een kaart: details
- Rasterpictogram rechtsboven: Instellingen
- Pijl linksboven: terug naar Dashboard
- Elke aanraking wekt het scherm

**Instellingen**: helderheid (minimaal 15%), scherm uit na screensaver (Ja/Nee), accuprofiel, touch opnieuw kalibreren, SD-kaartstatus.

## 8. Problemen oplossen

| Probleem | Oorzaak / oplossing |
|---|---|
| Wit of leeg scherm | Verkeerde driver. Controleer `User_Setup.h`: `ST7789_DRIVER`, en dat `User_Setup_Select.h` het bestand includet. Herstart de IDE na een wijziging. |
| Rood en blauw verwisseld | Zet `TFT_RGB_ORDER` op `TFT_BGR` (staat zo in `User_Setup_CYD.h`). |
| Scherm blijft zwart, touch reageert wel (zie log) | Backlight staat uit of te laag opgeslagen. Sinds deze versie is er een ondergrens van 15%. Zie `[BL]`-regels in de Serial Monitor. |
| Touch reageert niet | Kijk in de Serial Monitor of `[CAL]` meldt "geen touch-signaal". Controleer de touch-pinnen in `VictronDashboardCYD/config.h` (CS33, IRQ36, MOSI32, MISO39, CLK25 op de geteste print). |
| Touch klopt niet met wat je aantikt | Opnieuw kalibreren: Instellingen → Aanraakscherm kalibreren. |
| Altijd DEMO ondanks aangesloten Victron | Controleer TX/RX gekruist, gemeenschappelijke GND en de pinnen in `VictronDashboardCYD/config.h`. Als de kabel later wordt aangesloten schakelt het dashboard zelf om naar LIVE. |
| APPARATEN-pagina toont "..." | De HEX-opvraging via GPIO 27 ontvangt niets; live-waarden zijn hiervan onafhankelijk. |
| Compileerfout rond `drawArc` of `setViewport` | Versieverschil in TFT_eSPI. Update de library of pas de aanroep aan. |
| Geen geluid bij alarm | `ALARM_ENABLED 1`? Speaker-pin (GPIO 26) en luidspreker aanwezig op je bordje? |

## 9. Veiligheid

- De firmware **schrijft nooit** naar de MPPT of SmartShunt; er worden alleen waarden gelezen.
- Werk aan bedrading altijd spanningsloos en respecteer de specificaties van je installatie.
- Onafhankelijk hobbyproject, niet gelieerd aan Victron Energy. Gebruik op eigen risico.
