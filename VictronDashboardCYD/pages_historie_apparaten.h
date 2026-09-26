/* ============================================================================
   pages_historie_apparaten.h  -  PAGE_HISTORIE ("DAG & HISTORIE") en
   PAGE_APPARATEN (MPPT + Shunt apparaatinfo)
   ----------------------------------------------------------------------------
   Geen RTC in dit ontwerp: alle "dag"-statistiek is een ROLLEND 24-uursvenster
   t.o.v. de uptime (geen kalenderdag/-week). day.weekYieldWh[7] is een rollend
   7-slots-buffer (index 6 = meest recente rollover, index 0 = oudste) - dus
   labels als "D-6..D0" i.p.v. weekdagnamen.
============================================================================ */
#ifndef PAGES_HISTORIE_APPARATEN_H
#define PAGES_HISTORIE_APPARATEN_H

#include "dashboard.h"
#include "widgets.h"
#include "config.h"

// ---- PAGE_HISTORIE ----------------------------------------------------------
void pageHistorieDraw(bool first){
  // 1) vier statistiek-tegels naast elkaar
  int margin = 8, gap = 6;
  int tileW = (SCR_W - margin*2 - gap*3) / 4;   // (320-16-18)/4 = 71
  int tileY = CONTENT_TOP + 4;
  int tileH = 54;

  wStatTile(0, margin + 0*(tileW+gap), tileY, tileW, tileH, "opbrengst",
            String(day.yieldWh/1000.0, 1) + " kWh", COL.text);
  wStatTile(1, margin + 1*(tileW+gap), tileY, tileW, tileH, "verbruik",
            String(day.loadWh/1000.0, 1) + " kWh", COL.text);
  wStatTile(2, margin + 2*(tileW+gap), tileY, tileW, tileH, "geladen",
            String(day.chargedWh/1000.0, 1) + " kWh", COL.green);
  wStatTile(3, margin + 3*(tileW+gap), tileY, tileW, tileH, "ontladen",
            String(day.dischargedWh/1000.0, 1) + " kWh", COL.coral);

  // 2) min/max-regel - via wRow (i.p.v. 1 samengeplakte string) zodat het
  // label ("Accu"/"I"/"Piek") los gecached is van de wisselende min/max-waarde
  int infoY = CONTENT_TOP + 66;
  wRow(11, 8,   infoY, 104, "Accu", String(day.battV_min, 1) + "-" + String(day.battV_max, 1) + "V", COL.text);
  wRow(12, 120, infoY, 104, "I",    String(day.battA_min, 1) + ".." + String(day.battA_max, 1) + "A", COL.text);
  wRow(13, 230, infoY, 82,  "Piek", fmtW(day.load_max), COL.text);

  // 3) laadfase-verdeelbalk
  int barY = CONTENT_TOP + 90;
  float total = day.phaseSec[0] + day.phaseSec[1] + day.phaseSec[2];
  float fr[3];
  if(total > 1.0f){
    fr[0] = day.phaseSec[0] / total;
    fr[1] = day.phaseSec[1] / total;
    fr[2] = day.phaseSec[2] / total;
  } else {
    fr[0] = fr[1] = fr[2] = 1.0f/3.0f;
  }
  uint16_t phaseCols[3] = { COL.amber, COL.amberMid, COL.green };
  wSegmentBar(7, 8, barY, 304, 16, fr, 3, phaseCols);

  if(first){
    int labelY = barY + 16 + 4;
    wText(8,   labelY, 1, COL.muted, "bulk");
    wText(140, labelY, 1, COL.muted, "absorptie");
    wText(280, labelY, 1, COL.muted, "float");
  }

  // 4) 7-daagse (rollende) geschiedenis-grafiek
  static float weekKwh[7];
  for(int i = 0; i < 7; i++) weekKwh[i] = day.weekYieldWh[i] / 1000.0f;
  int bestIdx = 0;
  for(int i = 1; i < 7; i++) if(weekKwh[i] > weekKwh[bestIdx]) bestIdx = i;

  // vaste positie t.o.v. TOP (niet t.o.v. CONTENT_BOTTOM) zodat dit nooit met
  // de laadfase-labels hierboven kan overlappen, welke marge CONTENT_BOTTOM ook krijgt
  int graphY = CONTENT_TOP + 126, graphH = 32;
  wBarGraph(9, 8, graphY, 304, graphH, weekKwh, 7, COL.purple, bestIdx, COL.text);

  int capY = graphY + graphH + 4;
  if(first){
    wText(8,   capY, 1, COL.muted, "D-6");
    wText(290, capY, 1, COL.muted, "D0");
  }
  wValueC(10, 160, capY, 1, COL.muted,
          "beste: D-" + String(6 - bestIdx) + " " + String(weekKwh[bestIdx], 1) + " kWh");
}

// ---- PAGE_APPARATEN ----------------------------------------------------------
static String regTxt(int idx){
  if(!veRegs[idx].got) return "...";
  int dec = (idx == 8) ? 2 : 1;
  return String(veRegs[idx].value, dec) + " " + String(veRegs[idx].unit);
}

void pageApparatenDraw(bool first){
  if(first){
    tft.drawFastVLine(160, CONTENT_TOP + 4, CONTENT_BOTTOM - CONTENT_TOP - 8, COL.border);
  }

  // ---- linkerkolom: MPPT ----
  int iconY = CONTENT_TOP + 16;
  if(first){
    iconSun(24, iconY, 10, COL.amber);
    wText(42, iconY - 8, 1, COL.text, "MPPT");
    wText(8,  CONTENT_TOP + 28, 1, COL.muted, MPPT_MODEL);
  }
  wValue(0, 8, CONTENT_TOP + 42, 1, COL.muted, "FW v" + String(mpptInfo.fw / 100.0, 2));

  int rowY = CONTENT_TOP + 62;
  wRow(2, 8, rowY,          144, "max laadstroom", regTxt(0), COL.text);
  wRow(3, 8, rowY + 22,     144, "absorptie",       regTxt(1), COL.text);
  wRow(4, 8, rowY + 44,     144, "float",           regTxt(2), COL.text);

  // ---- rechterkolom: Shunt ----
  if(first){
    iconBattery(184, iconY, 10, COL.green);
    wText(202, iconY - 8, 1, COL.text, "SHUNT");
    wText(168, CONTENT_TOP + 28, 1, COL.muted, SHUNT_MODEL);
  }
  wValue(1, 168, CONTENT_TOP + 42, 1, COL.muted, "FW v" + String(shuntInfo.fw / 100.0, 2));

  wRow(5, 168, rowY,      144, "capaciteit", regTxt(3), COL.text);
  wRow(6, 168, rowY + 22, 144, "charged v",  regTxt(4), COL.text);
  wRow(7, 168, rowY + 44, 144, "peukert",    regTxt(8), COL.text);
}

#endif
