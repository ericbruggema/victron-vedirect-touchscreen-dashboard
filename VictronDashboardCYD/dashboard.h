/* ============================================================================
   dashboard.h  -  Gedeelde definities (ESP32-2432S028R / "CYD" versie)
   ----------------------------------------------------------------------------
   Eén vast ontwerp (geen thema's meer), touchscreen-navigatie i.p.v. PIR/knop,
   geen RTC: alle "dag"-statistiek is een ROLLEND 24-uursvenster t.o.v. de
   uptime, niet een kalenderdag. simHour/simHourF geven de positie binnen dat
   venster (0..24) - in DEMO versneld voor het testen, in LIVE gelijk aan de
   echte verstreken tijd. day.weekYieldWh was al een rollend 7-slots-buffer en
   verandert dus niet van betekenis.
============================================================================ */
#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

#define SCR_W 320
#define SCR_H 240

enum ChargeState { CS_OFF, CS_BULK, CS_ABSORPTION, CS_FLOAT };
enum DataSource  { SRC_DEMO, SRC_LIVE };
enum AlertLevel  { ALERT_OK, ALERT_WARN, ALERT_ALARM };

// ---- DATAMODELLEN (zelfde velden als het ESP32-D-ontwerp) -----------------
struct Live {
  float pvV, pvA; int pvW; ChargeState state;
  float battV, battA, battW; int soc;
  float remainingAh, consumedAh;
  int   loadW; int directSolarW;
  int   battCount;          // 1 of 2 accu's gedetecteerd
  float batt1V, batt2V;     // individuele accuspanningen
  float midDev;             // midpoint-afwijking in %
  int   cycles;             // aantal laadcycli (H4 van de shunt)
};
struct Day {
  float pvW_sum; long pvW_n; int pvW_max;
  float battV_min, battV_max;
  float battA_min, battA_max;
  float battW_min, battW_max;
  float soc_sum; long soc_n; int soc_min, soc_max;
  int   load_max;
  float yieldWh, loadWh, chargedWh, dischargedWh;
  float phaseSec[3];                  // bulk, absorptie, float (seconden, rollend 24u)
  float pvHour[24]; long pvHourN[24];  // rollend 24u-venster, index = huidig "simHour"
  int   socHour[24]; bool socHourSet[24];
  int   daysLogged; float weekYieldWh[7];   // rollend, schuift bij elke rollover
  float monthYieldWh; int monthDays;        // rollend (~30 rollovers), geen kalendermaand
};
struct Records {
  int maxPvW, maxLoadW, maxChargeW, maxDischargeW;
  float maxChargeA, maxDischargeA, maxBattV, minBattV;
  int maxSoc, minSoc;
  float bestDayWh, maxDayLoadWh;
  float totalChargedWh, totalDischargedWh;
  float lifetimeDischargedAh;
};
struct BatteryProfile {
  const char* name;
  float capacityAh;
  float vNomMin, vNomMax;
  float vAlarmLow;
  float vChargeMax;
  int   socWarn, socAlarm;
};
extern BatteryProfile BATT;                     // actief profiel (mutable, zie applyBatteryProfile)
extern BatteryProfile PROFILE_LIFEPO4, PROFILE_LEAD;
void applyBatteryProfile(int idx);              // 0 = LiFePO4, 1 = Lood

// ---- APP-INSTELLINGEN (Instellingen-pagina, bewaard in NVS) ----------------
struct AppSettings { int brightnessPct; bool screenOffEnabled; int battProfileIdx; };
extern AppSettings appSettings;
void settingsLoad();
void settingsSave();

extern Live    live;
extern Day     day;
extern Records rec;
extern bool    nvsOk, nvsFull;
extern int     demoGainPct;
extern bool    sdPresent;

extern DataSource dataSource;
extern bool       veConfigured;
extern bool       veStale;
extern bool       solarValid;
extern bool       battValid;
extern int        systemV;
extern float      vMult;

struct DevInfo { long pid; int fw; char ser[20]; };
extern DevInfo mpptInfo, shuntInfo;

struct VeReg { uint8_t dev; uint16_t id; const char* label; float scale; const char* unit; float value; bool got; };
extern VeReg veRegs[]; extern const int VE_REG_COUNT;
void veHexPoll();
void veSeedDemo();
void veDirectBegin();
void veDirectPoll();
void veDirectDrain();

extern int socTrend;             // -1 dalend, 0 stabiel, +1 stijgend

// ---- ROLLENDE KLOK (geen RTC) ----------------------------------------------
extern int   simHour;            // 0..23: positie binnen het rollende 24u-venster
extern float simHourF;           // idem, met fractie
String fmtUptime();              // "3u 42m" sinds boot

String fmtW(int w);
const char* trendArrow();
uint16_t vCol(bool valid, uint16_t c);
String   vTxt(bool valid, const String &s);

uint16_t socColor(int s);          // groen/amber/rood volgens BATT.socWarn/socAlarm
uint16_t loadColor(int w);         // groen/amber/amberMid/rood volgens LOAD_*_W (config.h)
uint16_t flowColor(float w);
uint16_t mpptColor(ChargeState s);
const char* mpptText(ChargeState s);
AlertLevel systemAlertLevel(const char** reasonOut);

// ---- CONTENT-GEBIED ---------------------------------------------------------
// Elke pagina tekent zijn inhoud BINNEN dit gebied; header/footer/navstipjes
// worden door het framework zelf getekend (pagina's hoeven dat niet te doen).
#define CONTENT_TOP    28
#define CONTENT_BOTTOM (SCR_H - 16)
// Terug naar de oorspronkelijke SCR_H-16: de tussentijdse extra marge
// (SCR_H-34, later SCR_H-24) was bedoeld tegen een vermoede scherm-crop die
// achteraf gewoon de ST7789/BGR-configuratiefout bleek - dat is inmiddels
// opgelost. Die marge kostte de dashboard-kaarten alleen maar onnodig veel
// ruimte, met tekst die tegen de onderkant aan liep als gevolg. Elke pagina
// rekent al relatief t.o.v. CONTENT_BOTTOM, dus deze ene waarde verschuift
// alle pagina-inhoud automatisch mee.)

// ---- PAGINA'S ---------------------------------------------------------------
// PAGE_DASHBOARD..PAGE_SYSTEEM zijn de 7 swipe-pagina's (in deze volgorde).
// PAGE_SETTINGS en PAGE_PICKER liggen BUITEN de swipe-cyclus (alleen via het
// grid-icoontje resp. via een kaart-tik te bereiken).
enum PageId {
  PAGE_DASHBOARD = 0, PAGE_ZON, PAGE_ACCU, PAGE_ENERGIE,
  PAGE_HISTORIE, PAGE_APPARATEN, PAGE_SYSTEEM,
  PAGE_COUNT,                       // = 7, aantal swipe-pagina's
  PAGE_SETTINGS, PAGE_PICKER,
  PAGE_NONE                         // "geen tikzone geraakt"
};
typedef void (*PageFn)(bool first);
// touch-callback: scherm-coordinaten, of het net ingedrukt/losgelaten is
typedef void (*TouchFn)(int x, int y, bool justPressed, bool justReleased);
struct PageDef { const char* title; PageFn fn; TouchFn onTouch; };

extern PageDef PAGES[PAGE_COUNT];      // de 7 swipe-pagina's, ingevuld door pages_*.h
extern PageDef SETTINGS_PAGE;
extern PageDef PICKER_PAGE;

// ---- KLEUREN (vast, geen thema's meer) -------------------------------------
struct ColorSet {
  uint16_t bg, card, border, text, muted;
  uint16_t amber, green, coral, amberMid, red, blue, purple;
};
extern ColorSet COL;
void colorsInit();

// ---- AANRAAKZONES (tik op een kaart/rij -> naar een pagina) ----------------
void clearTapZones();
void addTapZone(int x, int y, int w, int h, PageId target);
PageId hitTestZones(int x, int y);   // PAGE_NONE als niets geraakt is

// ---- WIDGET-TOOLKIT (implementatie in widgets.h) ---------------------------
void resetWidgetCache();             // wist de diff-cache (bij paginawissel)

// tekst, gecached op "id": hertekent alleen bij wijziging. Elke aanroepplek
// binnen een pagina moet een UNIEK id (0..59) gebruiken - zie widgets.h voor
// de gereserveerde chrome-ids (60-64).
void wValue (int id, int x,  int y, uint8_t fontSize, uint16_t col, const String &val);
void wValueR(int id, int xr, int y, uint8_t fontSize, uint16_t col, const String &val);
void wValueC(int id, int cx, int y, uint8_t fontSize, uint16_t col, const String &val);
// eenmalige statische tekst (alleen aanroepen als first==true, niet gecached)
void wText(int x, int y, uint8_t fontSize, uint16_t col, const String &s);
void wDivider(int x, int y, int w);

// iconen (vector, getekend in de meegegeven kleur, r = straal in pixels)
typedef void (*IconFn)(int cx, int cy, int r, uint16_t col);
void iconSun(int cx, int cy, int r, uint16_t col);
void iconMoon(int cx, int cy, int r, uint16_t col);
void iconBattery(int cx, int cy, int r, uint16_t col);
void iconBolt(int cx, int cy, int r, uint16_t col);
void iconArrowRight(int cx, int cy, int r, uint16_t col);
void iconChevronRight(int cx, int cy, int r, uint16_t col);
void iconChevronLeft(int cx, int cy, int r, uint16_t col);
void iconGrid(int cx, int cy, int r, uint16_t col);
void iconHouse(int cx, int cy, int r, uint16_t col);
void iconClock(int cx, int cy, int r, uint16_t col);

// dashboard-kaart (icoon+ring+grote waarde+subwaarde+voettekst), gecached op id
struct CardData {
  const char* label;
  String      bigValue;
  String      subValue;
  const char* footLabel;
  String      footValue;
  uint16_t    accent;
  int         ringPct;          // 0..100, -1 = geen ring tekenen
  IconFn      icon;
};
void wCard(int id, int x, int y, int w, int h, const CardData &d);

// kleine statistiek-tegel (label boven, waarde eronder)
void wStatTile(int id, int x, int y, int w, int h, const char* label, const String &value, uint16_t valueColor);

// label-waarde rij (links label, rechts waarde)
void wRow(int id, int x, int y, int w, const char* label, const String &value, uint16_t valueColor);

// grote ring-gauge met tekst in het midden (Accu-detail SoC)
void wBigRing(int id, int cx, int cy, int r, int strokeW, uint16_t col, int pct, const String &centerBig, const String &centerSmall);

// staafgrafiek (auto-schalend), optioneel 1 staaf gemarkeerd
void wBarGraph(int id, int x, int y, int w, int h, const float* values, int n, uint16_t color, int highlightIdx, uint16_t highlightColor);
// lijn-grafiek (sparkline), waarden 0..100 (bv. SoC)
void wSparkline(int id, int x, int y, int w, int h, const float* values, int n, uint16_t color);
// gesegmenteerde balk (bv. laadfase-verdeling), fracties tellen op tot 1.0
void wSegmentBar(int id, int x, int y, int w, int h, const float* fractions, int n, const uint16_t* colors);

// status-pil (afgeronde badge met tekst, bv. "Absorptie")
void wStatusPill(int id, int x, int y, uint16_t bg, uint16_t textCol, const char* txt);

// instellingen-widgets
void wToggle(int id, int x, int y, bool on);
void wSlider(int id, int x, int y, int w, int pct);
void wSegmented(int id, int x, int y, const char* optA, const char* optB, int selected);

// paginachrome (door het framework aangeroepen, niet door pagina's zelf)
void drawStatusBar();                 // dashboard: klok + MPPT/SHUNT-stip + grid-icoon
void drawSubHeader(const char* title);// detail: terug-pijl + titel + grid-icoon
void drawNavDots(int activeIndex);    // positie-stipjes onderaan

#endif
