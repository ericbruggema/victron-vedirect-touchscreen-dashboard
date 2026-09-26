/* ============================================================================
   ve_direct.h  -  VE.Direct "Text mode" parser voor MPPT + SmartShunt (CYD)
   ----------------------------------------------------------------------------
   Non-blocking. Leest de seriele stroom (19200 8N1) die de Victron-apparaten
   uit zichzelf uitzenden, valideert per frame de checksum, en zet de waarden
   om naar de `live`-struct.

   PINNEN (config.h): elk apparaat heeft een EIGEN RX-pin (continue live-data,
   hardware-UART, alleen lezen). De HEX-instellingen-opvraging (optioneel, TX)
   loopt over een GEDEELDE lijn (VEDIRECT_SHARED_TX) die naar de RX-ingang van
   BEIDE apparaten gaat (simpele Y-splitsing).

   Waarom dat geen multiplexer nodig heeft: op die lijn zit maar EEN driver
   (de ESP32-GPIO, push-pull-uitgang); de RX-ingang van een VE.Direct-apparaat
   is passief/hoogohmig en duwt nooit zelf terug. Twee zulke ingangen door één
   uitgang laten aansturen is een fan-out, geen bus-conflict. Belangrijk: het
   is daarom bewust GEEN hardware-UART-TX-pin - de ESP32 GPIO-matrix laat maar
   1 peripheral tegelijk een pin fysiek aansturen (de LAATSTE begin() "wint"
   stilzwijgend), dus twee HardwareSerial-objecten dezelfde TX-pin geven zou
   de eerste zonder waarschuwing laten uitvallen. In plaats daarvan wordt hier
   met simpele digitalWrite()-timing (software-UART, alleen verzenden) op die
   pin gezonden - dat kán geen conflict geven omdat er maar één zender is.
   Antwoorden komen sowieso terug via de EIGEN RX-pin van elk apparaat, dus
   daar is niets gedeeld.
============================================================================ */
#ifndef VE_DIRECT_H
#define VE_DIRECT_H
#include "dashboard.h"
#include "config.h"

HardwareSerial MpptSerial(1);
HardwareSerial ShuntSerial(2);

enum VEDev { DEV_MPPT, DEV_SHUNT };

struct VEStream {
  HardwareSerial* ser; VEDev dev; bool active;
  int st; char label[20]; int li; char value[20]; int vi; uint8_t cs;
  unsigned long lastFrame;
  char hex[48]; int hxi;
};
static VEStream mpptStream  = { &MpptSerial,  DEV_MPPT,  false, 0, "", 0, "", 0, 0, 0, "", 0 };
static VEStream shuntStream = { &ShuntSerial, DEV_SHUNT, false, 0, "", 0, "", 0, 0, 0, "", 0 };

// ---- INSTELLINGEN-REGISTERS (VE.Direct HEX, ALLEEN LEZEN) -----------------
//  dev: 0 = MPPT, 1 = SmartShunt. value = raw * scale.
//  LET OP: register-ID's kunnen per model/firmware verschillen; verifieer ze
//  desnoods met de officiele "VE.Direct HEX Protocol"-documentatie.
VeReg veRegs[] = {
  { 0, 0xEDF0, "Max laad",     0.1f,  "A",   0, false },
  { 0, 0xEDF7, "Absorptie",    0.01f, "V",   0, false },
  { 0, 0xEDF6, "Float",        0.01f, "V",   0, false },
  { 1, 0x1000, "Capaciteit",   1.0f,  "Ah",  0, false },
  { 1, 0x1001, "Charged V",    0.1f,  "V",   0, false },
  { 1, 0x1002, "Tail",         0.1f,  "%",   0, false },
  { 1, 0x1003, "Chg detect",   1.0f,  "min", 0, false },
  { 1, 0x1004, "Laadeff.",     1.0f,  "%",   0, false },
  { 1, 0x1005, "Peukert",      0.01f, "",    0, false },
  { 1, 0x1006, "Stroomdrmpl",  0.01f, "A",   0, false },
};
const int VE_REG_COUNT = sizeof(veRegs)/sizeof(veRegs[0]);

static uint8_t hexNib(char c){
  if(c>='0'&&c<='9') return c-'0';
  if(c>='A'&&c<='F') return c-'A'+10;
  if(c>='a'&&c<='f') return c-'a'+10;
  return 0;
}
static void veStoreReg(VEDev dev, uint16_t reg, long raw){
  for(int i=0;i<VE_REG_COUNT;i++)
    if(veRegs[i].dev==(uint8_t)dev && veRegs[i].id==reg){
      veRegs[i].value = raw * veRegs[i].scale; veRegs[i].got = true; return;
    }
}
static void veHexResponse(VEStream &s){
  if(s.hxi < 9) return;
  if(s.hex[0] != '7' && s.hex[0] != '9') return;
  uint8_t bytes[24]; int nb=0;
  for(int i=1; i+1<s.hxi && nb<24; i+=2)
    bytes[nb++] = (hexNib(s.hex[i])<<4) | hexNib(s.hex[i+1]);
  if(nb < 4) return;
  uint8_t sum = hexNib(s.hex[0]);
  for(int i=0;i<nb;i++) sum += bytes[i];
  if((sum & 0xFF) != 0x55) return;
  uint16_t reg = bytes[0] | (bytes[1]<<8);
  int dataLen = nb - 3 - 1;
  if(dataLen < 1) return;
  long val=0; for(int i=0;i<dataLen && i<4;i++) val |= ((long)bytes[3+i]) << (8*i);
  veStoreReg(s.dev, reg, val);
}

struct MpptData { float battV, I, vpv; int ppv, cs; float yieldTodayWh; } mpptT;
struct ShuntData{ float battV, I; int P, soc; float consumedAh; int ttg;
                  float vs, vm, dm; int h4; } shuntT;

bool veConfigured = false;
bool veStale = true;
DevInfo mpptInfo = {0,0,""}, shuntInfo = {0,0,""};

static ChargeState mapCS(int cs){
  switch(cs){ case 3: return CS_BULK; case 4: return CS_ABSORPTION;
              case 5: case 7: return CS_FLOAT; default: return CS_OFF; }
}

static void veStore(VEDev dev, const char* l, const char* v){
  long n = atol(v);
  if(dev==DEV_MPPT){
    if(!strcmp(l,"V"))        mpptT.battV = n/1000.0;
    else if(!strcmp(l,"I"))   mpptT.I     = n/1000.0;
    else if(!strcmp(l,"VPV")) mpptT.vpv   = n/1000.0;
    else if(!strcmp(l,"PPV")) mpptT.ppv   = (int)n;
    else if(!strcmp(l,"CS"))  mpptT.cs    = (int)n;
    else if(!strcmp(l,"H20")) mpptT.yieldTodayWh = n*10.0;
    else if(!strcmp(l,"PID"))  mpptInfo.pid = strtol(v,NULL,0);
    else if(!strcmp(l,"FW"))   mpptInfo.fw  = atoi(v);
    else if(!strcmp(l,"SER#")) strncpy(mpptInfo.ser, v, 19);
  } else {
    if(!strcmp(l,"V"))        shuntT.battV = n/1000.0;
    else if(!strcmp(l,"I"))   shuntT.I     = n/1000.0;
    else if(!strcmp(l,"P"))   shuntT.P     = (int)n;
    else if(!strcmp(l,"SOC")) shuntT.soc   = (int)(n/10);
    else if(!strcmp(l,"CE"))  shuntT.consumedAh = -n/1000.0;
    else if(!strcmp(l,"TTG")) shuntT.ttg   = (int)n;
    else if(!strcmp(l,"VS"))  shuntT.vs    = n/1000.0;
    else if(!strcmp(l,"VM"))  shuntT.vm    = n/1000.0;
    else if(!strcmp(l,"DM"))  shuntT.dm    = n/10.0;
    else if(!strcmp(l,"H4"))  shuntT.h4    = (int)n;
    else if(!strcmp(l,"PID"))  shuntInfo.pid = strtol(v,NULL,0);
    else if(!strcmp(l,"FW"))   shuntInfo.fw  = atoi(v);
    else if(!strcmp(l,"SER#")) strncpy(shuntInfo.ser, v, 19);
  }
}

static void veProcess(VEStream &s){
  if(!s.active || !s.ser) return;
  while(s.ser->available()){
    char c = (char)s.ser->read();
    if(s.st==2){
      if(c=='\n' || c=='\r'){ s.hex[s.hxi]=0; veHexResponse(s); s.st=0; s.hxi=0; }
      else if(s.hxi < 46) s.hex[s.hxi++]=c;
      continue;
    }
    if(s.st==0 && s.li==0 && c==':'){ s.st=2; s.hxi=0; continue; }
    s.cs += (uint8_t)c;
    if(s.st==0){
      if(c=='\t'){ s.label[s.li]=0; s.st=1; s.vi=0; }
      else if(c=='\r'||c=='\n'){ s.li=0; }
      else if(s.li<19) s.label[s.li++]=c;
    } else {
      if(!strcmp(s.label,"Checksum")){
        if((s.cs & 0xFF)==0) s.lastFrame = millis();
        s.cs=0; s.st=0; s.li=0; s.vi=0;
      } else if(c=='\r'||c=='\n'){
        s.value[s.vi]=0; veStore(s.dev, s.label, s.value);
        s.st=0; s.li=0; s.vi=0;
      } else if(s.vi<19) s.value[s.vi++]=c;
    }
  }
}

void veDirectBegin(){
#if (VEDIRECT_MPPT_RX) >= 0
  MpptSerial.begin(19200, SERIAL_8N1, VEDIRECT_MPPT_RX, -1);   // RX-only
  mpptStream.active = true;
#endif
#if (VEDIRECT_SHUNT_RX) >= 0
  ShuntSerial.begin(19200, SERIAL_8N1, VEDIRECT_SHUNT_RX, -1);
  shuntStream.active = true;
#endif
#if (VEDIRECT_SHARED_TX) >= 0
  pinMode(VEDIRECT_SHARED_TX, OUTPUT);
  digitalWrite(VEDIRECT_SHARED_TX, HIGH);   // UART-rustlijn = HIGH
#endif
  veConfigured = (mpptStream.active || shuntStream.active);
}

// software-UART (alleen zenden) op de gedeelde TX-lijn, 19200 8N1
static void swWriteByte(uint8_t pin, uint8_t b){
  const uint16_t bitUs = 52;   // ~1000000/19200
  noInterrupts();
  digitalWrite(pin, LOW); delayMicroseconds(bitUs);
  for(int i=0;i<8;i++){ digitalWrite(pin, (b>>i)&1 ? HIGH : LOW); delayMicroseconds(bitUs); }
  digitalWrite(pin, HIGH); delayMicroseconds(bitUs);
  interrupts();
}
static void swSendLine(uint8_t pin, const char* s){ while(*s) swWriteByte(pin, (uint8_t)*s++); }

static void veHexGet(uint16_t reg){
#if (VEDIRECT_SHARED_TX) >= 0
  uint8_t d0=reg&0xFF, d1=(reg>>8)&0xFF, fl=0x00;
  uint8_t sum = 0x07 + d0 + d1 + fl;
  uint8_t chk = (0x55 - sum) & 0xFF;
  char msg[16]; snprintf(msg,sizeof(msg),":7%02X%02X%02X%02X\n", d0, d1, fl, chk);
  swSendLine(VEDIRECT_SHARED_TX, msg);
#endif
}

void veSeedDemo(){
  struct { uint16_t id; float v; } demo[] = {
    {0xEDF0,30.0f},{0xEDF7,28.4f},{0xEDF6,27.0f},
    {0x1000,200},{0x1001,26.4f},{0x1002,4.0f},{0x1003,3},{0x1004,99},{0x1005,1.05f},{0x1006,0.10f}
  };
  for(unsigned k=0;k<sizeof(demo)/sizeof(demo[0]);k++)
    for(int i=0;i<VE_REG_COUNT;i++) if(veRegs[i].id==demo[k].id){ veRegs[i].value=demo[k].v; veRegs[i].got=true; }
  mpptInfo.pid=0xA060; mpptInfo.fw=159;
  shuntInfo.pid=0xA389; shuntInfo.fw=412;
}

// vraagt periodiek het volgende register op (round-robin over BEIDE apparaten,
// op de gedeelde lijn - elk apparaat reageert alleen op zijn eigen registers)
void veHexPoll(){
  static unsigned long last=0; static int idx=0;
  if(VE_REG_COUNT==0) return;
  if(millis()-last < 800) return; last=millis();
  VeReg &r = veRegs[idx];
  idx = (idx+1) % VE_REG_COUNT;
  veHexGet(r.id);
}

void veDirectDrain(){
  veProcess(mpptStream);
  veProcess(shuntStream);
  unsigned long now = millis();
  bool mF = mpptStream.lastFrame  && (now - mpptStream.lastFrame  < 5000);
  bool sF = shuntStream.lastFrame && (now - shuntStream.lastFrame < 5000);
  veStale = !(mF || sF);
}

void veDirectPoll(){
  veProcess(mpptStream);
  veProcess(shuntStream);

  unsigned long now = millis();
  bool mFresh = mpptStream.lastFrame  && (now - mpptStream.lastFrame  < 5000);
  bool sFresh = shuntStream.lastFrame && (now - shuntStream.lastFrame < 5000);
  veStale = !(mFresh || sFresh);
  solarValid = mFresh;
  battValid  = sFresh;

  if(mFresh){
    live.pvV = mpptT.vpv; live.pvW = mpptT.ppv;
    live.pvA = live.pvV > 1 ? live.pvW/live.pvV : 0;
    live.state = mapCS(mpptT.cs);
  }
  if(sFresh){
    live.battV = shuntT.battV; live.battA = shuntT.I; live.battW = shuntT.P;
    live.soc = shuntT.soc;
    live.consumedAh  = shuntT.consumedAh;
    live.remainingAh = BATT.capacityAh - shuntT.consumedAh;
    live.cycles = shuntT.h4;
    if(shuntT.vm > 0.5){
      live.battCount = 2;
      live.batt1V = shuntT.vm;
      live.batt2V = shuntT.battV - shuntT.vm;
      live.midDev = shuntT.dm;
    } else if(shuntT.vs > 0.5){
      live.battCount = 2;
      live.batt1V = shuntT.battV;
      live.batt2V = shuntT.vs;
      live.midDev = 0;
    } else {
      live.battCount = 1;
      live.batt1V = shuntT.battV; live.batt2V = 0; live.midDev = 0;
    }
  } else if(mFresh){
    live.battV = mpptT.battV; live.battA = mpptT.I; live.battW = mpptT.battV*mpptT.I;
  }

  if(mFresh || sFresh){
    int chargeW = live.battW > 0 ? (int)live.battW : 0;
    live.loadW = live.pvW - (int)live.battW; if(live.loadW < 0) live.loadW = 0;
    live.directSolarW = live.pvW - chargeW; if(live.directSolarW < 0) live.directSolarW = 0;
  }
}

#endif
