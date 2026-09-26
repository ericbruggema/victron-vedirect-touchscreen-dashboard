# Changelog — Victron VE.Direct touchscreen dashboard

Nieuwste bovenaan.

## 1.0-cyd (2026-08/09)

Poort van het ESP32-D/1.8"-project naar de ESP32-2432S028R (320x240, touch).

- (+) Docs: README (Engels), INSTALLATIE (Nederlands), illustratieve demo-schermen in `docs/screenshots/` (gegenereerd met `tools/render_demo_screens.py`), MIT-licentie.
- (+) Touchscreen-bediening (swipe, tik, kalibratie); PIR en drukknop vervallen.
- (+) Eén vast "gedetailleerd" ontwerp met 7 pagina's plus Instellingen; thema's vervallen.
- (+) Rollend 24-uursvenster voor dagstatistiek (geen RTC nodig).
- (+) Gecombineerd opstartscherm en VE.Direct-zoeken (max 15 s, voortgangsbalk, aftelling), met scrollende bedradingsgids en letter-voor-letter credits.
- (+) Energie-flow screensaver na 30 s (PV, verbruik, accu-vermogen en SoC); scherm uit na 5 min, aan/uit te zetten in Instellingen.
- (+) Alarm via onboard luidspreker en schermmelding.
- (+) Instellingen: helderheid (min 15%), scherm uit Ja/Nee, accuprofiel, touchkalibratie, SD-status.
- (+) Uitgebreide Serial-logging (opstart, touch, backlight, pagina's).
- (~) Rasterpictogram opent rechtstreeks Instellingen (paginakiezer ongebruikt).
- (~) Flikkervrij tekenen: statische en dynamische delen van kaarten, rijen, tegels en statusbalk apart gecachet.
- (~) Display: ST7789 met BGR-kleurvolgorde (was ILI9341 aangenomen).
- (−) Automatische helderheid (LDR) en alarm-schakelaar verwijderd.
- (fix) Zwart scherm na opstarten door opgeslagen 0% helderheid: ondergrens `MIN_BRIGHTNESS_PCT`.
