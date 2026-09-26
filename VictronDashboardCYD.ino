/* ============================================================================
   VictronDashboardCYD.ino  -  FRAMEWORK (ESP32-2432S028R / "CYD", touch-versie)
   ----------------------------------------------------------------------------
   Poort van het ESP32-D/ST7735-project naar een 320x240 ILI9341 met resistief
   XPT2046-touchscreen. PIR/knop/RTC/thema's zijn vervangen door: tik-navigatie,
   swipe links/rechts tussen 7 pagina's, een rollend 24u-statistiekvenster
   (geen kalenderklok) en één vast "detailed" ontwerp.

   BELANGRIJK (nog niet op hardware getest - hier geen Arduino-toolchain
   beschikbaar om te compileren): controleer bij de eerste build vooral de
   TFT_eSPI::drawArc()-signatuur en de User_Setup-pinnen tegen je geïnstalleerde
   library-versie. Zie README_CYD.md voor de installatiestappen.

   Bestanden: dashboard.h, config.h, touch.h, widgets.h, ve_direct.h,
              pages_dashboard.h, pages_accu_energie.h,
              pages_historie_apparaten.h, pages_systeem_settings.h
============================================================================ */
#include <SPI.h>
#include <Preferences.h>
#include <SD.h>
#include "dashboard.h"
#include "config.h"
#include "touch.h"
#include "widgets.h"
#include "ve_direct.h"
#include "pages_dashboard.h"
#include "pages_accu_energie.h"
#include "pages_historie_apparaten.h"
#include "pages_systeem_settings.h"

// ---- HARDWARE + GLOBALS ----------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

Live    live;
Day     day;
Records rec;
bool    nvsOk = false, nvsFull = false;
int     demoGainPct = 0;
DataSource dataSource = SRC_DEMO;
int     socTrend = 0;
int     simHour = 0; float simHourF = 0.0f;
bool    solarValid = true, battValid = true;
int     systemV = 12; float vMult = 1.0;
bool    sdPresent = false;

AppSettings appSettings = { 80, SCREENOFF_DEFAULT, BATTERY_DEFAULT_IDX };

Preferences prefs;

// ---- PAGINAREGISTER (implementaties in pages_*.h) --------------------------
PageDef PAGES[PAGE_COUNT] = {
  { "DASHBOARD",       pageDashboardDraw, nullptr },
  { "ZON DETAILS",     pageZonDraw,       nullptr },
  { "ACCU DETAILS",    pageAccuDraw,      nullptr },
  { "ENERGIE",         pageEnergieDraw,   nullptr },
  { "DAG & HISTORIE",  pageHistorieDraw,  nullptr },
  { "APPARATEN",       pageApparatenDraw, nullptr },
  { "SYSTEEM",         pageSysteemDraw,   nullptr },
};
PageDef SETTINGS_PAGE = { "INSTELLINGEN", pageSettingsDraw, pageSettingsTouch };
PageDef PICKER_PAGE   = { "PAGINA'S",     pagePickerDraw,   nullptr };

// ---- KLEINE HULPFUNCTIES ----------------------------------------------------
String fmtW(int w){
  int a = w < 0 ? -w : w;
  if(a >= 1000){ char b[14]; snprintf(b, sizeof(b), "%.2fkW", w/1000.0); return String(b); }
  return String(w) + "W";
}
String fmtUptime(){
  unsigned long s = millis()/1000;
  int h = s/3600, m = (s%3600)/60, ss = s%60;
  char b[10]; snprintf(b, sizeof(b), "%02d:%02d:%02d", h, m, ss);
  return String(b);
}
const char* trendArrow(){ return socTrend > 0 ? "^" : socTrend < 0 ? "v" : ""; }
uint16_t vCol(bool valid, uint16_t c){ return valid ? c : COL.muted; }
String   vTxt(bool valid, const String &s){ return valid ? s : String("--"); }

uint16_t socColor(int s){
  if(s <= BATT.socAlarm) return COL.red;
  if(s <= BATT.socWarn)  return COL.amberMid;
  return COL.green;
}
uint16_t loadColor(int w){
  if(w < LOAD_GREEN_W)  return COL.green;
  if(w < LOAD_YELLOW_W) return COL.amber;
  if(w < LOAD_ORANGE_W) return COL.amberMid;
  return COL.red;
}
uint16_t flowColor(float w){ return w > 5 ? COL.green : w < -5 ? COL.coral : COL.muted; }
uint16_t mpptColor(ChargeState s){
  switch(s){ case CS_BULK: return COL.amber; case CS_ABSORPTION: return COL.amberMid;
             case CS_FLOAT: return COL.green; default: return COL.muted; }
}
const char* mpptText(ChargeState s){
  switch(s){ case CS_BULK: return "BULK"; case CS_ABSORPTION: return "ABSORPTIE";
             case CS_FLOAT: return "FLOAT"; default: return "UIT"; }
}
AlertLevel systemAlertLevel(const char** reasonOut){
  if(!battValid){ if(reasonOut) *reasonOut = ""; return ALERT_OK; }
  if(live.soc <= BATT.socAlarm)          { if(reasonOut) *reasonOut = "SoC KRITIEK"; return ALERT_ALARM; }
  if(live.battV <= BATT.vAlarmLow*vMult) { if(reasonOut) *reasonOut = "ACCU LAAG";   return ALERT_ALARM; }
  if(live.soc <= BATT.socWarn)           { if(reasonOut) *reasonOut = "SoC laag";    return ALERT_WARN; }
  if(reasonOut) *reasonOut = ""; return ALERT_OK;
}

// ---- NVS: RECORDS, ROLLENDE HISTORIE, INSTELLINGEN -------------------------
void nvsInit(){ nvsOk = prefs.begin("victron", false); }
void recordsInit(){
  rec.maxPvW=0; rec.maxLoadW=0; rec.maxChargeW=0; rec.maxDischargeW=0;
  rec.maxChargeA=0; rec.maxDischargeA=0; rec.maxBattV=0; rec.minBattV=99;
  rec.maxSoc=0; rec.minSoc=100; rec.bestDayWh=0; rec.maxDayLoadWh=0;
  rec.totalChargedWh=0; rec.totalDischargedWh=0; rec.lifetimeDischargedAh=0;
}
void loadRecords(){
  if(!nvsOk) return;
  rec.maxPvW=prefs.getInt("mPvW",rec.maxPvW); rec.maxLoadW=prefs.getInt("mLoadW",rec.maxLoadW);
  rec.maxChargeW=prefs.getInt("mChgW",rec.maxChargeW); rec.maxDischargeW=prefs.getInt("mDisW",rec.maxDischargeW);
  rec.maxBattV=prefs.getFloat("mBatV",rec.maxBattV); rec.minBattV=prefs.getFloat("nBatV",rec.minBattV);
  rec.maxSoc=prefs.getInt("mSoc",rec.maxSoc); rec.minSoc=prefs.getInt("nSoc",rec.minSoc);
  rec.bestDayWh=prefs.getFloat("bDay",rec.bestDayWh);
  rec.totalChargedWh=prefs.getFloat("tChg",rec.totalChargedWh);
  rec.totalDischargedWh=prefs.getFloat("tDis",rec.totalDischargedWh);
  rec.lifetimeDischargedAh=prefs.getFloat("life",rec.lifetimeDischargedAh);
  prefs.getBytes("week", day.weekYieldWh, sizeof(day.weekYieldWh));
  day.daysLogged   = prefs.getInt("dlog", day.daysLogged);
  day.monthYieldWh = prefs.getFloat("mon", day.monthYieldWh);
  day.monthDays    = prefs.getInt("mond", day.monthDays);
}
void putI(const char*k,int v){ if(prefs.putInt(k,v)==0){ nvsFull=true; nvsOk=false; } }
void putF(const char*k,float v){ if(prefs.putFloat(k,v)==0){ nvsFull=true; nvsOk=false; } }
void saveRecords(){
  if(!nvsOk) return;
  putI("mPvW",rec.maxPvW); putI("mLoadW",rec.maxLoadW);
  putI("mChgW",rec.maxChargeW); putI("mDisW",rec.maxDischargeW);
  putF("mBatV",rec.maxBattV); putF("nBatV",rec.minBattV);
  putI("mSoc",rec.maxSoc); putI("nSoc",rec.minSoc);
  putF("bDay",rec.bestDayWh); putF("tChg",rec.totalChargedWh); putF("tDis",rec.totalDischargedWh);
  putF("life",rec.lifetimeDischargedAh);
  prefs.putBytes("week", day.weekYieldWh, sizeof(day.weekYieldWh));
  putI("dlog",day.daysLogged); putF("mon",day.monthYieldWh); putI("mond",day.monthDays);
}
void settingsLoad(){
  if(!nvsOk) return;
  // max() beschermt tegen een oude NVS-waarde van vóór MIN_BRIGHTNESS_PCT (of
  // een eerdere 0%-opslag door de slider-bug) - anders blijft dat scherm zwart
  // ondanks de nieuwe ondergrens, want dan wordt hij hier alsnog overschreven.
  appSettings.brightnessPct   = max((int)prefs.getInt("brPct", appSettings.brightnessPct), MIN_BRIGHTNESS_PCT);
  appSettings.screenOffEnabled= prefs.getBool("scrOff", appSettings.screenOffEnabled);
  appSettings.battProfileIdx  = prefs.getInt("battIdx", appSettings.battProfileIdx);
  applyBatteryProfile(appSettings.battProfileIdx);
}
void settingsSave(){
  if(!nvsOk) return;
  prefs.putInt("brPct", appSettings.brightnessPct);
  prefs.putBool("scrOff", appSettings.screenOffEnabled);
  prefs.putInt("battIdx", appSettings.battProfileIdx);
}

// ---- ROLLENDE KLOK (geen RTC) -----------------------------------------------
// simHour/simHourF = positie binnen het rollende 24u-venster. In DEMO versneld
// (SIM_HOUR_REAL_MS per uur, voor een prettige demo); in LIVE gelijk aan de
// echte verstreken tijd sinds de laatste rollover.
void dayReset();
void dayRollover();
void advanceClock(unsigned long now){
  if(dataSource == SRC_DEMO){
    static unsigned long lastSim = 0;
    if(now - lastSim < (SIM_HOUR_REAL_MS/60)) return;
    lastSim = now;
    simHourF += 1.0/60.0;
    if(simHourF >= 24.0f){ simHourF -= 24.0f; dayRollover(); }
    simHour = (int)simHourF;
  } else {
    static unsigned long lastDayCount = 0;
    const unsigned long DAY_MS = 24UL*3600000UL;
    unsigned long dayCount = now / DAY_MS;
    if(dayCount != lastDayCount){ lastDayCount = dayCount; dayRollover(); }
    simHourF = (now % DAY_MS) / 3600000.0f;
    simHour = (int)simHourF;
  }
}

// ---- SIMULATIE (DEMO) --------------------------------------------------------
float frand(float lo, float hi){ return lo + (hi-lo)*(random(0,1001)/1000.0); }
float solarFactor(float h){ if(h<6||h>20) return 0; float x=(h-13.0)/6.0; float f=1-x*x; return f<0?0:f; }
void simulateData(){
  float sf = solarFactor(simHourF);
#if SIM_SCENARIO==1
  sf *= 0.4;
#elif SIM_SCENARIO==2
  sf = 0;
#endif
  live.pvW = (int)(sf*frand(300,360));
  live.pvV = sf>0 ? frand(30,42) : frand(0,5);
  live.pvA = live.pvV>1 ? live.pvW/live.pvV : 0;
  if(live.pvW>250) live.state=CS_BULK; else if(live.pvW>120) live.state=CS_ABSORPTION;
  else if(live.pvW>5) live.state=CS_FLOAT; else live.state=CS_OFF;
  live.loadW = (int)frand(50,120);
#if SIM_BATTERIES >= 2
  live.battCount = 2;
  live.batt1V = frand(BATT.vNomMin, BATT.vNomMax);
  live.batt2V = live.batt1V + frand(-0.08, 0.08);
  live.battV  = live.batt1V + live.batt2V;
  live.midDev = live.battV>0 ? (live.batt1V-live.batt2V)/live.battV*100.0 : 0;
#else
  live.battCount = 1;
  live.batt1V = frand(BATT.vNomMin, BATT.vNomMax);
  live.batt2V = 0; live.midDev = 0;
  live.battV  = live.batt1V;
#endif
  live.battW = live.pvW - live.loadW;
  live.battA = live.battV>0 ? live.battW/live.battV : 0;
  live.cycles = (int)(rec.lifetimeDischargedAh / BATT.capacityAh);
  static float socF = (SIM_SCENARIO==3) ? 16.0 : 84.0;
  socF += (live.battW/3000.0);
  if(socF>100) socF=100; if(socF<8) socF=8;
  live.soc = (int)socF;
  live.remainingAh = BATT.capacityAh*live.soc/100.0;
  live.consumedAh  = BATT.capacityAh - live.remainingAh;
  int chargeW = live.battW>0 ? (int)live.battW : 0;
  live.directSolarW = live.pvW - chargeW; if(live.directSolarW<0) live.directSolarW=0;
  demoGainPct = (live.pvW+live.loadW)>0 ? (100*live.pvW/(live.pvW+live.loadW)) : 0;
}

// ---- AGGREGATIE (rollend 24u-venster, index = simHour) ----------------------
void aggregate(){
  float dtH = dataSource==SRC_DEMO ? 1.0/(SIM_HOUR_REAL_MS/1000.0) : (DATA_INTERVAL/1000.0)/3600.0;
  float dtS = dtH*3600.0;
  int h = simHour % 24;

  if(solarValid){
    day.pvW_sum += live.pvW; day.pvW_n++;
    if(live.pvW > day.pvW_max) day.pvW_max = live.pvW;
    day.yieldWh += live.pvW*dtH;
    day.pvHour[h] = (day.pvHour[h]*day.pvHourN[h] + live.pvW) / (day.pvHourN[h]+1); day.pvHourN[h]++;
    if(live.pvW > rec.maxPvW) rec.maxPvW = live.pvW;
    if(live.state==CS_BULK) day.phaseSec[0]+=dtS;
    else if(live.state==CS_ABSORPTION) day.phaseSec[1]+=dtS;
    else if(live.state==CS_FLOAT) day.phaseSec[2]+=dtS;
  }
  if(battValid){
    if(live.battV<day.battV_min) day.battV_min=live.battV;
    if(live.battV>day.battV_max) day.battV_max=live.battV;
    day.soc_sum += live.soc; day.soc_n++;
    if(live.soc<day.soc_min) day.soc_min=live.soc;
    if(live.soc>day.soc_max) day.soc_max=live.soc;
    if(live.battA<day.battA_min) day.battA_min=live.battA;
    if(live.battA>day.battA_max) day.battA_max=live.battA;
    if(live.battW<day.battW_min) day.battW_min=live.battW;
    if(live.battW>day.battW_max) day.battW_max=live.battW;
    if(live.battW>0){ day.chargedWh+=live.battW*dtH; rec.totalChargedWh+=live.battW*dtH; }
    else            { day.dischargedWh+=(-live.battW)*dtH; rec.totalDischargedWh+=(-live.battW)*dtH; }
    if(live.battA<0) rec.lifetimeDischargedAh += (-live.battA)*dtH;
    day.socHour[h]=live.soc; day.socHourSet[h]=true;
    if(live.battW>rec.maxChargeW) rec.maxChargeW=(int)live.battW;
    if(-live.battW>rec.maxDischargeW) rec.maxDischargeW=(int)(-live.battW);
    if(live.battA>rec.maxChargeA) rec.maxChargeA=live.battA;
    if(-live.battA>rec.maxDischargeA) rec.maxDischargeA=-live.battA;
    if(live.battV>rec.maxBattV) rec.maxBattV=live.battV;
    if(live.battV<rec.minBattV) rec.minBattV=live.battV;
    if(live.soc>rec.maxSoc) rec.maxSoc=live.soc;
    if(live.soc<rec.minSoc) rec.minSoc=live.soc;
    static int trendPrev=-1; static unsigned long trendTs=0;
    if(millis()-trendTs>30000){ trendTs=millis();
      if(trendPrev>=0) socTrend = live.soc>trendPrev+1 ? 1 : live.soc<trendPrev-1 ? -1 : 0;
      trendPrev=live.soc; }
  }
  if(solarValid && battValid){
    if(live.loadW>day.load_max) day.load_max=live.loadW;
    day.loadWh += live.loadW*dtH;
    if(live.loadW>rec.maxLoadW) rec.maxLoadW=live.loadW;
  }
}
void dayReset(){
  day.pvW_sum=0; day.pvW_n=0; day.pvW_max=0; day.battV_min=9999; day.battV_max=0;
  day.battA_min=9999; day.battA_max=-9999; day.battW_min=99999; day.battW_max=-99999;
  day.soc_sum=0; day.soc_n=0; day.soc_min=100; day.soc_max=0; day.load_max=0;
  day.yieldWh=day.loadWh=day.chargedWh=day.dischargedWh=0;
  day.phaseSec[0]=day.phaseSec[1]=day.phaseSec[2]=0;
  for(int i=0;i<24;i++){ day.pvHour[i]=0; day.pvHourN[i]=0; day.socHour[i]=0; day.socHourSet[i]=false; }
}
void dayRollover(){
  if(day.yieldWh>rec.bestDayWh) rec.bestDayWh=day.yieldWh;
  if(day.loadWh>rec.maxDayLoadWh) rec.maxDayLoadWh=day.loadWh;
  for(int i=0;i<6;i++) day.weekYieldWh[i]=day.weekYieldWh[i+1];
  day.weekYieldWh[6]=day.yieldWh;
  if(day.daysLogged<7) day.daysLogged++;
  day.monthYieldWh += day.yieldWh; day.monthDays++;
  if(day.monthDays>=30){ day.monthYieldWh=0; day.monthDays=0; }
  saveRecords(); dayReset();
}

// ---- ALARM (onboard speaker) -------------------------------------------------
void alarmUpdate(unsigned long now){
#if ALARM_ENABLED
  static unsigned long lastBeep = 0;
  const char* r; bool alarm = (systemAlertLevel(&r) == ALERT_ALARM);
  if(alarm && now - lastBeep > 2000){ lastBeep = now; tone(SPEAKER_PIN, ALARM_FREQ_HZ, ALARM_BEEP_MS); }
#endif
}

// ---- BACKLIGHT --------------------------------------------------------------
unsigned long lastTouchMs = 0;
void backlightUpdate(unsigned long now){
  int pct; const char* reason;
  if(appSettings.screenOffEnabled && now - lastTouchMs > OFF_TIMEOUT_MS){ pct = 0; reason = "OFF (screenOffEnabled, >5min idle)"; }
  else if(now - lastTouchMs > DIM_TIMEOUT_MS){ pct = (DIM_BRIGHTNESS*100)/BL_MAX; reason = "DIM (>30s idle)"; }
  else { pct = max(appSettings.brightnessPct, MIN_BRIGHTNESS_PCT); reason = "NORMAAL"; }
  int bl = map(constrain(pct,0,100), 0, 100, 0, BL_MAX);

  // alleen loggen bij een ECHTE overgang (niet elke loop-tick) - anders spamt
  // dit de Serial Monitor vol terwijl er niets verandert
  static int lastBl = -1;
  if(bl != lastBl){
    Serial.printf("[BL] backlight -> %d/%d (%s) | idle=%lums screenOffEnabled=%d\n",
                  bl, BL_MAX, reason, now - lastTouchMs, appSettings.screenOffEnabled);
    lastBl = bl;
  }
  ledcWrite(TFT_BL_PIN, bl);
}

// ---- PAGINABEHEER ------------------------------------------------------------
int curPage = 0;
bool firstDraw = true;
enum UIMode { MODE_SWIPE, MODE_SETTINGS, MODE_PICKER, MODE_SAVER };
UIMode uiMode = MODE_SWIPE;
UIMode preSaverMode = MODE_SWIPE;   // waar we naar terugkeren zodra de screensaver eindigt
bool saverNeedsInit = true;
bool suppressNextRelease = false;   // negeert het loslaten van de aanraking die net wekte

void gotoPage(int idx){
  curPage = constrain(idx, 0, PAGE_COUNT-1);
  uiMode = MODE_SWIPE; firstDraw = true;
  resetWidgetCache(); clearTapZones();
  tft.fillScreen(COL.bg);
  Serial.printf("[PAGE] -> %d (%s)\n", curPage, PAGES[curPage].title);
}
void openSettings(){ uiMode=MODE_SETTINGS; firstDraw=true; resetWidgetCache(); clearTapZones(); tft.fillScreen(COL.bg); Serial.println("[PAGE] -> INSTELLINGEN"); }
void openPicker(){   uiMode=MODE_PICKER;   firstDraw=true; resetWidgetCache(); clearTapZones(); tft.fillScreen(COL.bg); Serial.println("[PAGE] -> PAGINA'S"); }

// ---- SCREENSAVER (na DIM_TIMEOUT_MS inactiviteit) --------------------------
// Geen aparte "dim"-actie meer nodig van deze functie zelf - backlightUpdate()
// regelt de helderheid al op basis van dezelfde DIM_TIMEOUT_MS/OFF_TIMEOUT_MS.
// Dit is puur de VISUELE screensaver: een langzaam stuiterende statusregel
// (tijd/SoC/PV), zodat er iets te zien is i.p.v. de bevroren laatste pagina.
void enterSaver(){
  if(uiMode == MODE_SAVER) return;
  preSaverMode = uiMode;
  uiMode = MODE_SAVER;
  saverNeedsInit = true;
  tft.fillScreen(COL.bg);
  Serial.println("[SAVER] -> screensaver (30s inactiviteit)");
}
void exitSaver(){
  uiMode = preSaverMode;
  firstDraw = true;
  resetWidgetCache(); clearTapZones();
  tft.fillScreen(COL.bg);
  Serial.println("[SAVER] -> terug naar dashboard (aanraking)");
}
// Energie-flow screensaver - dit is een directe poort van geaSaver() uit het
// originele ESP32-D-project (theme_geavanceerd.h), niet een eigen herontwerp:
// zon/maan -> knooppunt -> huis, en knooppunt -> accu (met echte SoC-vulling),
// met VLOEIEND geïnterpoleerde stroompjes (geen stap-voor-stap modulo) die van
// richting wisselen al naargelang de accu laadt/ontlaadt, en 's nachts stil-
// staan i.p.v. animeren. Geen klok (op verzoek) - in plaats daarvan expliciet
// PV(W)/verbruik(W)/accu(±W)/SoC(%), dat is waar we hier echt iets aan hebben.
static void drawSaverGroup(int gx, int gy, int sunRelX, int houseRelX, int nodeRelY,
                            int batRelX, int batRelY, int batW, int batH){
  bool night = live.pvW <= 5;
  int sx = gx+sunRelX, sy = gy+nodeRelY;
  int hx = gx+houseRelX, hy = gy+nodeRelY;
  int bx = gx+batRelX,  by = gy+batRelY;

  if(!night){
    tft.fillCircle(sx, sy, 8, COL.amber);
    for(int a=0;a<360;a+=45){ float rd=a*DEG_TO_RAD;
      tft.drawLine(sx+cos(rd)*10, sy+sin(rd)*10, sx+cos(rd)*13, sy+sin(rd)*13, COL.amber); }
  } else {
    tft.fillCircle(sx, sy, 8, COL.muted);
    tft.fillCircle(sx+4, sy-2, 7, COL.bg);   // ponst een maansikkel (vlakke bg hier, dus veilig)
  }

  tft.fillRect(hx-8, hy-2, 16, 12, COL.muted);
  tft.fillTriangle(hx-10, hy-2, hx+10, hy-2, hx, hy-12, COL.muted);

  tft.fillRect(bx+batW/2-4, by-3, 8, 3, COL.text);
  tft.drawRect(bx, by, batW, batH, COL.text);
  tft.fillRect(bx+1, by+1, batW-2, batH-2, COL.card);
  int fh = (int)((batH-2) * live.soc/100.0);
  tft.fillRect(bx+1, by+batH-1-fh, batW-2, fh, socColor(live.soc));

  tft.setTextFont(2); tft.setTextSize(1); tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL.amber, COL.bg); tft.drawString(fmtW(live.pvW), sx, sy+16);
  tft.setTextColor(COL.text,  COL.bg); tft.drawString(fmtW(live.loadW), hx, hy+16);

  String battStr = (live.battW>=0?"+":"") + fmtW((int)live.battW);
  uint16_t battValCol = live.battW>5?COL.green:live.battW<-5?COL.coral:COL.muted;
  tft.setTextColor(battValCol, COL.bg); tft.drawString(battStr, bx+batW/2, by+batH+4);
  tft.setTextColor(socColor(live.soc), COL.bg); tft.drawString(String(live.soc)+"%", bx+batW/2, by+batH+20);
}

void renderScreensaver(){
  static bool init = false;
  static float gx, gy, vx, vy;
  static unsigned long lastMove = 0, lastFlow = 0;
  static float phase = 0;
  static int pdx[9], pdy[9]; static bool phave = false;

  // GH was 130, maar de SoC%-tekst onder de accu (by+batH+20, ~16px hoog)
  // reikt tot ~136 - dat stak dus 6px buiten het vlak dat bij elke stuiter
  // gewist wordt, en liet precies daardoor een lijntje pixels achter zodra
  // het blok wegbewoog. GH vergroot met ruime marge om dat definitief te dekken.
  const int GW = 200, GH = 144;
  const int sunRelX = 30, houseRelX = 170, nodeRelX = 100, nodeRelY = 40;
  const int batW = 34, batH = 26, batRelX = nodeRelX - batW/2, batRelY = nodeRelY + 34;

  if(saverNeedsInit){ saverNeedsInit = false; init = false; }
  if(!init){
    init = true; gx = 40; gy = 40; vx = 0.9f; vy = 0.6f;
    tft.fillScreen(COL.bg); phave = false;
  }

  unsigned long now = millis();

  // BOUNCE: hele blok langzaam verplaatsen (elke 260ms, zoals SAVER_MOVE_MS origineel)
  if(now - lastMove >= 260){
    lastMove = now;
    tft.fillRect((int)gx, (int)gy, GW, GH, COL.bg);
    gx += vx; gy += vy;
    if(gx <= 0){ gx = 0; vx = -vx; }
    if(gx >= SCR_W-GW){ gx = SCR_W-GW; vx = -vx; }
    if(gy <= 0){ gy = 0; vy = -vy; }
    if(gy >= SCR_H-GH){ gy = SCR_H-GH; vy = -vy; }
    drawSaverGroup((int)gx, (int)gy, sunRelX, houseRelX, nodeRelY, batRelX, batRelY, batW, batH);
    phave = false;
  }

  // stroompjes: los, sneller tempo dan de stuiter, maar iets rustiger dan
  // eerst (was 70ms) - vloeiend geïnterpoleerd (phase 0..1), snelheid schaalt
  // mee met PV+verbruik
  if(now - lastFlow >= 90){
    lastFlow = now;
    bool night = live.pvW <= 5;
    float spd = 0.03f + (live.pvW + live.loadW) * 0.00015f; if(spd > 0.20f) spd = 0.20f;
    phase += spd; if(phase > 1) phase -= 1;
    int dir = live.battW > 5 ? 1 : live.battW < -5 ? -1 : 0;   // >0 laden, <0 ontladen

    int bgx = (int)gx, bgy = (int)gy;
    int sx = bgx+sunRelX, hx = bgx+houseRelX, nx = bgx+nodeRelX, ny = bgy+nodeRelY;
    int bty = bgy+batRelY;

    if(phave) for(int i=0;i<9;i++) tft.fillCircle(pdx[i], pdy[i], 2, COL.bg);
    int idx = 0;
    for(int k=0;k<3;k++){
      float f = phase + k/3.0f; if(f>1) f -= 1;
      { int x0=sx+16, x1=nx-3; float ff = night ? (k+1)/4.0f : f;   // 's nachts: stilstaand, niet animerend
        pdx[idx]=x0+(int)((x1-x0)*ff); pdy[idx]=ny;
        tft.fillCircle(pdx[idx], pdy[idx], 2, night?COL.muted:COL.amber); idx++; }
      { int x0=nx+3, x1=hx-16;
        pdx[idx]=x0+(int)((x1-x0)*f); pdy[idx]=ny;
        tft.fillCircle(pdx[idx], pdy[idx], 2, loadColor(live.loadW)); idx++; }
      { int y0=ny+4, y1=bty-3; float ff = dir>=0 ? f : 1-f;   // richting keert om bij ontladen
        uint16_t c = dir>0?COL.green:dir<0?COL.coral:COL.border;
        pdx[idx]=nx; pdy[idx]=y0+(int)((y1-y0)*ff);
        tft.fillCircle(pdx[idx], pdy[idx], 2, c); idx++; }
    }
    phave = true;
  }
}

void renderCurrent(){
  if(uiMode == MODE_SAVER){
    renderScreensaver();
    return;
  }
  if(uiMode == MODE_SWIPE){
    if(curPage == PAGE_DASHBOARD){
      drawStatusBar();
      if(firstDraw) addTapZone(SCR_W-40,0,40,28,PAGE_SETTINGS);   // grid-icoon -> direct Instellingen
    } else {
      drawSubHeader(PAGES[curPage].title);
      if(firstDraw){ addTapZone(0,0,40,28,PAGE_DASHBOARD); addTapZone(SCR_W-40,0,40,28,PAGE_SETTINGS); }
    }
    PAGES[curPage].fn(firstDraw);
    drawNavDots(curPage);
  } else if(uiMode == MODE_SETTINGS){
    drawSubHeader("INSTELLINGEN");
    if(firstDraw) addTapZone(0,0,40,28,PAGE_DASHBOARD);
    SETTINGS_PAGE.fn(firstDraw);
  } else {
    drawSubHeader("PAGINA'S");
    if(firstDraw) addTapZone(0,0,40,28,PAGE_DASHBOARD);
    PICKER_PAGE.fn(firstDraw);
  }
  firstDraw = false;
}

// ---- OPSTART + VE.DIRECT-ZOEKEN GECOMBINEERD (1 scherm, sneller) -----------
// Voorheen twee na elkaar lopende schermen (zoeken tot 15s, dan nog een losse
// 6s-opstartanimatie erna - tot 21s in totaal). Nu een enkele accu-balk die
// als voortgang van de zoektocht dient: is er VE.Direct geconfigureerd, dan
// loopt de balk mee met de wachttijd (max VEDIRECT_WAIT_MS) en springt hij
// direct naar vol zodra er data binnenkomt (kan dus véél sneller klaar zijn
// dan het maximum); is er niets geconfigureerd, dan is het gewoon een korte
// (BOOT_DURATION) animatie voordat DEMO start.
// Tikt een tekst letter voor letter uit (typewriter-effect), gecentreerd op
// de EINDBREEDTE zodat de tekst niet zijwaarts "kruipt" terwijl hij verschijnt.
// Tikt elke letter apart en EENMALIG neer op zijn eigen plek (i.p.v. de hele
// groeiende string steeds opnieuw te wissen+tekenen) - dezelfde "alleen
// tekenen wat nodig is"-aanpak als de rest van de UI, en toevallig ook een
// leukere typewriter-look (elke letter verschijnt echt als eigen letter).
static void typeTextCentered(int cx, int y, uint8_t font, uint16_t col, const String &s, int msPerChar){
  tft.setTextFont(font); tft.setTextSize(1);
  int fullW = tft.textWidth(s);
  int x = cx - fullW/2;
  tft.setTextDatum(TL_DATUM); tft.setTextColor(col, COL.bg);
  for(unsigned int i = 0; i < s.length(); i++){
    String ch = String(s[i]);
    tft.drawString(ch, x, y);       // alleen déze ene letter - de vorige staan al
    x += tft.textWidth(ch);
    delay(msPerChar);
  }
}

// Bedradingsgids als Star Wars-achtige omhoog-scrollende crawl, binnen een
// vast kadertje (tft.setViewport() clipt alle tekst binnen dit kader, dus de
// regels verschijnen onderin en verdwijnen bovenin zonder handmatig te hoeven
// bijhouden welk deel van elke regel nog zichtbaar is). Regels bovenin het
// kader (verder "weg") worden gedimd getekend - een goedkope diepte-illusie
// zonder dat we per regel het lettertype hoeven te verkleinen.
// Snelheid is een VASTE pasduur (CRAWL_MS), losstaand van de zoektijd: bij
// VEDIRECT_WAIT_MS=15s past de hele gids ruim 2x, dus "start over" gebeurt
// vanzelf via de modulo - en de gids is altijd al na de eerste, kortere pas
// volledig getoond, ruim binnen de 15 seconden.
static void drawWiringCrawl(unsigned long elapsedMs, int vx, int vy, int vw, int vh){
  static String lines[16]; static uint16_t lineCol[16]; static int lineCount = 0;
  if(lineCount == 0){
    uint16_t victronBlue = tft.color565(0, 105, 170);
    int i = 0;
    lines[i]="VE.DIRECT BEDRADING";                       lineCol[i]=COL.text;   i++;
    lines[i]="";                                          lineCol[i]=COL.text;   i++;
    lines[i]="MPPT LADER";                                lineCol[i]=victronBlue;i++;
    lines[i]=" TX  ->  GPIO " + String(VEDIRECT_MPPT_RX); lineCol[i]=COL.text;   i++;
    lines[i]=" RX  ->  GPIO " + String(VEDIRECT_SHARED_TX);lineCol[i]=COL.text;  i++;
    lines[i]=" GND ->  GND";                              lineCol[i]=COL.muted;  i++;
    lines[i]="";                                          lineCol[i]=COL.text;   i++;
    lines[i]="BATTERY SHUNT";                             lineCol[i]=victronBlue;i++;
    lines[i]=" TX  ->  GPIO " + String(VEDIRECT_SHUNT_RX);lineCol[i]=COL.text;   i++;
    lines[i]=" RX  ->  GPIO " + String(VEDIRECT_SHARED_TX);lineCol[i]=COL.text;  i++;
    lines[i]=" GND ->  GND";                              lineCol[i]=COL.muted;  i++;
    lines[i]="";                                          lineCol[i]=COL.text;   i++;
    lines[i]="LET OP: TX/RX KRUISEN";                     lineCol[i]=COL.amber;  i++;
    lines[i]="+V-pin blijft los";                         lineCol[i]=COL.muted;  i++;
    lineCount = i;
  }

  const int lineH = 14;
  const unsigned long CRAWL_MS = 10000;  // duur van 1 volledige pas (was 6000 - ging te snel om te lezen)
  int contentH = lineCount * lineH;
  int totalDist = vh + contentH;         // van helemaal onder tot helemaal boven verdwenen
  int scrollY = (int)(((unsigned long)(elapsedMs % CRAWL_MS)) * (unsigned long)totalDist / CRAWL_MS);

  tft.setViewport(vx, vy, vw, vh);       // alle coördinaten hierna zijn t.o.v. dit kader
  tft.fillRect(0, 0, vw, vh, COL.bg);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(1); tft.setTextSize(1);
  for(int i = 0; i < lineCount; i++){
    int ly = vh - scrollY + i*lineH;
    if(ly < -lineH || ly > vh) continue;              // buiten het kader, niet tekenen
    uint16_t c = (ly < vh/3) ? COL.muted : lineCol[i]; // bovenin = "ver weg" = gedimd
    tft.setTextColor(c, COL.bg);
    tft.drawString(lines[i], vw/2, ly);
  }
  tft.resetViewport();
}

void playBootAndSearch(){
  uint16_t victronBlue = tft.color565(0, 105, 170);
  tft.fillScreen(COL.bg);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(4); tft.setTextSize(1); tft.setTextColor(COL.text, COL.bg);
  tft.drawString("ENERGIE MONITOR", SCR_W/2, 26);

  // fancy eigenaarsvermelding: typewriter-effect, klein en bescheiden
  typeTextCentered(SCR_W/2, 52, 1, COL.muted, "gemaakt door Eric Bruggema", 30);
  delay(300);

  tft.setTextFont(2); tft.setTextColor(victronBlue, COL.bg); tft.setTextDatum(TC_DATUM);
  tft.drawString(veConfigured ? "zoeken: VE.Direct" : "DEMO-modus", SCR_W/2, 74);

  int bw=140, bh=26, bx=SCR_W/2-bw/2, by=100;
  tft.drawRoundRect(bx,by,bw,bh,4,COL.text);

  // bedradingsgids alleen tonen als er ook echt iets aan te sluiten valt
  int crawlY = by+bh+38;
  bool showCrawl = veConfigured && (SCR_H - crawlY - 4) > 30;
  if(showCrawl) tft.drawFastHLine(8, crawlY-4, SCR_W-16, COL.border);

  unsigned long searchMs = veConfigured ? (unsigned long)VEDIRECT_WAIT_MS : (unsigned long)BOOT_DURATION;
  unsigned long t0 = millis();
  bool got = false;
  int lastFw = -1, lastSec = -1;

  while(true){
    unsigned long el = millis() - t0;
    if(veConfigured){
      veDirectPoll();
      if(!veStale){ got = true; break; }         // data binnen -> meteen klaar, niet uitzitten
    }
    if(el >= searchMs) break;

    int pct = (int)(100.0 * el / searchMs);
    int fw = (bw-4)*pct/100;
    if(fw != lastFw){ lastFw = fw;
      uint16_t c = pct>=70?COL.green:pct>=30?COL.amber:COL.red;
      tft.fillRect(bx+2,by+2,fw,bh-4,c);
    }
    if(veConfigured){
      int secLeft = (int)((searchMs - el)/1000) + 1;
      if(secLeft != lastSec){ lastSec = secLeft;
        tft.fillRect(SCR_W/2-30, by+bh+10, 60, 22, COL.bg);
        tft.setTextFont(4); tft.setTextColor(victronBlue, COL.bg); tft.setTextDatum(TC_DATUM);
        tft.drawString(String(secLeft), SCR_W/2, by+bh+12);
      }
    }
    if(showCrawl) drawWiringCrawl(el, 0, crawlY, SCR_W, SCR_H-crawlY-4);
    delay(20);
  }

  dataSource = got ? SRC_LIVE : SRC_DEMO;
  if(dataSource==SRC_DEMO){ simulateData(); veSeedDemo(); }

  // balk direct laten "volschieten" (gevonden = groen, timeout = amber, DEMO = groen)
  tft.fillRect(bx+2,by+2,bw-4,bh-4, got ? COL.green : (veConfigured?COL.amber:COL.green));

  // alles onder de balk wissen (incl. de bedradingsgids) voor de eindmelding
  tft.fillRect(0, by+bh+8, SCR_W, SCR_H-(by+bh+8), COL.bg);
  tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(COL.text, COL.bg); tft.setTextDatum(TC_DATUM);
  String src = dataSource==SRC_LIVE ? "LIVE VE.Direct gevonden" : (veConfigured?"Geen data -> DEMO":"DEMO-simulatie");
  tft.drawString(src, SCR_W/2, by+bh+18);

  delay(1200);
  tft.fillScreen(COL.bg);
}

// ---- SETUP / LOOP -------------------------------------------------------------
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\n==================================================");
  Serial.println("[BOOT] Victron Dashboard CYD - setup() start");
  Serial.println("==================================================");

  Serial.println("[BOOT] 1/8 backlight-pin (ledcAttach) instellen...");
  ledcAttach(TFT_BL_PIN, BL_FREQ, BL_RES);
  ledcWrite(TFT_BL_PIN, 0);
  Serial.printf("[BOOT]     klaar. TFT_BL_PIN=%d BL_FREQ=%d BL_RES=%d\n", TFT_BL_PIN, BL_FREQ, BL_RES);

  Serial.println("[BOOT] 2/8 tft.init() aanroepen...");
  tft.init();
  tft.setRotation(1);
  Serial.printf("[BOOT]     klaar. tft.width()=%d tft.height()=%d (verwacht 320x240 - staat hier 0 of iets anders, dan is User_Setup.h niet (goed) toegepast)\n",
                tft.width(), tft.height());
  tft.fillScreen(TFT_BLACK);
  Serial.println("[BOOT]     fillScreen(TFT_BLACK) verzonden");
  ledcWrite(TFT_BL_PIN, BL_MAX);
  Serial.println("[BOOT]     backlight AAN (vol) - scherm zou nu ZWART moeten zijn, niet wit/leeg");

  Serial.println("[BOOT] 3/8 touchInit() (XPT2046, eigen SPI-bus)...");
  touchInit();
  Serial.println("[BOOT]     klaar");

  Serial.println("[BOOT] 4/8 colorsInit()...");
  colorsInit();
  Serial.println("[BOOT]     klaar");

  randomSeed(analogRead(35));

  Serial.println("[BOOT] 5/8 NVS/records/instellingen laden...");
  nvsInit(); recordsInit(); loadRecords(); settingsLoad(); dayReset();
  Serial.printf("[BOOT]     NVS: %s\n", nvsOk ? "OK" : "niet beschikbaar (RAM-only)");
  sdPresent = SD.begin(SD_CS_PIN);
  Serial.printf("[BOOT]     SD-kaart: %s\n", sdPresent ? "aanwezig" : "niet gevonden");

#if ALARM_ENABLED
  pinMode(SPEAKER_PIN, OUTPUT);
  Serial.println("[BOOT]     speaker-pin ingesteld");
#endif

  // --- databron: LIVE (VE.Direct) of DEMO, gecombineerd met het opstartscherm ---
  Serial.println("[BOOT] 6/8 VE.Direct starten...");
  veDirectBegin();
  Serial.printf("[BOOT]     veConfigured=%d (MPPT_RX=%d SHUNT_RX=%d SHARED_TX=%d)\n",
                veConfigured, (int)VEDIRECT_MPPT_RX, (int)VEDIRECT_SHUNT_RX, (int)VEDIRECT_SHARED_TX);
  Serial.printf("[BOOT]     playBootAndSearch() - max %lums (of tot data binnenkomt)...\n",
                veConfigured ? (unsigned long)VEDIRECT_WAIT_MS : (unsigned long)BOOT_DURATION);
  playBootAndSearch();
  Serial.println(dataSource==SRC_LIVE ? "[BRON] LIVE VE.Direct gevonden" : "[BRON] DEMO");

  // --- touch-kalibratie: eerste keer verplicht, anders overslaan ---
  Serial.printf("[BOOT] 7/8 touchCalibrated()=%d\n", touchCalibrated());
  if(!touchCalibrated()){
    Serial.println("[BOOT]     GEEN kalibratie in NVS -> touchRunCalibration() start (zie hieronder voor voortgang)");
    touchRunCalibration();
    Serial.println("[BOOT]     kalibratie klaar en opgeslagen");
  } else {
    Serial.println("[BOOT]     kalibratie al aanwezig, overslaan");
  }

  lastTouchMs = millis();
  Serial.println("[BOOT] 8/8 gotoPage(PAGE_DASHBOARD)");
  gotoPage(PAGE_DASHBOARD);
  Serial.println("[BOOT] setup() volledig klaar.\n");
}

void loop(){
  unsigned long now = millis();

  advanceClock(now);
  if(dataSource==SRC_LIVE){ veDirectPoll(); veHexPoll(); }
  if(dataSource==SRC_DEMO && veConfigured){
    veDirectDrain();
    if(!veStale){ dataSource=SRC_LIVE; gotoPage(PAGE_DASHBOARD); Serial.println("[BRON] data gevonden -> LIVE"); }
  }

  static unsigned long lastData=0, lastNvs=0;
  if(now-lastData>=DATA_INTERVAL){ lastData=now;
    if(dataSource==SRC_DEMO){ simulateData(); solarValid=true; battValid=true; }
    if(battValid){ systemV = live.battV>18.0 ? 24 : 12; vMult = systemV/12.0; }
    aggregate();
  }
  if(now-lastNvs>=NVS_SAVE_MS){ lastNvs=now; saveRecords(); }

  alarmUpdate(now);
  backlightUpdate(now);

  // DIM_TIMEOUT_MS inactiviteit -> screensaver (de helderheid zelf regelt
  // backlightUpdate() al op basis van diezelfde/OFF_TIMEOUT_MS drempels)
  if(uiMode != MODE_SAVER && (now - lastTouchMs) >= DIM_TIMEOUT_MS) enterSaver();

  // ---- periodieke heartbeat (elke 2s), zodat je in de Serial Monitor kunt
  // zien dat loop() daadwerkelijk draait (i.p.v. ergens vast te lopen) ----
  static unsigned long lastHeartbeat = 0;
  if(now - lastHeartbeat >= 2000){
    lastHeartbeat = now;
    Serial.printf("[LOOP] t=%lus bron=%s stale=%d curPage=%d uiMode=%d SoC=%d PV=%dW vrijRAM=%uKB\n",
                  now/1000, dataSource==SRC_LIVE?"LIVE":"DEMO", veStale, curPage, (int)uiMode,
                  live.soc, live.pvW, (unsigned)(ESP.getFreeHeap()/1024));
  }

  // ---- AANRAKING -> NAVIGATIE / INTERACTIE ----
  Gesture g = touchPoll();
  if(g.type != GEST_NONE) lastTouchMs = now;
  // tijdstempel erbij: als dit heel snel achter elkaar afvuurt (elke paar ms i.p.v.
  // een bewuste tik) is dat een teken van elektrische ruis op de touch-lijn i.p.v.
  // een echte aanraking (zelfde soort probleem als de PIR-ruis in het ESP32-D-project)
  if(g.type == GEST_DOWN) Serial.printf("[TOUCH] t=%lums down  x=%d y=%d\n", now, g.x, g.y);
  if(g.type == GEST_TAP)  Serial.printf("[TOUCH] t=%lums tap   x=%d y=%d\n", now, g.x, g.y);
  if(g.type == GEST_SWIPE_LEFT)  Serial.printf("[TOUCH] t=%lums swipe links\n", now);
  if(g.type == GEST_SWIPE_RIGHT) Serial.printf("[TOUCH] t=%lums swipe rechts\n", now);

  bool wokeFromSaver = false;
  if(uiMode == MODE_SAVER && g.type != GEST_NONE){
    exitSaver();
    wokeFromSaver = true;                            // deze aanraking alleen laten wekken, niet OOK laten navigeren
    if(g.type == GEST_DOWN) suppressNextRelease = true;  // en het bijbehorende loslaten ook nog negeren
  }

  if(wokeFromSaver){
    // niets verder doen met dit gebaar
  } else if(suppressNextRelease && (g.type == GEST_TAP || g.type == GEST_SWIPE_LEFT || g.type == GEST_SWIPE_RIGHT)){
    suppressNextRelease = false;   // loslaten van de wek-aanraking - negeren, geen navigatie
  } else if(g.type == GEST_TAP){
    // header-tikzones (terug-pijl / grid-icoon) gaan ALTIJD voor, ook in
    // Instellingen - anders "vangt" pageSettingsTouch() elke tik weg en werkt
    // de zichtbare terug-pijl daar niet meer.
    PageId hit = hitTestZones(g.x, g.y);
    Serial.printf("[TOUCH] hitTestZones -> %d (PAGE_NONE=%d)\n", (int)hit, (int)PAGE_NONE);
    if(hit == PAGE_SETTINGS) openSettings();   // grid-icoon: rechtstreeks naar Instellingen (geen paginakiezer meer)
    else if(hit >= 0 && hit < PAGE_COUNT) gotoPage((int)hit);
    else if(uiMode==MODE_SETTINGS && SETTINGS_PAGE.onTouch) SETTINGS_PAGE.onTouch(g.x, g.y, false, true);
  } else if(g.type == GEST_SWIPE_LEFT && uiMode==MODE_SWIPE){
    gotoPage((curPage+1) % PAGE_COUNT);
  } else if(g.type == GEST_SWIPE_RIGHT && uiMode==MODE_SWIPE){
    gotoPage((curPage+PAGE_COUNT-1) % PAGE_COUNT);
  }

  renderCurrent();
}
