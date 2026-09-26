// ============================================================================
// User_Setup_CYD.h  -  TFT_eSPI configuratie voor ESP32-2432S028R ("CYD")
// ----------------------------------------------------------------------------
// DIT BESTAND HOORT NIET IN DE SKETCH TE DRAAIEN. TFT_eSPI leest zijn pin-
// configuratie op LIBRARY-niveau, niet per sketch. Installeer als volgt:
//
//   1. Zoek je Arduino-libraries-map, meestal:
//        Windows:  Documents\Arduino\libraries\TFT_eSPI
//   2. Hernoem daar het bestaande "User_Setup.h" naar bv. "User_Setup.h.orig"
//      (gewoon even opzij zetten, niet weggooien).
//   3. Kopieer DIT bestand naar die map en hernoem het naar "User_Setup.h".
//   4. Herstart de Arduino IDE (of sluit en heropen de sketch) zodat de
//      library de nieuwe instellingen inleest.
//
// De TOUCH-pinnen (XPT2046) staan hier NIET in - die lopen via de aparte
// XPT2046_Touchscreen-library met een eigen SPI-bus, zie touch.h/config.h.
// Reden: op deze specifieke board zit de touch-controller op fysiek andere
// MOSI/MISO/CLK-pinnen dan het scherm, en TFT_eSPI's ingebouwde touch-
// ondersteuning gaat er juist vanuit dat touch DEZELFDE SPI-bus deelt.
// ============================================================================

#define USER_SETUP_INFO "CYD_ESP32-2432S028R_ST7789"

// BELANGRIJK: dit exemplaar heeft een ST7789-paneel, GEEN ILI9341 (de meeste
// online "CYD"-gidsen gaan uit van ILI9341 - dat gaf hier een wit/leeg scherm
// omdat de verkeerde init-commando's verstuurd werden). Vastgesteld met een
// zelfgebouwd testscript op de echte hardware (2026-08-24) - zie de paste-
// ready instellingen die daaruit kwamen, hieronder 1-op-1 overgenomen.
#define ST7789_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1     // niet aangesloten op een GPIO, gaat naar EN (reset via board-reset)
// TFT_BL wordt NIET hier ingesteld - de backlight-pin (GPIO21) wordt in
// config.h (TFT_BL_PIN) via ledcAttach/ledcWrite door de sketch zelf aangestuurd,
// niet door TFT_eSPI.
#define TFT_BACKLIGHT_ON HIGH

// Kleurvolgorde: het testscript zei RGB, maar op het echte scherm kwamen
// rood en blauw verwisseld uit (rood tekent blauw, blauw tekent rood - groen
// klopte, wat past bij een R/B-swap) - dus BGR gebruiken i.p.v. wat het
// testscript rapporteerde. Vertrouw de zichtbare uitkomst boven de test.
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000   // ongebruikt hier (touch loopt via XPT2046_Touchscreen), onschadelijk laten staan

#define SUPPORT_TRANSACTIONS
