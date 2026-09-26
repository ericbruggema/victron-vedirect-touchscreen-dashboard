/* ============================================================================
   widgets.h  -  Widget-toolkit voor de CYD (TFT_eSPI, cached/diff-redraw)
   ----------------------------------------------------------------------------
   Filosofie ongewijzigd t.o.v. het ESP32-D-ontwerp: elke widget heeft een vast
   cache-slot (id) en tekent zichzelf alleen opnieuw als zijn inhoud écht
   gewijzigd is (cacheChanged). Dat is hier eenvoudiger dan de oude teken-diff
   omdat TFT_eSPI tekst OPAAK kan tekenen (col, bgCol) - de font-cel wist zich
   dus zelf. Enige uitzondering: bij een KORTERE nieuwe waarde blijft een staart
   van de oude tekst staan, dus dat reststuk wissen we expliciet.

   Alle pagina's DELEN dezelfde cache[] (64 slots). Dat mag: bij elke
   paginawisseling wordt de cache volledig gewist (resetWidgetCache()), dus
   elke pagina mag zijn eigen ids 0..N hergebruiken zonder overlap met een
   andere pagina. Reserveer ids 60-63 NIET in pagina's - die zijn voor de
   paginachrome (statusbalk/subheader/navstipjes).

   Tekst-"stijlen" (fontSize-parameter van wValue/wValueR/wValueC/wText):
     1 = klein   (font 2,  ~16px hoog) - labels, subtekst
     2 = middel  (font 4,  ~26px hoog) - normale waarden
     3 = groot   (font 4, size x2, ~52px hoog) - blikvangers (SoC, vermogen)
============================================================================ */
#ifndef WIDGETS_H
#define WIDGETS_H
#include "dashboard.h"

// 0-59 = pagina-eigen ids, 60-64 = chrome (zie widgets.h onderin), 100-149 =
// interne wRow label/waarde-splitsing (100+id*2 / 101+id*2), 150-199 =
// interne wStatTile label/waarde-splitsing (150+id*2 / 151+id*2). Nieuwe
// widgets met eenzelfde label/waarde-opsplitsing: kies een volgende vrije band.
#define NCACHE 200
static String widgetCache[NCACHE];

void resetWidgetCache(){ for(int i = 0; i < NCACHE; i++) widgetCache[i] = ""; }

static bool cacheChanged(int id, const String &key){
  if(widgetCache[id] == key) return false;
  widgetCache[id] = key;
  return true;
}
// zoals cacheChanged, maar geeft ook de VORIGE waarde terug (voor tekst-opruiming)
static bool cacheChangedPrev(int id, const String &key, String &prevOut){
  prevOut = widgetCache[id];
  if(prevOut == key) return false;
  widgetCache[id] = key;
  return true;
}

ColorSet COL;
void colorsInit(){
  COL.bg       = tft.color565(10, 13, 16);
  COL.card     = tft.color565(20, 24, 29);
  COL.border   = tft.color565(38, 44, 51);
  COL.text     = tft.color565(232, 236, 239);
  COL.muted    = tft.color565(139, 149, 161);
  COL.amber    = tft.color565(245, 166, 35);
  COL.green    = tft.color565(46, 204, 143);
  COL.coral    = tft.color565(255, 107, 74);
  COL.amberMid = tft.color565(240, 168, 63);
  COL.red      = tft.color565(255, 77, 77);
  COL.blue     = tft.color565(77, 163, 255);
  COL.purple   = tft.color565(127, 119, 221);
}

// ---- AANRAAKZONES -----------------------------------------------------------
#define MAX_TAPZONES 8
struct TapZoneRec { int16_t x, y, w, h; PageId target; };
static TapZoneRec tapZones[MAX_TAPZONES];
static int tapZoneCount = 0;
void clearTapZones(){ tapZoneCount = 0; }
void addTapZone(int x, int y, int w, int h, PageId target){
  if(tapZoneCount >= MAX_TAPZONES) return;
  tapZones[tapZoneCount++] = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, target };
}
PageId hitTestZones(int x, int y){
  for(int i = 0; i < tapZoneCount; i++){
    TapZoneRec &z = tapZones[i];
    if(x >= z.x && x < z.x + z.w && y >= z.y && y < z.y + z.h) return z.target;
  }
  return PAGE_NONE;
}

// ---- TEKST-STIJLEN ------------------------------------------------------------
static void applyTextStyle(uint8_t style){
  if(style == 1)      { tft.setTextFont(2); tft.setTextSize(1); }
  else if(style == 2) { tft.setTextFont(4); tft.setTextSize(1); }
  else                 { tft.setTextFont(4); tft.setTextSize(2); }
}
static int styleHeight(uint8_t style){ return style == 1 ? 16 : style == 2 ? 26 : 52; }

// ---- GECACHTE TEKST (links/rechts/midden uitgelijnd) ------------------------
void drawCachedText(int id, int x, int y, uint8_t style, uint16_t col, const String &val, uint8_t datum){
  String key = String(style) + "|" + String(col) + "|" + val;
  String prev;
  if(!cacheChangedPrev(id, key, prev)) return;

  // vorige tekst/kleur/stijl uit de cache-sleutel halen om de "staart" te kunnen wissen
  int p1 = prev.indexOf('|'); int p2 = prev.indexOf('|', p1 + 1);
  String prevVal = (p2 >= 0) ? prev.substring(p2 + 1) : "";

  applyTextStyle(style);
  int newW = tft.textWidth(val);
  int prevW = prevVal.length() ? tft.textWidth(prevVal) : 0;
  int h = styleHeight(style);

  if(datum == TC_DATUM || datum == MC_DATUM){
    if(prevW > newW){
      int cw = max(prevW, newW);
      tft.fillRect(x - cw/2 - 2, y - (datum==MC_DATUM?h/2:0) - 1, cw + 4, h + 2, COL.bg);
    }
  } else if(datum == TR_DATUM || datum == MR_DATUM){
    if(prevW > newW) tft.fillRect(x - prevW - 2, y - (datum==MR_DATUM?h/2:0) - 1, prevW - newW + 2, h + 2, COL.bg);
  } else {
    if(prevW > newW) tft.fillRect(x + newW, y - (datum==ML_DATUM?h/2:0) - 1, prevW - newW + 2, h + 2, COL.bg);
  }

  tft.setTextColor(col, COL.bg);
  tft.setTextDatum(datum);
  tft.drawString(val, x, y);
}
void wValue (int id, int x,  int y, uint8_t style, uint16_t col, const String &val){ drawCachedText(id, x,  y, style, col, val, TL_DATUM); }
void wValueR(int id, int xr, int y, uint8_t style, uint16_t col, const String &val){ drawCachedText(id, xr, y, style, col, val, TR_DATUM); }
void wValueC(int id, int cx, int y, uint8_t style, uint16_t col, const String &val){ drawCachedText(id, cx, y, style, col, val, TC_DATUM); }

void wText(int x, int y, uint8_t style, uint16_t col, const String &s){
  applyTextStyle(style);
  tft.setTextColor(col, COL.bg);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(s, x, y);
}
void wDivider(int x, int y, int w){ tft.drawFastHLine(x, y, w, COL.border); }

// ---- ICONEN (eenvoudige vectorfiguren) --------------------------------------
void iconSun(int cx, int cy, int r, uint16_t col){
  tft.fillCircle(cx, cy, (int)(r*0.55f), col);
  for(int a = 0; a < 360; a += 45){
    float rad = a * DEG_TO_RAD;
    int x1 = cx + cos(rad)*r*0.78f, y1 = cy + sin(rad)*r*0.78f;
    int x2 = cx + cos(rad)*r,       y2 = cy + sin(rad)*r;
    tft.drawLine(x1, y1, x2, y2, col);
  }
}
void iconMoon(int cx, int cy, int r, uint16_t col){
  tft.fillCircle(cx, cy, (int)(r*0.8f), col);
}
void iconBattery(int cx, int cy, int r, uint16_t col){
  int w = (int)(r*1.6f), h = (int)(r*1.1f);
  int x = cx - w/2, y = cy - h/2;
  tft.drawRoundRect(x, y, w, h, 2, col);
  tft.drawRoundRect(x+1, y+1, w-2, h-2, 2, col);
  tft.fillRect(x + w, y + h/4, (int)(r*0.18f)+1, h/2, col);
}
void iconBolt(int cx, int cy, int r, uint16_t col){
  tft.fillTriangle(cx+(int)(r*0.15f), cy-r, cx-(int)(r*0.5f), cy+(int)(r*0.15f), cx+(int)(r*0.05f), cy+(int)(r*0.15f), col);
  tft.fillTriangle(cx-(int)(r*0.15f), cy+r, cx+(int)(r*0.5f), cy-(int)(r*0.15f), cx-(int)(r*0.05f), cy-(int)(r*0.15f), col);
}
void iconArrowRight(int cx, int cy, int r, uint16_t col){
  tft.drawLine(cx-r, cy, cx+(int)(r*0.4f), cy, col);
  tft.drawLine(cx-r, cy+1, cx+(int)(r*0.4f), cy+1, col);
  tft.fillTriangle(cx+(int)(r*0.4f), cy-(int)(r*0.5f), cx+(int)(r*0.4f), cy+(int)(r*0.5f), cx+r, cy, col);
}
void iconChevronRight(int cx, int cy, int r, uint16_t col){
  tft.drawLine(cx-(int)(r*0.3f), cy-r, cx+(int)(r*0.5f), cy, col);
  tft.drawLine(cx+(int)(r*0.5f), cy,   cx-(int)(r*0.3f), cy+r, col);
  tft.drawLine(cx-(int)(r*0.3f)+1, cy-r, cx+(int)(r*0.5f)+1, cy, col);
  tft.drawLine(cx+(int)(r*0.5f)+1, cy,   cx-(int)(r*0.3f)+1, cy+r, col);
}
void iconChevronLeft(int cx, int cy, int r, uint16_t col){
  tft.drawLine(cx+(int)(r*0.3f), cy-r, cx-(int)(r*0.5f), cy, col);
  tft.drawLine(cx-(int)(r*0.5f), cy,   cx+(int)(r*0.3f), cy+r, col);
  tft.drawLine(cx+(int)(r*0.3f)-1, cy-r, cx-(int)(r*0.5f)-1, cy, col);
  tft.drawLine(cx-(int)(r*0.5f)-1, cy,   cx+(int)(r*0.3f)-1, cy+r, col);
}
void iconGrid(int cx, int cy, int r, uint16_t col){
  int s = max(2, r/3);
  for(int i = -1; i <= 1; i += 2)
    for(int j = -1; j <= 1; j += 2)
      tft.fillRect(cx + i*r/2 - s/2, cy + j*r/2 - s/2, s, s, col);
}
void iconClock(int cx, int cy, int r, uint16_t col){
  tft.drawCircle(cx, cy, r, col);
  tft.drawLine(cx, cy, cx, cy-(int)(r*0.6f), col);
  tft.drawLine(cx, cy, cx+(int)(r*0.5f), cy, col);
}
void iconHouse(int cx, int cy, int r, uint16_t col){
  int w = (int)(r*1.5f), h = (int)(r*0.9f);
  int x = cx - w/2, y = cy - h/2 + (int)(r*0.2f);
  tft.fillRect(x, y, w, h, col);
  tft.fillTriangle(cx, y-(int)(r*0.85f), x-3, y+2, x+w+3, y+2, col);
}

// ---- DASHBOARD-KAART ----------------------------------------------------------
void wCard(int id, int x, int y, int w, int h, const CardData &d){
  // Opgesplitst in 4 los-gecachte delen (achtergrond / icoon+ring / label / tekst)
  // i.p.v. 1 gezamenlijke sleutel: anders duwt elke kleine wiggel in bv. de
  // spanning (subValue) het hele icoon+ring+label mee de hertekening in,
  // terwijl dat er zichtbaar he-le-maal niet anders uitziet. Het LABEL
  // ("ZON"/"ACCU"/"VERBRUIK") verandert voor een gegeven kaart NOOIT, dus die
  // krijgt een eigen sleutel die letterlijk nooit wijzigt -> wordt precies 1x
  // getekend en daarna nooit meer aangeraakt. Sub-ids = id*10+0/1/2/3, dus
  // voor 3 kaarten (id 0,1,2) gebruikt dit 0-23 - ruim binnen de 0-59 ruimte.
  int bgId = id*10, ringId = id*10+1, labelId = id*10+3, textId = id*10+2;

  int ringR = constrain(min(w, h)/2 - 26, 24, 40);
  int cx = x + w/2;

  // Verschuivingen (pixels) - zie opmerking van de gebruiker:
  //  - icoon/ring: iconShift omhoog
  //  - label: volgt het icoon (zelfde afstand tot het icoon als voorheen)
  //  - grote waarde + V&A-regel: valueShift (= iconShift-2) omhoog, dus een
  //    IETS grotere afstand tot het icoon dan voorheen, maar ONDERLING (grote
  //    waarde <-> V&A) exact dezelfde afstand als nu
  //  - streep + piekwaarden: iconShift omhoog (zelfde als het icoon)
  const int iconShift  = 4;
  const int valueShift = iconShift - 2;

  int cy = y + 14 + ringR - iconShift;                 // icoon/ring
  int textAnchor = (y + 14 + ringR) + ringR + 2;        // referentie = oude (ongewijzigde) textTop
  int labelY = textAnchor + 2  - iconShift;             // label volgt het icoon
  int bigY   = textAnchor + 16 - valueShift + 3 + 4;    // grote waarde: nog eens 4px lager
  int subY   = textAnchor + 42 - valueShift + 3 + 2 + 4;// V&A: 4px lager, zelfde afstand tot de waarde erboven
  int textBottom = y + h;
  int fy = textBottom - 24 - iconShift - 3 - 3 - 5;     // streep + voettekst: nog eens 5px hoger

  // achtergrond/kader: puur decoratief, hangt nooit van live data af -> 1x
  if(cacheChanged(bgId, String("bg"))){
    tft.fillRoundRect(x, y, w, h, 10, COL.card);
    tft.drawRoundRect(x, y, w, h, 10, COL.border);
  }

  // icoon + ring: alleen opnieuw tekenen als vorm/kleur/percentage ECHT wijzigt
  String ringKey = String((uint32_t)(uintptr_t)d.icon) + "|" + String(d.accent) + "|" + String(d.ringPct);
  if(cacheChanged(ringId, ringKey)){
    tft.fillCircle(cx, cy, ringR+2, COL.card);   // wist alleen de ronde plek, niet de hele kaart
    if(d.ringPct >= 0){
      tft.drawArc(cx, cy, ringR, ringR-6, 0, 360, COL.border, COL.card, true);
      if(d.ringPct > 0) tft.drawArc(cx, cy, ringR, ringR-6, 0, (uint32_t)(3.6f*d.ringPct), d.accent, COL.card, true);
    }
    if(d.icon) d.icon(cx, cy, ringR-14, d.accent);
  }

  // label ("ZON"/"ACCU"/"VERBRUIK"): sleutel is de tekst zelf, en die
  // verandert voor deze kaart nooit -> na de eerste keer wordt dit nooit meer
  // aangeraakt, ook niet als de tekst hieronder om de seconde wisselt.
  if(cacheChanged(labelId, String(d.label))){
    tft.fillRect(x+2, labelY-3, w-4, 18, COL.card);
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(COL.muted, COL.card);
    tft.drawString(d.label, cx, labelY);
  }

  // tekst: grote waarde / V&A / voettekst - los van het label hierboven, dus
  // een wiggelende spanning/stroom raakt het label nooit meer aan. clearTop
  // moet ONDER de volledige tekst-hoogte van het label liggen (font2 is ~16px
  // hoog) - stond eerder op labelY+12, dus 12px onder de TOP van het label
  // i.p.v. onder de ONDERKANT ervan, waardoor elke waarde-update de onderste
  // paar pixels van "ZON"/"ACCU"/"VERBRUIK" wegveegde en nooit terugtekende.
  String textKey = d.bigValue + "|" + d.subValue + "|" + String(d.footLabel) + "|" + d.footValue;
  if(cacheChanged(textId, textKey)){
    int clearTop = labelY + 16;   // ruim onder de onderkant van het label
    tft.fillRect(x+2, clearTop, w-4, textBottom-clearTop, COL.card);

    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(4); tft.setTextSize(1); tft.setTextColor(COL.text, COL.card);
    tft.drawString(d.bigValue, cx, bigY);

    tft.setTextFont(2); tft.setTextColor(COL.muted, COL.card);
    tft.drawString(d.subValue, cx, subY);

    // voettekst: label en waarde op TWEE regels (i.p.v. samengeplakt op 1 regel,
    // wat op een smalle 94px-kaart al snel breder werd dan de kaart zelf)
    tft.drawFastHLine(x+10, fy-4, w-20, COL.border);
    tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(COL.muted, COL.card);
    tft.drawString(d.footLabel, cx, fy+2);
    tft.setTextFont(2); tft.setTextColor(COL.text, COL.card);
    tft.drawString(d.footValue, cx, fy+12);
  }
}

// ---- STATISTIEK-TEGEL / RIJ -----------------------------------------------
void wStatTile(int id, int x, int y, int w, int h, const char* label, const String &value, uint16_t valueColor){
  // rand+label los gecached van de waarde (zie NCACHE-band 150-199): het
  // label ("opbrengst", "direct zon", ...) verandert voor een gegeven tegel
  // NOOIT, dus wordt na de eerste tekenbeurt nooit meer aangeraakt.
  int labelId = 150 + id*2, valueId = 151 + id*2;

  if(cacheChanged(labelId, String(label))){
    tft.fillRoundRect(x, y, w, h, 8, COL.card);
    tft.drawRoundRect(x, y, w, h, 8, COL.border);
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(COL.muted, COL.card);
    tft.drawString(label, x+w/2, y+6);
  }

  String valKey = value + "|" + String(valueColor);
  if(cacheChanged(valueId, valKey)){
    // waarde: gebruik het grote font alleen als het ook echt past in de tegel,
    // anders het kleine font - voorkomt te grote/overlopende cijfers in smalle
    // tegels (bv. "10.0 kWh" op de DAG & HISTORIE-pagina)
    tft.setTextFont(4); tft.setTextSize(1);
    int vw4 = tft.textWidth(value);
    uint8_t valFont = (vw4 > w - 10) ? 2 : 4;

    int valTop = y + 22;   // net onder het label, dat blijft dus altijd ongemoeid
    tft.fillRect(x+2, valTop, w-4, (y+h)-valTop-2, COL.card);

    tft.setTextFont(valFont);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(valueColor, COL.card);
    tft.drawString(value, x+w/2, y+h/2+10);
  }
}
void wRow(int id, int x, int y, int w, const char* label, const String &value, uint16_t valueColor){
  // label en waarde los gecached (zie NCACHE-band 100-149): het label
  // ("Spanning", "uptime", "Charged V", ...) verandert voor een gegeven rij
  // NOOIT, dus zodra het 1x getekend is wordt het nooit meer aangeraakt - ook
  // niet als de waarde ernaast elke seconde verandert.
  int labelId = 100 + id*2, valueId = 101 + id*2;
  tft.setTextFont(2); tft.setTextSize(1);
  int labelW = tft.textWidth(label);

  if(cacheChanged(labelId, String(label))){
    tft.fillRect(x, y, w, 18, COL.bg);   // 1x de hele rij-achtergrond wissen
    tft.setTextDatum(TL_DATUM); tft.setTextColor(COL.muted, COL.bg);
    tft.drawString(label, x, y+2);
  }

  String valKey = value + "|" + String(valueColor);
  if(cacheChanged(valueId, valKey)){
    int valueW = tft.textWidth(value);
    // label+waarde samen te breed voor de rij (bv. "bron: MPPT+SHUNT live" op
    // een smalle kolom) -> waarde in het kleinere font i.p.v. over het label heen
    uint8_t valFont = (labelW + valueW + 8 > w) ? 1 : 2;

    // wis alleen het WAARDE-gedeelte (rechts van het label), nooit het label zelf
    tft.fillRect(x + labelW + 4, y, w - labelW - 4, 18, COL.bg);

    tft.setTextFont(valFont); tft.setTextSize(1);
    tft.setTextDatum(TR_DATUM); tft.setTextColor(valueColor, COL.bg);
    tft.drawString(value, x+w, valFont==1 ? y+5 : y+2);
  }
}

// ---- GROTE RING-GAUGE (bv. SoC op ACCU-detail) ------------------------------
void wBigRing(int id, int cx, int cy, int r, int strokeW, uint16_t col, int pct, const String &centerBig, const String &centerSmall){
  String key = String(col) + "|" + String(pct) + "|" + centerBig + "|" + centerSmall;
  if(!cacheChanged(id, key)) return;
  tft.drawArc(cx, cy, r, r-strokeW, 0, 360, COL.border, COL.bg, true);
  if(pct > 0) tft.drawArc(cx, cy, r, r-strokeW, 0, (uint32_t)(3.6f*pct), col, COL.bg, true);
  tft.fillCircle(cx, cy, r-strokeW-2, COL.bg);   // binnenkant leeg voor de tekst
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4); tft.setTextSize(1); tft.setTextColor(COL.text, COL.bg);
  tft.drawString(centerBig, cx, cy-8);
  tft.setTextFont(2); tft.setTextColor(COL.muted, COL.bg);
  tft.drawString(centerSmall, cx, cy+16);
}

// ---- GRAFIEKEN ----------------------------------------------------------------
void wBarGraph(int id, int x, int y, int w, int h, const float* values, int n, uint16_t color, int highlightIdx, uint16_t highlightColor){
  String key = String(n) + ":" + String(highlightIdx);
  for(int i = 0; i < n; i++) key += ":" + String((int)(values[i]*10));
  if(!cacheChanged(id, key)) return;
  tft.fillRect(x, y, w, h, COL.bg);
  float mx = 0.001f; for(int i = 0; i < n; i++) if(values[i] > mx) mx = values[i];
  int gap = 3; int bw = (w - gap*(n-1)) / n; if(bw < 1) bw = 1;
  for(int i = 0; i < n; i++){
    int bh = (int)(values[i] / mx * (h - 2));
    if(bh < 1 && values[i] > 0) bh = 1;
    if(bh <= 0) continue;
    uint16_t c = (i == highlightIdx) ? highlightColor : color;
    tft.fillRect(x + i*(bw+gap), y + h - bh, bw, bh, c);
  }
}
void wSparkline(int id, int x, int y, int w, int h, const float* values, int n, uint16_t color){
  String key = String(n); for(int i = 0; i < n; i++) key += ":" + String((int)values[i]);
  if(!cacheChanged(id, key)) return;
  tft.fillRect(x, y, w, h, COL.bg);
  if(n < 2) return;
  int px = -1, py = -1;
  for(int i = 0; i < n; i++){
    int vx = x + (int)((float)i/(n-1) * (w-1));
    float v = constrain(values[i], 0.0f, 100.0f);
    int vy = y + h - 1 - (int)(v/100.0f * (h-2));
    if(px >= 0) tft.drawLine(px, py, vx, vy, color);
    px = vx; py = vy;
  }
}
void wSegmentBar(int id, int x, int y, int w, int h, const float* fractions, int n, const uint16_t* colors){
  String key = String(n); for(int i = 0; i < n; i++) key += ":" + String((int)(fractions[i]*100));
  if(!cacheChanged(id, key)) return;
  int cx = x;
  for(int i = 0; i < n; i++){
    int seg = (i == n-1) ? (x + w - cx) : (int)(fractions[i] * w);
    if(seg > 0) tft.fillRect(cx, y, seg, h, colors[i]);
    cx += seg;
  }
}

// ---- STATUS-PIL / INSTELLINGEN-WIDGETS -------------------------------------
void wStatusPill(int id, int x, int y, uint16_t bg, uint16_t textCol, const char* txt){
  String key = String(bg) + txt;
  if(!cacheChanged(id, key)) return;
  tft.setTextFont(2); tft.setTextSize(1);
  int w = tft.textWidth(txt) + 20;
  tft.fillRoundRect(x, y, w, 20, 10, bg);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(textCol, bg);
  tft.drawString(txt, x+w/2, y+10);
}
void wToggle(int id, int x, int y, bool on){
  String key = on ? "1" : "0";
  if(!cacheChanged(id, key)) return;
  int w = 34, h = 18;
  tft.fillRoundRect(x, y, w, h, h/2, on ? COL.green : COL.border);
  int kr = h/2 - 2;
  int kx = on ? (x + w - h + 2 + kr) : (x + 2 + kr);
  tft.fillCircle(kx, y+h/2, kr, COL.bg);
}
void wSlider(int id, int x, int y, int w, int pct){
  String key = String(pct);
  if(!cacheChanged(id, key)) return;
  int h = 6;
  tft.fillRoundRect(x, y, w, h, 3, COL.border);
  int fw = w * constrain(pct,0,100) / 100;
  if(fw > 0) tft.fillRoundRect(x, y, fw, h, 3, COL.blue);
  tft.fillCircle(x+fw, y+h/2, 8, COL.text);
}
void wSegmented(int id, int x, int y, const char* optA, const char* optB, int selected){
  String key = String(selected);
  if(!cacheChanged(id, key)) return;
  tft.setTextFont(2); tft.setTextSize(1);
  int wA = tft.textWidth(optA) + 24, wB = tft.textWidth(optB) + 24;
  int h = 24;
  tft.fillRoundRect(x, y, wA+wB, h, 6, COL.card);
  tft.fillRoundRect(selected == 0 ? x : x+wA, y, selected == 0 ? wA : wB, h, 6, COL.blue);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(selected == 0 ? COL.bg : COL.muted, selected == 0 ? COL.blue : COL.card);
  tft.drawString(optA, x+wA/2, y+h/2);
  tft.setTextColor(selected == 1 ? COL.bg : COL.muted, selected == 1 ? COL.blue : COL.card);
  tft.drawString(optB, x+wA+wB/2, y+h/2);
}

// ---- PAGINACHROME (ids 60-64, gereserveerd) --------------------------------
// drawStatusBar is in 3 los-gecachte stukken opgesplitst i.p.v. 1 gezamenlijke
// sleutel: anders zorgt de klok (die elke seconde verandert) ervoor dat de
// HELE balk - lijn, grid-icoon, MPPT/SHUNT-stippen - elke seconde overbodig
// opnieuw getekend wordt, terwijl alleen de tijd zelf hoeft te veranderen.
void drawStatusBar(){
  // statische achtergrond/lijn/grid-icoon: verandert nooit zolang deze pagina
  // actief is, dus na de eerste tekenbeurt (cache leeg na paginawissel) nooit meer.
  if(cacheChanged(60, String("static"))){
    tft.fillRect(0, 0, SCR_W, 28, COL.bg);
    tft.drawFastHLine(0, 27, SCR_W, COL.border);
    iconGrid(SCR_W-16, 14, 7, COL.muted);
  }

  // klok: tikt elke seconde, raakt ALLEEN zijn eigen kleine tekstregio aan
  String tkey = fmtUptime();
  if(cacheChanged(63, tkey)){
    tft.fillRect(0, 3, 100, 22, COL.bg);
    iconClock(14, 14, 7, COL.muted);
    tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(COL.muted, COL.bg);
    tft.setTextDatum(ML_DATUM); tft.drawString(tkey, 26, 14);
  }

  // MPPT/SHUNT-stippen: alleen bijwerken als de status-van-de-bron echt wijzigt.
  // Posities worden nu vanaf rechts UITGEREKEND op basis van de echte tekst-
  // breedte (i.p.v. vaste offsets die uitgingen van een te smalle "MPPT"/
  // "SHUNT" breedte) - anders viel de stip van het volgende label binnen de
  // tekst van het vorige (stip vóór SHUNT werd overlapt door het woord zelf).
  String dkey = String(solarValid) + String(battValid);
  if(cacheChanged(64, dkey)){
    int rightLimit = SCR_W - 16 - 14;   // net voor het grid-icoon
    tft.fillRect(90, 3, rightLimit - 90 + 2, 22, COL.bg);

    tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(COL.muted, COL.bg);
    tft.setTextDatum(MR_DATUM);

    int xr = rightLimit;
    tft.drawString("MPPT", xr, 14);
    int mpptDotX = xr - tft.textWidth("MPPT") - 10;
    tft.fillCircle(mpptDotX, 14, 3, solarValid ? COL.green : COL.border);

    xr = mpptDotX - 12;
    tft.drawString("SHUNT", xr, 14);
    int shuntDotX = xr - tft.textWidth("SHUNT") - 10;
    tft.fillCircle(shuntDotX, 14, 3, battValid ? COL.green : COL.border);
  }
}
void drawSubHeader(const char* title){
  String key = String(title);
  if(!cacheChanged(61, key)) return;
  tft.fillRect(0, 0, SCR_W, 28, COL.bg);
  tft.drawFastHLine(0, 27, SCR_W, COL.border);
  iconChevronLeft(16, 14, 6, COL.muted);

  bool showGrid = (strcmp(title, "INSTELLINGEN") != 0 && strcmp(title, "PAGINA'S") != 0);
  int rightLimit = showGrid ? (SCR_W - 16 - 14) : (SCR_W - 8);   // ruimte tot vóór het grid-icoon

  tft.setTextFont(2); tft.setTextSize(1);
  int tw = tft.textWidth(title);
  uint8_t titleFont = (30 + tw > rightLimit) ? 1 : 2;   // te lang voor de ruimte -> kleiner font
  tft.setTextFont(titleFont);
  tft.setTextColor(COL.text, COL.bg);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(title, 30, 14);

  // grid-icoon ALTIJD als laatste tekenen, zodat een (te) lange titel het nooit
  // kan overlappen/verbergen
  if(showGrid) iconGrid(SCR_W-16, 14, 7, COL.muted);
}
void drawNavDots(int activeIndex){
  String key = String(activeIndex);
  if(!cacheChanged(62, key)) return;
  int n = PAGE_COUNT, spacing = 14;
  int startX = SCR_W/2 - ((n-1)*spacing)/2;
  // gebonden aan CONTENT_BOTTOM (niet aan SCR_H) zodat het footer-gebied altijd
  // precies aansluit op waar de pagina-inhoud ophoudt, ook als de marge verandert
  int y = CONTENT_BOTTOM + (SCR_H - CONTENT_BOTTOM) / 2;
  tft.fillRect(0, CONTENT_BOTTOM, SCR_W, SCR_H - CONTENT_BOTTOM, COL.bg);
  for(int i = 0; i < n; i++)
    tft.fillCircle(startX + i*spacing, y, 3, i == activeIndex ? COL.text : COL.border);
}

#endif
