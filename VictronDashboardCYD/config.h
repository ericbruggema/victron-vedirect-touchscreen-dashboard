/* ============================================================================
   config.h  -  Alle instellingen op één plek (ESP32-2432S028R / "CYD")
   ----------------------------------------------------------------------------
   De TFT- en touch-PINNEN staan hier NIET in: TFT_eSPI leest zijn pinnen uit
   een apart User_Setup-bestand in de LIBRARY (buiten deze sketch), zie
   README_CYD.md. De touch-controller (XPT2046) zit op een fysiek APARTE SPI-bus
   en wordt via de XPT2046_Touchscreen-library aangestuurd (pinnen hieronder).
============================================================================ */
#ifndef CONFIG_H
#define CONFIG_H
#include "dashboard.h"

// ===== FIRMWARE =====
#define FW_VERSION "1.0-cyd"

// ===== BACKLIGHT =====
#define TFT_BL_PIN   21
#define BL_FREQ      5000
#define BL_RES        8
#define BL_MAX       255
// Ondergrens voor de NORMALE helderheid (dus niet de bewuste dim/uit-standen
// van de screensaver): voorkomt dat een tik net links van de schuifbalk op de
// Instellingen-pagina 'm op 0% zet en opslaat in NVS - zonder deze bodem zou
// je dan bij elke opstart met een volledig zwart scherm blijven zitten, zonder
// enige manier om via de (onzichtbare) UI terug naar Instellingen te tikken.
#define MIN_BRIGHTNESS_PCT 15

// ===== TOUCH (XPT2046, aparte SPI-bus t.o.v. het scherm) ====================
#define XPT2046_CS    33
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define TOUCH_SWIPE_MIN_PX     40   // minimale horizontale beweging voor een swipe
#define TOUCH_TAP_MAX_MOVE_PX  16   // maximale beweging om nog als "tik" te tellen

// ===== ONBOARD EXTRA'S =====
#define SPEAKER_PIN   26            // onboard luidspreker-footprint (alarm-piep)
// microSD zit op een EIGEN VSPI-bus (SCK18/MISO19/MOSI23), niet gedeeld met
// het scherm - bevestigd met het hardware-testscript (2026-08-24). SD.begin()
// gebruikt het standaard ESP32 "SPI"-object, dat toevallig al op die pinnen
// staat, dus verder is er niets aan te passen.
#define SD_CS_PIN      5

// ===== VE.DIRECT (alleen lezen) =============================================
// RX = Victron TX -> ESP RX. Elk apparaat een EIGEN RX-pin (continue live-data).
// GPIO27 is een GEDEELDE, software (bit-banged) TX-lijn naar de RX-ingang van
// BEIDE apparaten (simpele Y-splitsing, geen multiplexer nodig - zie ve_direct.h
// voor waarom dat elektrisch prima is). Alleen gebruikt voor de af-en-toe
// HEX-instellingen-opvraging; de live-databron staat er los van.
#define VEDIRECT_MPPT_RX     35     // input-only pin, prima voor RX
#define VEDIRECT_SHUNT_RX    22
#define VEDIRECT_SHARED_TX   27
#define VEDIRECT_CONFIGURED  ((VEDIRECT_MPPT_RX) >= 0 || (VEDIRECT_SHUNT_RX) >= 0)
#define VEDIRECT_WAIT_MS   15000    // bij opstart max wachttijd op LIVE-data -> anders DEMO

// ===== ACCUTYPE ==============================================================
#define BATTERY_LIFEPO4
//#define BATTERY_LEAD
#define BATTERY_CAPACITY_AH 300

// ===== VERBRUIK-KLEURDREMPELS (W) ===========================================
#define LOAD_GREEN_W    80
#define LOAD_YELLOW_W  150
#define LOAD_ORANGE_W  250

// ===== GEDRAG / TIMING =======================================================
#define DATA_INTERVAL       1000    // data/aggregatie-interval (ms)
#define NVS_SAVE_MS        60000    // records + rollende historie wegschrijven (ms)
// Opstartscherm + VE.Direct-zoeken zijn nu ÉÉN scherm (playBootAndSearch):
// is VE.Direct geconfigureerd, dan geldt VEDIRECT_WAIT_MS als duur (en stopt
// hij eerder zodra er data binnenkomt); BOOT_DURATION geldt alleen als er
// HELEMAAL geen VE.Direct-pin is ingesteld (dan is er niets om op te wachten,
// dus gewoon een korte animatie voordat DEMO start).
#define BOOT_DURATION       2500
#define DIM_TIMEOUT_MS     30000    // ms zonder aanraking -> dimmen ("screensaver")
#define OFF_TIMEOUT_MS    300000    // ms zonder aanraking -> scherm uit (backlight 0)
#define DIM_BRIGHTNESS        40    // backlight bij dimmen (0..255)
#define SCREENOFF_DEFAULT    true   // scherm mag na OFF_TIMEOUT_MS volledig uit (Instellingen: Ja/Nee)

// ===== ALARM (onboard speaker) ==============================================
#define ALARM_ENABLED 1
#define ALARM_FREQ_HZ 2000
#define ALARM_BEEP_MS  120

// ===== DEMO (alleen als er geen VE.Direct-pinnen actief zijn, of voor test) ==
// 0=zonnige dag, 1=bewolkt, 2=nacht, 3=bijna lege accu
#define SIM_SCENARIO        0
#define SIM_BATTERIES       2       // 1 of 2 accu's nabootsen
#define SIM_HOUR_REAL_MS 4000       // DEMO-klok: ms per gesimuleerd uur (kleiner = sneller)

// ===== APPARAAT-INFO (info-only; VE.Direct zendt dit niet uit) =============
#define MPPT_MODEL          "SmartSolar 100/30"
#define SHUNT_MODEL         "SmartShunt 500A"
#define MPPT_MAXCHARGE_A    "30 A"
#define SHUNT_CAPACITY_TXT  "200 Ah"

// ===== Accuprofielen =========================================================
// velden: name, capacity, vNomMin, vNomMax, vAlarmLow, vChargeMax, socWarn, socAlarm
// Beide profielen bestaan altijd (nodig voor de omschakelaar op de
// Instellingen-pagina); BATTERY_LIFEPO4/BATTERY_LEAD hierboven kiest alleen
// welke er bij een VERSE installatie (lege NVS) als standaard geldt.
BatteryProfile PROFILE_LEAD    = { "Loodzuur", BATTERY_CAPACITY_AH, 12.00, 12.70, 11.80, 14.40, 50, 40 };
BatteryProfile PROFILE_LIFEPO4 = { "LiFePO4",  BATTERY_CAPACITY_AH, 13.00, 13.60, 12.00, 14.60, 20, 10 };

#if defined(BATTERY_LEAD)
  BatteryProfile BATT = PROFILE_LEAD;
  #define BATTERY_DEFAULT_IDX 1
#elif defined(BATTERY_LIFEPO4)
  BatteryProfile BATT = PROFILE_LIFEPO4;
  #define BATTERY_DEFAULT_IDX 0
#else
  #error "Kies een accutype in config.h (BATTERY_LIFEPO4 of BATTERY_LEAD)"
#endif

void applyBatteryProfile(int idx){ BATT = (idx == 1) ? PROFILE_LEAD : PROFILE_LIFEPO4; }

#endif
