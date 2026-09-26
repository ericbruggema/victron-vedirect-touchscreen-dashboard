/* ============================================================================
   pages_accu_energie.h  -  PAGE_ACCU (accudetail) en PAGE_ENERGIE (energiestroom)
   ----------------------------------------------------------------------------
   Geen RTC in dit ontwerp: "dag"-statistiek (day.socHour[24] enz.) is een
   ROLLEND 24-uursvenster t.o.v. nu, geen kalenderdag. Daarom hier nergens
   "vandaag" gebruiken, maar "laatste 24u" / "rollend 24u".
============================================================================ */
#ifndef PAGES_ACCU_ENERGIE_H
#define PAGES_ACCU_ENERGIE_H

#include "dashboard.h"
#include "widgets.h"
#include "config.h"

// ---- PAGE_ACCU --------------------------------------------------------------
// Compacte layout: alles heeft een vaste, VERIFIEERDE positie t.o.v. CONTENT_TOP
// zodat niets kan overlappen ongeacht CONTENT_BOTTOM (geen scrollen nodig).
void pageAccuDraw(bool first){
  // -- links: SoC-ring (kleiner dan voorheen, ruimte voor alles eronder) --
  int ringCx = 90, ringCy = CONTENT_TOP + 62, ringR = 50;   // spant y=40..140 (t.o.v. TOP=28: 12..112)
  wBigRing(0, ringCx, ringCy, ringR, 9, socColor(live.soc),
            battValid ? live.soc : 0,
            battValid ? (String(live.soc) + "%") : String("--"),
            "SoC");

  // -- rechts: rijen met kernwaarden --
  int rx = 185, rw = 312 - 185;
  int ry = CONTENT_TOP + 4;      // start
  const int rowStep = 22;

  wRow(1, rx, ry, rw, "Spanning", vTxt(battValid, String(live.battV, 1) + " V"),
       vCol(battValid, COL.text));
  ry += rowStep;

  uint16_t battACol = !battValid ? COL.muted : (live.battA >= 0 ? COL.green : COL.coral);
  wRow(2, rx, ry, rw, "Stroom", vTxt(battValid, String(live.battA, 1) + " A"), battACol);
  ry += rowStep;

  const char* estLabel;
  float hours;
  if(live.battA > 0.05f){
    estLabel = "Tot vol";
    hours = (BATT.capacityAh * (100 - live.soc) / 100.0f) / live.battA;
  } else if(live.battA < -0.05f){
    estLabel = "Resterend";
    hours = live.remainingAh / (-live.battA);
  } else {
    estLabel = "Resterend";
    hours = -1;
  }
  String estVal = (battValid && hours >= 0)
    ? (String((int)hours) + "u " + String((int)((hours - (int)hours) * 60)) + "m")
    : String("--");
  wRow(3, rx, ry, rw, estLabel, estVal, vCol(battValid, COL.text));
  ry += rowStep;

  if(live.battCount == 2){
    uint16_t balCol = (fabsf(live.midDev) < 2.0f) ? COL.green : COL.red;
    wRow(4, rx, ry, rw, "Accu 1/2",
         String(live.batt1V, 1) + "V / " + String(live.batt2V, 1) + "V", balCol);
  } else {
    wRow(4, rx, ry, rw, "Cycli", String(live.cycles), COL.text);
  }
  ry += rowStep;

  // -- status-pil met laadfase -- (ry is nu CONTENT_TOP+4+4*22 = TOP+92)
  ry += 6;
  wStatusPill(5, rx, ry, mpptColor(live.state), COL.bg, mpptText(live.state));
  // pil eindigt rond TOP+92+6+20 = TOP+118 = y=146 - ruim boven de sparkline hieronder

  // -- onderin: sparkline van SoC over het rollende 24u-venster --
  // vaste positie t.o.v. TOP (niet t.o.v. CONTENT_BOTTOM) zodat dit nooit met
  // de rijen/pil hierboven kan overlappen, welke marge CONTENT_BOTTOM ook krijgt
  static float socF[24];
  for(int i = 0; i < 24; i++) socF[i] = (float)day.socHour[i];
  int sx = 8, sy = CONTENT_TOP + 128, sw = 304, sh = 32;   // spant y=156..188
  wSparkline(6, sx, sy, sw, sh, socF, 24, COL.green);

  if(first){
    int capY = sy + sh + 2;   // y=190, ruim binnen CONTENT_BOTTOM
    wText(sx, capY, 1, COL.muted, "-24u");
    wText(150, capY, 1, COL.muted, "SoC (rollend 24u)");
    wText(290 - 20, capY, 1, COL.muted, "nu");
  }
}

// ---- PAGE_ENERGIE -------------------------------------------------------------
void pageEnergieDraw(bool first){
  const int margin = 8, gap = 8;
  const int tileW = (SCR_W - 2*margin - 2*gap) / 3;
  const int tileH = ((CONTENT_BOTTOM - CONTENT_TOP) - 2*margin - gap) / 2;
  const int col1 = margin;
  const int col2 = col1 + tileW + gap;
  const int col3 = col2 + tileW + gap;
  const int row1 = CONTENT_TOP + margin;
  const int row2 = row1 + tileH + gap;

  bool ok = solarValid && battValid;

  int pctDirect = (ok && live.pvW > 0) ? (live.directSolarW * 100 / live.pvW) : 0;
  wStatTile(0, col1, row1, tileW, tileH, "direct zon",
            ok ? (String(pctDirect) + "%") : String("--"), COL.text);

  int pctSelf = (ok && live.loadW > 0) ? (live.directSolarW * 100 / live.loadW) : 0;
  wStatTile(1, col2, row1, tileW, tileH, "zelfconsumptie",
            ok ? (String(pctSelf) + "%") : String("--"), COL.text);

  wStatTile(2, col3, row1, tileW, tileH, "naar accu",
            battValid ? fmtW(live.battW > 0 ? (int)live.battW : 0) : String("--"), COL.green);

  wStatTile(3, col1, row2, tileW, tileH, "uit accu",
            battValid ? fmtW(live.battW < 0 ? (int)(-live.battW) : 0) : String("--"), COL.text);

  String nettoVal = battValid
    ? (String(live.battW >= 0 ? "+" : "") + fmtW((int)live.battW))
    : String("--");
  wStatTile(4, col2, row2, tileW, tileH, "netto accu", nettoVal,
            live.battW >= 0 ? COL.green : COL.coral);

  int net = live.pvW - live.loadW;
  String netVal = ok ? (String(net >= 0 ? "+" : "") + fmtW(net)) : String("--");
  wStatTile(5, col3, row2, tileW, tileH, "zon - verbruik", netVal,
            net >= 0 ? COL.green : COL.coral);
}

#endif
