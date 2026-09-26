/* ============================================================================
   pages_dashboard.h  -  PAGE_DASHBOARD (thuispagina) + PAGE_ZON (zon-detail)
   ----------------------------------------------------------------------------
   Tekent alleen de INHOUD tussen CONTENT_TOP en CONTENT_BOTTOM; de statusbalk/
   subheader/navstipjes worden door renderCurrent() (in de .ino) getekend.
============================================================================ */
#ifndef PAGES_DASHBOARD_H
#define PAGES_DASHBOARD_H
#include "dashboard.h"
#include "widgets.h"
#include "config.h"

// Klein, lokaal hulpje: tekent een icoon dat van VORM kan wisselen (zon<->maan)
// zonder spookresten van de vorige vorm. wCard() lost dit intern al op door de
// hele kaart te wissen bij een cache-wijziging; hier (los icoon, geen kaart-
// achtergrond) wissen we zelf een vierkantje eromheen zodra de sleutel (icoon-
// functie + kleur) verandert. Gebruikt de bestaande cache-primitive uit
// widgets.h (zelfde translation unit, dus zichtbaar).
static void wIconCached(int id, int cx, int cy, int r, IconFn fn, uint16_t col){
  String key = String((uint32_t)(uintptr_t)fn) + "|" + String(col);
  if(!cacheChanged(id, key)) return;
  tft.fillRect(cx - r - 2, cy - r - 2, 2*r + 4, 2*r + 4, COL.bg);
  fn(cx, cy, r, col);
}

// ---- PAGE_DASHBOARD ---------------------------------------------------------
void pageDashboardDraw(bool first){
  // Marge minimaal (1px) - nu CONTENT_BOTTOM weer los staat (zie dashboard.h)
  // is er genoeg fysieke ruimte; de kaarten waren te kort, met tekst die
  // onderaan wegviel/tegen de rand liep.
  int y = CONTENT_TOP + 1;
  int h = CONTENT_BOTTOM - y - 1;

  // ---- Kaart 1: ZON ----
  CardData dZon;
  dZon.label     = "ZON";
  dZon.bigValue  = vTxt(solarValid, fmtW(live.pvW));
  dZon.subValue  = vTxt(solarValid, String(live.pvV,1) + "V . " + String(live.pvA,1) + "A");
  dZon.footLabel = "Piek 24u";
  dZon.footValue = vTxt(solarValid, fmtW(day.pvW_max));
  dZon.accent    = vCol(solarValid, COL.amber);
  dZon.ringPct   = (solarValid && day.pvW_max > 0) ? constrain(live.pvW * 100 / day.pvW_max, 0, 100) : -1;
  dZon.icon      = (solarValid && live.pvW > 5) ? iconSun : iconMoon;
  wCard(0, 8, y, 94, h, dZon);

  // ---- Kaart 2: ACCU ----
  float hours; const char* footLabel;
  if(live.battA > 0.05f){
    hours = (BATT.capacityAh * (100 - live.soc) / 100.0f) / live.battA;
    footLabel = "Tot vol";
  } else if(live.battA < -0.05f){
    hours = live.remainingAh / (-live.battA);
    footLabel = "Resterend";
  } else {
    hours = -1;
    footLabel = "Resterend";
  }
  CardData dAccu;
  dAccu.label     = "ACCU";
  dAccu.bigValue  = vTxt(battValid, String(live.soc) + "%");
  dAccu.subValue  = vTxt(battValid, String(live.battV,1) + "V . " + String(live.battA,1) + "A");
  dAccu.footLabel = footLabel;
  dAccu.footValue = (battValid && hours >= 0)
                     ? (String((int)hours) + "u" + String((int)((hours - (int)hours) * 60)) + "m")
                     : String("--");
  dAccu.accent    = vCol(battValid, socColor(live.soc));
  dAccu.ringPct   = battValid ? live.soc : -1;
  dAccu.icon      = iconBattery;
  wCard(1, 112, y, 94, h, dAccu);

  // ---- Kaart 3: VERBRUIK ----
  bool loadOk = solarValid && battValid;
  CardData dLoad;
  dLoad.label     = "VERBRUIK";
  dLoad.bigValue  = vTxt(loadOk, fmtW(live.loadW));
  dLoad.subValue  = vTxt(loadOk, String(live.battV > 1.0f ? live.loadW / live.battV : 0.0f, 1) + "A");
  dLoad.footLabel = "Piek 24u";
  dLoad.footValue = vTxt(loadOk, fmtW(day.load_max));
  dLoad.accent    = loadOk ? loadColor(live.loadW) : COL.muted;
  dLoad.ringPct   = loadOk ? constrain(live.loadW * 100 / LOAD_ORANGE_W, 0, 100) : -1;
  dLoad.icon      = iconBolt;
  wCard(2, 216, y, 94, h, dLoad);

  // ---- Pijltjes tussen de kaarten (statische decoratie) ----
  if(first){
    iconArrowRight(104, CONTENT_TOP + 45, 8, vCol(solarValid, COL.amber));
    iconArrowRight(208, CONTENT_TOP + 45, 8, loadOk ? loadColor(live.loadW) : COL.muted);
  }

  // ---- Tikzones: kaart -> detailpagina ----
  if(first){
    addTapZone(8,   CONTENT_TOP + 4, 94, h, PAGE_ZON);
    addTapZone(112, CONTENT_TOP + 4, 94, h, PAGE_ACCU);
    addTapZone(216, CONTENT_TOP + 4, 94, h, PAGE_ENERGIE);
  }
}

// ---- PAGE_ZON (zon-detail) ---------------------------------------------------
void pageZonDraw(bool first){
  bool sunActive = solarValid && live.pvW > 5;
  uint16_t accent = vCol(solarValid, COL.amber);

  // links: groot icoon + grote waarde (icoon eindigt y<=88, waarde 94-146, label 150-166:
  // strak op elkaar gestapeld zodat de grafiek eronder niet overlapt - zie CONTENT_*-budget)
  wIconCached(10, 60, CONTENT_TOP + 38, 22, sunActive ? iconSun : iconMoon, accent);
  wValueC(0, 60, CONTENT_TOP + 66, 3, accent, vTxt(solarValid, fmtW(live.pvW)));
  if(first) wText(28, CONTENT_TOP + 122, 1, COL.muted, "vermogen");

  // rechts: drie rijen
  int rx = 150, rw = SCR_W - 8 - rx;
  wRow(1, rx, CONTENT_TOP + 30, rw, "Stroom",      vTxt(solarValid, String(live.pvA,1) + " A"), COL.text);
  wRow(2, rx, CONTENT_TOP + 58, rw, "Spanning",    vTxt(solarValid, String(live.pvV,1) + " V"), COL.text);
  wRow(3, rx, CONTENT_TOP + 86, rw, "Laatste 24u", vTxt(solarValid, String(day.yieldWh/1000.0,1) + " kWh"), COL.text);

  // onder: staafgrafiek PV per uur (start ruim na het label hierboven, geen overlap)
  int gy = CONTENT_TOP + 142, gh = 26;
  wBarGraph(4, 8, gy, 304, gh, day.pvHour, 24, COL.amber, -1, COL.amber);

  if(first){
    int cy = gy + gh + 4;
    wText(8,   cy, 1, COL.muted, "-24u");
    wText(148, cy, 1, COL.muted, "PV per uur");
    wText(294, cy, 1, COL.muted, "nu");
  }
}

#endif
