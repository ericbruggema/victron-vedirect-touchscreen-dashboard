/* ============================================================================
   pages_systeem_settings.h  -  PAGE_SYSTEEM, PAGE_SETTINGS, PAGE_PICKER
   ----------------------------------------------------------------------------
   Drie "lijst"-pagina's zonder grafieken: systeemstatus (info-only),
   instellingen (interactief: helderheid/scherm-uit/accuprofiel/kalibratie/SD)
   en de paginakiezer (tik-om-te-springen lijst van de 7 swipe-pagina's).
============================================================================ */
#ifndef PAGES_SYSTEEM_SETTINGS_H
#define PAGES_SYSTEEM_SETTINGS_H
#include "dashboard.h"
#include "widgets.h"
#include "config.h"
#include "touch.h"

// ---- PAGE_SYSTEEM -----------------------------------------------------------
void pageSysteemDraw(bool first){
  int x1 = 8, x2 = 164;
  int y0 = CONTENT_TOP + 12;

  // linkerkolom
  wRow(0, x1, y0 + 0*30, 140, "uptime",   fmtUptime(), COL.text);
  wRow(1, x1, y0 + 1*30, 140, "vrij RAM", String(ESP.getFreeHeap()/1024) + " KB", COL.text);
  wRow(2, x1, y0 + 2*30, 140, "NVS",      nvsOk ? "OK" : "RAM-only", nvsOk ? COL.green : COL.amberMid);
  wRow(3, x1, y0 + 3*30, 140, "systeem",  String(systemV) + "V", COL.text);

  // rechterkolom
  wRow(4, x2, y0 + 0*30, 140, "SD-kaart", sdPresent ? "aanwezig" : "niet gevonden", sdPresent ? COL.green : COL.muted);
  wRow(5, x2, y0 + 1*30, 140, "bron",
       dataSource == SRC_LIVE ? (veStale ? "LIVE (wacht)" : "MPPT+SHUNT live") : "DEMO",
       dataSource == SRC_LIVE ? COL.green : COL.amber);
  wRow(6, x2, y0 + 2*30, 140, "cycli",    String(live.cycles), COL.text);
  wRow(7, x2, y0 + 3*30, 140, "firmware", String(FW_VERSION), COL.muted);
}

// ---- PAGE_SETTINGS -----------------------------------------------------------
void pageSettingsDraw(bool first){
  const int y0 = CONTENT_TOP + 10;

  if(first){
    wText(8, y0 + 0*30, 1, COL.muted, "Helderheid");
    wText(8, y0 + 1*30, 1, COL.muted, "Scherm uit na screensaver");
    wText(8, y0 + 2*30, 1, COL.muted, "Accuprofiel");
    wText(8, y0 + 3*30, 1, COL.muted, "Aanraakscherm kalibreren");
    wText(290, y0 + 3*30, 1, COL.muted, ">");
    wText(8, y0 + 4*30, 1, COL.muted, "SD-kaart");
  }

  wSlider(10, 140, y0 + 0*30 + 4, 160, appSettings.brightnessPct);
  wSegmented(11, 200, y0 + 1*30 - 2, "Ja", "Nee", appSettings.screenOffEnabled ? 0 : 1);
  wSegmented(12, 130, y0 + 2*30 - 2, "LiFePO4", "Lood", appSettings.battProfileIdx);
  wValue(14, 220, y0 + 4*30, 1, sdPresent ? COL.green : COL.muted, sdPresent ? "aangesloten" : "niet aangesloten");
}

void pageSettingsTouch(int x, int y, bool justPressed, bool justReleased){
  if(!justReleased) return;
  const int y0 = CONTENT_TOP + 10;
  if(y < y0) return;
  int row = (y - y0) / 30;
  if(row < 0 || row > 4) return;

  switch(row){
    case 0: {
      // ondergrens MIN_BRIGHTNESS_PCT i.p.v. 0: anders kan een tik net links van
      // de balk het scherm op zwart zetten en opslaan in NVS, zonder enige weg
      // terug via de (dan onzichtbare) UI
      int pct = constrain(map(x, 140, 300, 0, 100), MIN_BRIGHTNESS_PCT, 100);
      appSettings.brightnessPct = pct;
      settingsSave();
      break;
    }
    case 1:
      appSettings.screenOffEnabled = (x < 200 + 46);   // "Ja"-segment links, "Nee" rechts
      settingsSave();
      break;
    case 2: {
      int idx = (x < 130 + 80) ? 0 : 1;
      appSettings.battProfileIdx = idx;
      applyBatteryProfile(idx);
      settingsSave();
      break;
    }
    case 3:
      touchRunCalibration();
      resetWidgetCache();
      tft.fillScreen(COL.bg);
      break;
    case 4:
    default:
      break;
  }
}

// ---- PAGE_PICKER --------------------------------------------------------------
// Let op: dit was de eigenlijke bug achter "ik kan verder niets" - Instellingen
// stond nergens als bereikbare rij, dus was er via de UI geen enkele weg naar
// binnen. Rij-hoogte iets verkleind (24 i.p.v. 26) om alle 8 rijen ruim
// binnen het scherm te houden.
void pagePickerDraw(bool first){
  const int rowH = 24;
  for(int i = 0; i < PAGE_COUNT; i++){
    int y = CONTENT_TOP + 2 + i*rowH;
    wRow(i, 8, y, 300, PAGES[i].title, "", COL.text);
    if(first) addTapZone(8, y, 300, rowH, (PageId)i);
  }
  int y = CONTENT_TOP + 2 + PAGE_COUNT*rowH;
  wRow(PAGE_COUNT, 8, y, 300, "INSTELLINGEN", "", COL.blue);
  if(first) addTapZone(8, y, 300, rowH, PAGE_SETTINGS);
}

#endif
