# Victron VE.Direct touchscreen dashboard — technische notities (CYD)

> Gebruikersdocumentatie: zie [README.md](README.md) (Engels) en
> [INSTALLATIE.md](INSTALLATIE.md) (Nederlands). Dit bestand bevat de
> technische achtergrond en hardware-bevindingen.

Poort van het oorspronkelijke ESP32-D/1.8"-project naar een 320x240 ST7789
touchscreen-board (Sunton "Cheap Yellow Display", ESP32-2432S028R, resistief
XPT2046-touchscreen). Geen PIR, geen knop, geen RTC — navigatie via
tikken/swipen, en alle "dag"-statistiek is een **rollend 24-uursvenster**
sinds het laatste opstarten/rollover (geen kalenderklok aan boord).

## 1. Libraries installeren (Arduino IDE, Library Manager)

- **TFT_eSPI** (Bodmer)
- **XPT2046_Touchscreen** (Paul Stoffregen)
- `Preferences` en `SD` zijn onderdeel van de ESP32-core, niets extra nodig.
- ESP32 Arduino Core **3.x** (zelfde eis als het originele project, voor de
  nieuwe `ledcAttach`/`ledcWrite`-API).

## 2. TFT_eSPI configureren (verplicht, éénmalig)

TFT_eSPI leest zijn pin-configuratie uit een bestand in de LIBRARY-map, niet
uit de sketch. Zie `User_Setup_CYD.h` in deze map voor de exacte stappen:
kopieer het naar `Documents\Arduino\libraries\TFT_eSPI\User_Setup.h` (zet het
bestaande bestand even opzij i.p.v. het te overschrijven).

## 3. Bedrading VE.Direct (alleen lezen)

| Signaal | GPIO | Opmerking |
|---|---|---|
| MPPT TX -> ESP RX | 35 | input-only pin, prima voor RX |
| Shunt TX -> ESP RX | 22 | |
| ESP TX -> beide RX-ingangen | 27 | simpele Y-splitsing, GEEN multiplexer nodig (zie toelichting in `ve_direct.h`) |
| GND | - | gemeenschappelijke GND verplicht |

De `+V`-pin van de VE.Direct-connector blijft los (alleen voor Victron-eigen
accessoires). Zie `INSTALLATIE.md` van het originele project voor de
uitleg van de 4-pins VE.Direct-stekker (die blijft ongewijzigd geldig).

Zonder pinnen ingesteld (of zonder dat er data binnenkomt binnen
`VEDIRECT_WAIT_MS`) start het systeem automatisch in **DEMO**-modus.

Tijdens het zoeken naar VE.Direct-data (het opstartscherm, max
`VEDIRECT_WAIT_MS`) scrollt onderin een Star Wars-achtige "crawl" met precies
deze bedradingstabel omhoog (dynamisch opgebouwd uit de GPIO-nummers hierboven,
dus altijd correct als je de pinnen in `config.h` wijzigt). Eén volledige pas
duurt 10s (leesbaar tempo), past ruim binnen de 15s zoektijd en begint vanzelf opnieuw
als hij eerder klaar is dan de zoektijd om is (`drawWiringCrawl()` in
`VictronDashboardCYD.ino`, gebruikt `tft.setViewport()` om de tekst netjes
binnen het kadertje te clippen).

## 4. Eerste keer opstarten

- Bij een lege NVS start automatisch de **touchscreen-kalibratie**: tik het
  kruisje linksboven en daarna rechtsonder aan. Dit hoeft daarna nooit meer
  (opnieuw kalibreren kan altijd via Instellingen).
- Daarna een kort opstartscherm, en dan het DASHBOARD.

## 5. Bediening

- **Swipe links/rechts** = volgende/vorige pagina (7 pagina's, in vaste volgorde).
- **Tik op een kaart** (Dashboard: Zon/Accu/Verbruik) = direct naar die pagina.
- **Tik het rasterpictogram** (rechtsboven, op elke pagina) = rechtstreeks naar Instellingen.
- **Tik de terug-pijl** (linksboven, op elke detailpagina/Instellingen) = terug naar Dashboard.
- **Instellingen**: helderheid, scherm uit na screensaver (Ja/Nee), accuprofiel,
  touchscreen opnieuw kalibreren, SD-kaartstatus.

## 6. Hardware-bevindingen (met een eigen testscript vastgesteld, 2026-08-24)

Dit specifieke exemplaar wijkt op een paar punten af van de gangbare online
"CYD"-documentatie - onderstaande is GEVERIFIEERD op echte hardware, niet
aangenomen:

- **Scherm is een ST7789, geen ILI9341.** Verklaarde het witte/lege scherm:
  verkeerde init-commando's voor die driver. `User_Setup_CYD.h` staat al goed
  (`ST7789_DRIVER`, **BGR**-kleurvolgorde (het testscript zei RGB, maar op het
  scherm waren rood/blauw verwisseld), geen inversie, TFT_WIDTH/HEIGHT 240x320).
  Zelfde pinnen als eerst aangenomen (SCK14/MOSI13/MISO12/CS15/DC2/RST=-1).
- **microSD zit op een eigen VSPI-bus** (SCK18/MISO19/MOSI23/CS5), niet
  gedeeld met het scherm zoals ik eerder abusievelijk schreef. `config.h` is
  bijgewerkt; geen codewijziging nodig omdat `SD.begin()` toevallig al het
  standaard ESP32-SPI-object op die pinnen gebruikt.
- **Touch (XPT2046)** faalde in het eerste testscript, maar werkt in het
  dashboard zelf (pinnen CS33/IRQ36/MOSI32/MISO39/CLK25, eigen HSPI-bus via
  `XPT2046_Touchscreen`). Kalibratie bij eerste start; bij "geen touch" zie
  de `[CAL]`-regels in de Serial Monitor.
- **LDR / speaker / BOOT-knop faalden in de test.** De LDR (auto-helderheid) is
  inmiddels uit het ontwerp verwijderd. De luidspreker (alarm-piep) is
  optioneel en niet blokkerend voor de rest van het dashboard.

## 7. Overige aandachtspunten / nog te verifiëren

Er is in deze omgeving **geen Arduino-compiler beschikbaar** om dit tegen te
compileren — alles is met zorg geschreven en tegen de bestaande
projectconventies gecontroleerd, maar de onderstaande punten zijn de
waarschijnlijkste kandidaten voor een volgend compile-foutje:

- **`tft.drawArc()`-signatuur**: gebruikt voor alle ring-gauges. De
  parametervolgorde/naam van het laatste (smooth/roundEnds) argument kan
  per TFT_eSPI-versie licht verschillen — pas zo nodig aan in `widgets.h`.
- **`tft.textWidth()`/`tft.drawString()`/tekst-datums** (TL_DATUM etc.):
  standaard TFT_eSPI-API, maar controleer dat `LOAD_GFXFF`/de juiste fonts
  aanstaan in `User_Setup.h` (al meegenomen in `User_Setup_CYD.h`).
- **`tft.setViewport()`/`resetViewport()`** (opstartscherm, bedradingscrawl):
  standaard TFT_eSPI-API voor een geclipt tekengebied, al lang stabiel in de
  library, maar zoals altijd geldt: is een oudere TFT_eSPI-versie geïnstalleerd
  en compileert dit niet, dan is dit de eerste plek om te checken.
- **Software-UART-timing** (`ve_direct.h`, `swWriteByte`): bit-banged TX op
  de gedeelde HEX-opvraaglijn (GPIO27), getimed op 52µs/bit (19200 baud).
  Dit is alleen voor de instellingen-opvraging (niet de live-data), dus een
  kleine timingsafwijking is niet kritiek — als de MPPT/SHUNT-INFO-pagina
  leeg blijft ("...") terwijl LIVE-data verder goed binnenkomt, is dit de
  eerste plek om te controleren.
- **Rondom `map()`/`constrain()`**: overal met `int` aangenomen; als een
  compiler klaagt over ambiguë overloads, expliciet casten naar `long`/`int`.
- **`tone()` op de SPEAKER_PIN** (alarm-piep): vereist ESP32 Arduino Core 3.x
  (oudere cores kenden dit niet voor willekeurige GPIO's). Zelfde eis als de
  rest van het project, dus geen extra afhankelijkheid, maar wel iets om te
  checken als de compiler hierover klaagt.

Meld compiler-foutmeldingen gewoon terug — dat is de snelste manier om dit
verder af te maken zonder dat ik hier zelf kan compileren.

## 9. Bewuste vereenvoudigingen t.o.v. de eerder getoonde mockups

- **Paginakiezer is een lijst, geen grid.** Grotere tikdoelen zijn
  betrouwbaarder op een resistief scherm; functioneel identiek (tik = spring
  naar die pagina).
- **Helderheid-schuifregelaar reageert op TIKKEN, niet slepen.** Het
  gebaar-model onderscheidt alleen "tik" en "swipe" (bepaald pas ná
  loslaten); een tik ergens op de balk zet de helderheid naar die positie.
  Slepen live volgen kan later toegevoegd worden door tijdens het vasthouden
  te blijven tekenen, maar dat is een aparte uitbreiding.
- **Geen sprite-dubbelbuffering.** Widgets tekenen direct naar het scherm
  (met dezelfde cache-aanpak als vroeger: alleen opnieuw tekenen bij
  wijziging). Dat hoort al flikkervrij te zijn omdat TFT_eSPI tekst/vlakken
  opaak tekent; mocht er op echte hardware toch zichtbare flicker optreden op
  een specifiek widget, dan is per-widget sprites de volgende stap.
- **Ingebouwde TFT_eSPI-fonts (2 en 4), geen "smooth" anti-aliased fonts.**
  Nette proportionele fonts zonder extra bestanden te hoeven uploaden; echte
  anti-aliasing (.vlw-fonts vanaf LittleFS/SD) kan later als upgrade.

## 8. Wat is bewust weggelaten t.o.v. het origineel

- PIR-bewegingssensor, drukknop, DS1302-RTC — vervangen door touch-navigatie
  en een rollend 24u-venster (zie `CLAUDE.md`-projectgeschiedenis voor de
  discussie waarom er geen vervangende RTC/WiFi+NTP bij kon op de schaarse
  GPIO's van dit board).
- SIMPEL/NORMAAL-thema's — er is nu precies één ontwerp.
- Fysieke alarm-uitgang — vervangen door de onboard luidspreker (GPIO26) +
  een schermbanner (bij ALARM-niveau volgens `systemAlertLevel()`).
