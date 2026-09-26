/* ============================================================================
   touch.h  -  XPT2046 resistief touchscreen (aparte SPI-bus t.o.v. de TFT)
   ----------------------------------------------------------------------------
   Gebruikt de XPT2046_Touchscreen library (Paul Stoffregen) op een EIGEN
   SPIClass(HSPI)-instantie, want op de ESP32-2432S028R zit de touch-controller
   fysiek op andere MOSI/MISO/CLK-pinnen dan het scherm (zie config.h).

   Kalibratie: eenvoudige 2-punts (min/max) kalibratie - tik linksboven en
   rechtsonder een kruisje aan. Voor een axis-aligned resistief paneel is dat
   ruim voldoende (geen rotatie/scheefstand zoals bij een 5-punts matrix).
   Resultaat wordt in NVS bewaard ("touchcal") en bij opstart teruggelezen;
   is er niets bewaard, dan start automatisch de kalibratie-routine.
============================================================================ */
#ifndef TOUCH_H
#define TOUCH_H
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "dashboard.h"
#include "config.h"

static SPIClass touchSPI(HSPI);
static XPT2046_Touchscreen touchCtrl(XPT2046_CS, XPT2046_IRQ);

struct TouchCal { int16_t rawXMin, rawXMax, rawYMin, rawYMax; bool swapXY; };
static TouchCal tcal = { 300, 3800, 300, 3800, false };
static bool tcalLoaded = false;

static void touchSaveCal(){
  Preferences p;
  if(!p.begin("victron", false)) return;
  p.putBytes("touchcal", &tcal, sizeof(tcal));
  p.putBool("touchcalok", true);
  p.end();
}
static bool touchLoadCal(){
  Preferences p;
  if(!p.begin("victron", true)) return false;
  bool ok = p.getBool("touchcalok", false);
  if(ok) p.getBytes("touchcal", &tcal, sizeof(tcal));
  p.end();
  return ok;
}

void touchInit(){
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchCtrl.begin(touchSPI);
  touchCtrl.setRotation(1);            // zelfde landscape-rotatie als tft.setRotation(1)
  tcalLoaded = touchLoadCal();
}
bool touchCalibrated(){ return tcalLoaded; }

// ruwe ADC-waarden -> scherm-pixels, met de opgeslagen kalibratie
static void touchMap(int16_t rx, int16_t ry, int16_t &sx, int16_t &sy){
  int16_t xx = rx, yy = ry;
  if(tcal.swapXY){ int16_t t=xx; xx=yy; yy=t; }
  long mx = map((long)xx, tcal.rawXMin, tcal.rawXMax, 0, SCR_W - 1);
  long my = map((long)yy, tcal.rawYMin, tcal.rawYMax, 0, SCR_H - 1);
  sx = constrain((int)mx, 0, SCR_W - 1);
  sy = constrain((int)my, 0, SCR_H - 1);
}

struct TouchPoint { int16_t x, y; bool pressed; };

// gekalibreerd scherm-punt; pressed=false als er niets aangeraakt wordt
TouchPoint touchRead(){
  TouchPoint t = { 0, 0, false };
  if(!touchCtrl.touched()) return t;
  TS_Point p = touchCtrl.getPoint();
  t.pressed = true;
  touchMap(p.x, p.y, t.x, t.y);
  return t;
}

// blokkerende kalibratie-routine: 2 kruisjes aantikken (linksboven, rechtsonder).
// Heeft een TIMEOUT (30s per stap): als touch niet reageert (verkeerde pinnen/
// bedrading) loopt dit niet voor altijd vast, maar valt terug op een grove
// standaardkalibratie zodat je in elk geval verder komt en het probleem via
// de Serial Monitor kunt zien in plaats van een stil hangend scherm.
#define TOUCH_CAL_TIMEOUT_MS 30000
void touchRunCalibration(){
  const int MARGIN = 24;
  struct Target { int x, y; } targets[2] = {
    { MARGIN, MARGIN }, { SCR_W - MARGIN, SCR_H - MARGIN }
  };
  int16_t rawX[2], rawY[2];
  bool timedOut = false;

  for(int i = 0; i < 2 && !timedOut; i++){
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.print(i == 0 ? "Tik linksboven +" : "Tik rechtsonder +");
    int cx = targets[i].x, cy = targets[i].y;
    tft.drawLine(cx - 10, cy, cx + 10, cy, TFT_RED);
    tft.drawLine(cx, cy - 10, cx, cy + 10, TFT_RED);
    Serial.printf("[CAL] stap %d/2: wachten op tik bij scherm-punt (%d,%d)...\n", i+1, cx, cy);

    // wacht op loslaten-daarna-indrukken zodat een "sleep" van de vorige stap niet meetelt
    unsigned long t0 = millis();
    while(touchCtrl.touched()){ delay(20); if(millis()-t0 > TOUCH_CAL_TIMEOUT_MS){ timedOut=true; break; } }
    delay(150);
    unsigned long tWaitStart = millis(); unsigned long lastPrint = 0;
    while(!touchCtrl.touched()){
      delay(20);
      unsigned long elapsed = millis()-tWaitStart;
      if(elapsed - lastPrint > 3000){ lastPrint = elapsed; Serial.printf("[CAL]   ...nog geen aanraking gedetecteerd (%lus)\n", elapsed/1000); }
      if(elapsed > TOUCH_CAL_TIMEOUT_MS){ timedOut = true; break; }
    }
    if(timedOut){
      Serial.println("[CAL] TIMEOUT - geen touch-signaal ontvangen. Controleer XPT2046-bedrading/pinnen in config.h (CS/IRQ/MOSI/MISO/CLK).");
      break;
    }
    TS_Point p = touchCtrl.getPoint();
    rawX[i] = p.x; rawY[i] = p.y;
    Serial.printf("[CAL]   tik ontvangen: raw x=%d y=%d\n", p.x, p.y);
    while(touchCtrl.touched()) delay(20);
    delay(200);
  }

  if(timedOut){
    // grove standaardwaarden (tcal is al met deze default geïnitialiseerd) zodat
    // de rest van de sketch normaal doorstart i.p.v. voor altijd te blijven hangen
    Serial.println("[CAL] kalibratie NIET voltooid - standaardwaarden gebruikt, scherm werkt mogelijk verkeerd gepositioneerd tot je opnieuw kalibreert via Instellingen.");
    tcalLoaded = false;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(10, 100); tft.print("Geen touch gevonden");
    delay(1500);
    return;
  }

  tcal.rawXMin = min(rawX[0], rawX[1]);
  tcal.rawXMax = max(rawX[0], rawX[1]);
  tcal.rawYMin = min(rawY[0], rawY[1]);
  tcal.rawYMax = max(rawY[0], rawY[1]);
  tcal.swapXY  = false;
  tcalLoaded = true;
  touchSaveCal();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(60, 110);
  tft.print("Kalibratie OK");
  delay(700);
}

// ---- GEBAAR-HERKENNING (aanroepen vanuit loop()) ---------------------------
// Levert per loop-tick een classificatie: tik (met coördinaten), swipe-links,
// swipe-rechts, of "bezig"/niets. Debounced op indruk/loslaat-momenten.
enum GestureType { GEST_NONE, GEST_TAP, GEST_SWIPE_LEFT, GEST_SWIPE_RIGHT, GEST_DOWN, GEST_UP };
struct Gesture { GestureType type; int16_t x, y; };

Gesture touchPoll(){
  static bool wasPressed = false;
  static int16_t downX = 0, downY = 0, lastX = 0, lastY = 0;
  static bool moved = false;
  Gesture g = { GEST_NONE, 0, 0 };

  TouchPoint t = touchRead();
  if(t.pressed){
    lastX = t.x; lastY = t.y;
    if(!wasPressed){
      wasPressed = true; downX = t.x; downY = t.y; moved = false;
      g.type = GEST_DOWN; g.x = t.x; g.y = t.y;
    } else if(abs(t.x - downX) > TOUCH_TAP_MAX_MOVE_PX || abs(t.y - downY) > TOUCH_TAP_MAX_MOVE_PX){
      moved = true;                          // aan het slepen: wacht op loslaten voor het oordeel
    }
    return g;
  }
  if(!t.pressed && wasPressed){
    wasPressed = false;
    int dx = lastX - downX;                  // netto horizontale beweging over de hele aanraking
    g.x = downX; g.y = downY;
    if(!moved || abs(dx) < TOUCH_SWIPE_MIN_PX) g.type = GEST_TAP;
    else g.type = (dx < 0) ? GEST_SWIPE_LEFT : GEST_SWIPE_RIGHT;
    return g;
  }
  return g;
}

#endif
