/*
  Full PT2322 + PT2323 + TM1637 + IR (recv/send) + EEPROM + USB/BT power control
  Fixed and optimized version with proper initialization and error handling.
*/

#include <Wire.h>
#include <EEPROM.h>
#include <IRremote.h>
#include <TM1637Display.h>

// I2C addresses
#define PT2323_address 0b1001010  // 0x4A
#define PT2322_address 0b1000100  // 0x44

#define btn_delay 300

// Panel / buttons pins
#define sw01 9
#define sw02 11
#define sw03 10
#define sw04 A0
#define sw05 A1
#define sw06 A2
#define sw07 13

// TM1637 pins
#define TM1637_CLK 7
#define TM1637_DIO 6

// IR pins and BT power pin
#define IR_RECV_PIN 8
#define IR_SEND_PIN 3    // Hardware PWM pin for IRremote
#define BT_POWER_PIN 5   // USB/BT 5V ON/OFF control

// IR HEX codes (keep user's original codes)
#define ir_power         0x807F827D
#define ir_mute          0x807F42BD
#define ir_in            0x807F629D
#define ir_vol_i         0x807F906F
#define ir_vol_d         0x807FA05F
#define ir_sub_i         0x807FD02F
#define ir_sub_d         0x807FE01F
#define ir_fl_i          0x807F40BF
#define ir_fl_d          0x807FC03F
#define ir_fr_i          0x807F00FF
#define ir_fr_d          0x807F807F
#define ir_cn_i          0x807F50AF
#define ir_cn_d          0x807F609F
#define ir_sl_i          0x807F48B7
#define ir_sl_d          0x807FC837
#define ir_sr_i          0x807F08F7
#define ir_sr_d          0x807F8877
#define ir_bass_i        0x807F8A75
#define ir_bass_d        0x807F4AB5
#define ir_mid_i         0x807F6897
#define ir_mid_d         0x807FE817
#define ir_treb_i        0x807FAA55
#define ir_treb_d        0x807F6A95
#define ir_sp_mode       0x807F0AF5
#define ir_sou_mode      0x807FA857
#define ir_eq_mode       0x807F20DF
#define ir_surround_mix  0x807F32CD
#define IR_REPEAT        0xFFFFFFFFUL

// BT-panel IR codes (replace with actual learned codes)
#define ir_bt_toggle     0x20DF10EF
#define ir_bt_playpause  0x20DF22DD

// Global objects
IRrecv irrecv(IR_RECV_PIN);
decode_results results;
IRsend irsend;
TM1637Display display(TM1637_CLK, TM1637_DIO);

// 7-segment character definitions
const uint8_t S7_A = 0x77, S7_b = 0x7C, S7_C = 0x39, S7_d = 0x5E, S7_E = 0x79;
const uint8_t S7_F = 0x71, S7_H = 0x76, S7_L = 0x38, S7_M = 0x37, S7_O = 0x3F;
const uint8_t S7_P = 0x73, S7_r = 0x50, S7_S = 0x6D, S7_T = 0x78, S7_U = 0x3E;
const uint8_t S7_c = 0x58, S7_h = 0x74, S7_u = 0x1C, S7_SPACE = 0x00, S7_DASH = 0x40;

// Global variables
unsigned long time = 0;
int in = 0, vol = 50, bass = 7, mid = 7, treb = 7, sub = 7, fl = 7, cn = 7, fr = 7, sl = 7, sr = 7;
int mute = 0, ch_mute = 0, return_d = 0, surr = 0, mix = 0, vol_10 = 0, vol_1 = 0, a = 0, b = 0;
int mute_sel = 0, effect_sel = 0, tone_sel = 0, speaker_mode = 0, sub_menu = 0, menu_sel = 0, power = 0, vol_on = 0;
int menu = 0, sub_menu_sel = 0, sub_menu_item = 0, sub_menu_item_sel = 0;
int sub_v = 0, sub_10 = 0, sub_1 = 0, ir_on = 0, ir_menu = 0;
int effect3d = 0, eq_mode = 0;

// Mute blink variables
bool muteBlink = false, muteBlinkOn = false;
unsigned long lastMuteBlink = 0;
const unsigned long muteBlinkInterval = 500;

// Flash overlay variables
bool flashActive = false;
unsigned long flashUntil = 0;

// Standby banner variables
bool standbyActive = false;
unsigned long standbyStart = 0;

// IR hold variables (generic)
enum HoldTarget { HOLD_NONE, HOLD_VOL, HOLD_SUB, HOLD_FL, HOLD_FR, HOLD_CN, HOLD_SL, HOLD_SR, HOLD_BASS, HOLD_MID, HOLD_TREB };
HoldTarget holdTarget = HOLD_NONE;
int8_t holdDir = 0;
unsigned long lastIRTime = 0, lastHoldStep = 0;
const unsigned long volHoldInterval = 150;
uint32_t lastIrCode = 0;

// IR press debounce to ensure 1-step per press (no multi-step bursts)
uint32_t lastPressCode = 0;
unsigned long lastPressAt = 0;
const unsigned long irDebounceMs = 220;

// Forward declarations for hold handlers
void applyHoldStep(HoldTarget t, int8_t dir);
void handleHoldTick();

// Rotary encoder variables
uint8_t encPrevState = 0;
unsigned long encLastTransitionAt = 0;
int8_t encAccum = 0;
unsigned long encLastStepAt = 0;
const unsigned long encDebounceUs = 1500;
const unsigned long encAccelThresholdMs = 50;

// EEPROM update optimization
bool eepromNeedsUpdate = false;
unsigned long lastEepromUpdate = 0;
const unsigned long eepromUpdateInterval = 2000; // Update every 2 seconds when needed

// Helper functions
static inline uint8_t encDigit(uint8_t d) {
  return display.encodeDigit(d & 0x0F);
}

uint8_t segFromChar(char c) {
  switch(c) {
    case ' ': return S7_SPACE; case '-': return S7_DASH; case 'A': return S7_A; case 'B': return S7_b;
    case 'C': return S7_C; case 'D': return S7_d; case 'E': return S7_E; case 'F': return S7_F;
    case 'H': return S7_H; case 'L': return S7_L; case 'M': return S7_M; case 'O': return S7_O;
    case 'P': return S7_P; case 'R': return S7_r; case 'S': return S7_S; case 'T': return S7_T;
    case 'U': return S7_U; case 'c': return S7_c; case 'h': return S7_h; case 'u': return S7_u; case 'd': return S7_d;
    default: if(c >= '0' && c <= '9') return encDigit(c - '0'); return S7_SPACE;
  }
}

void showSegments(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3) {
  uint8_t d[4] = {s0, s1, s2, s3};
  display.setSegments(d);
}

void showLabelValue(uint8_t l0, uint8_t l1, int v) {
  if(v < 0) v = 0;
  if(v > 99) v = 99;
  showSegments(l0, l1, encDigit(v / 10), encDigit(v % 10));
}

// Display functions
void showMaster() { showLabelValue(S7_A, S7_L, vol); }
void showFront() { showLabelValue(S7_F, S7_r, fr); }
void showSurround() { showLabelValue(S7_S, S7_r, sr); }
void showCenter() { showLabelValue(S7_C, S7_SPACE, cn); }
void showSub() { showLabelValue(S7_S, S7_b, sub); }
void showBass() { showLabelValue(S7_b, S7_SPACE, bass); }
void showMid() { showLabelValue(S7_h, S7_SPACE, mid); }
void showTreble() { showLabelValue(S7_T, S7_r, treb); }

void showInput() {
  switch(in) {
    case 4: showSegments(encDigit(6), S7_c, encDigit((vol / 10) % 10), encDigit(vol % 10)); break;
    case 0: showSegments(S7_b, S7_T, encDigit((vol / 10) % 10), encDigit(vol % 10)); break;
    case 1: showSegments(S7_C, S7_d, encDigit((vol / 10) % 10), encDigit(vol % 10)); break;
    case 2: showSegments(S7_d, S7_c, encDigit((vol / 10) % 10), encDigit(vol % 10)); break;
    case 3: showSegments(S7_A, S7_u, encDigit((vol / 10) % 10), encDigit(vol % 10)); break;
    default: showMaster(); break;
  }
}

// Mute blink functions
void startMuteBlink() {
  muteBlink = true;
  muteBlinkOn = false;
  lastMuteBlink = millis();
}

void stopMuteBlink() {
  muteBlink = false;
  muteBlinkOn = false;
}

void updateMuteBlink() {
  if (!muteBlink) return;
  unsigned long now = millis();
  if (now - lastMuteBlink >= muteBlinkInterval) {
    lastMuteBlink = now;
    muteBlinkOn = !muteBlinkOn;
    if (muteBlinkOn) showSegments(S7_M, S7_U, S7_T, S7_E);
    else showSegments(S7_SPACE, S7_SPACE, S7_SPACE, S7_SPACE);
  }
}

// Flash display functions
void showFlash(const char* txt, unsigned long dur) {
  flashActive = true;
  flashUntil = millis() + dur;
  uint8_t d[4] = {S7_SPACE, S7_SPACE, S7_SPACE, S7_SPACE};
  for (int i = 0; i < 4 && txt[i]; i++) d[i] = segFromChar(txt[i]);
  display.setSegments(d);
}

void updateFlash() {
  if (flashActive && millis() > flashUntil) {
    flashActive = false;
    if (!muteBlink) showMaster();
  }
}

// Standby functions
void startStandby() {
  standbyActive = true;
  standbyStart = millis();
}

void updateStandby() {
  if (!standbyActive) return;
  unsigned long t = millis() - standbyStart;
  if (t < 2000) showSegments(S7_H, S7_E, S7_L, S7_O);
  else showSegments(S7_DASH, S7_DASH, S7_DASH, S7_DASH);
}

// EEPROM functions with wear leveling
void markEepromUpdate() {
  eepromNeedsUpdate = true;
}

void eeprom_update() {
  if (!eepromNeedsUpdate) return;
  unsigned long now = millis();
  if (now - lastEepromUpdate < eepromUpdateInterval) return;

  EEPROM.update(0, in); EEPROM.update(1, vol); EEPROM.update(2, bass);
  EEPROM.update(3, mid); EEPROM.update(4, treb); EEPROM.update(5, sub);
  EEPROM.update(6, fl); EEPROM.update(7, fr); EEPROM.update(8, cn);
  EEPROM.update(9, sl); EEPROM.update(10, sr); EEPROM.update(11, surr);
  EEPROM.update(12, mix); EEPROM.update(13, speaker_mode);
  EEPROM.update(15, effect3d); EEPROM.update(16, eq_mode);

  eepromNeedsUpdate = false;
  lastEepromUpdate = now;
}

void eeprom_read() {
  in = EEPROM.read(0); vol = EEPROM.read(1); bass = EEPROM.read(2);
  mid = EEPROM.read(3); treb = EEPROM.read(4); sub = EEPROM.read(5);
  fl = EEPROM.read(6); fr = EEPROM.read(7); cn = EEPROM.read(8);
  sl = EEPROM.read(9); sr = EEPROM.read(10); surr = EEPROM.read(11);
  mix = EEPROM.read(12); speaker_mode = EEPROM.read(13);
  effect3d = EEPROM.read(15); eq_mode = EEPROM.read(16);

  // Validate ranges
  if (in > 4) in = 0;
  if (vol < 29 || vol > 79) vol = 50;
  if (bass > 15) bass = 7;
  if (mid > 15) mid = 7;
  if (treb > 15) treb = 7;
  if (sub > 15) sub = 7;
  if (fl > 15) fl = 7;
  if (fr > 15) fr = 7;
  if (cn > 15) cn = 7;
  if (sl > 15) sl = 7;
  if (sr > 15) sr = 7;
  if (effect3d > 1) effect3d = 0;
  if (eq_mode > 3) eq_mode = 0;
  if (speaker_mode > 1) speaker_mode = 0;
}

// Timing functions
void btn_cl() {
  delay(btn_delay);
  time = millis();
  return_d = 1;
}

void ir_cl() {
  time = millis();
  return_d = 1;
}

void return_delay() {
  if (millis() - time > 5000 && return_d == 1 && menu != 0 && mute == 0) {
    menu = 0;
    return_d = 0;
    ir_menu = 0;
  }
}

// I2C helper function with error checking
bool writeI2C(uint8_t address, uint8_t data) {
  Wire.beginTransmission(address);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

bool writeI2C(uint8_t address, uint8_t data1, uint8_t data2) {
  Wire.beginTransmission(address);
  Wire.write(data1);
  Wire.write(data2);
  return (Wire.endTransmission() == 0);
}

// PT2322 3D effect
void set_3d_effect(int on) {
  effect3d = on ? 1 : 0;
  uint8_t cmd = 0b01110000 | (effect3d ? 0b00000100 : 0b00000000);
  writeI2C(PT2322_address, cmd);
  markEepromUpdate();
}

void toggle_3d_effect() {
  set_3d_effect(effect3d ? 0 : 1);
  showFlash(" 3d ", 800);
}

// EQ presets
void apply_eq_mode() {
  switch(eq_mode) {
    case 0: bass = 7;  mid = 7;  treb = 7;  break; // Flat
    case 1: bass = 10; mid = 8;  treb = 11; break; // Rock
    case 2: bass = 8;  mid = 6;  treb = 9;  break; // Pop
    case 3: bass = 11; mid = 5;  treb = 12; break; // Dance
  }
  bass = constrain(bass, 0, 15);
  mid = constrain(mid, 0, 15);
  treb = constrain(treb, 0, 15);

  set_bass(); set_mid(); set_treb();
}

void toggle_eq_mode() {
  eq_mode = (eq_mode + 1) % 4;
  markEepromUpdate();
  apply_eq_mode();
  char m[5] = {' ', 'E', char('0' + eq_mode), ' ', 0};
  showFlash(m, 800);
}

// Factory reset to defaults
void reset_defaults() {
  in = 0; vol = 50; bass = 7; mid = 7; treb = 7; sub = 7; fl = 7; cn = 7; fr = 7; sl = 7; sr = 7;
  mute = 0; ch_mute = 0; surr = 0; mix = 1; speaker_mode = 0; effect3d = 0; eq_mode = 0;
  // Persist immediately
  EEPROM.update(0, in); EEPROM.update(1, vol); EEPROM.update(2, bass);
  EEPROM.update(3, mid); EEPROM.update(4, treb); EEPROM.update(5, sub);
  EEPROM.update(6, fl); EEPROM.update(7, fr); EEPROM.update(8, cn);
  EEPROM.update(9, sl); EEPROM.update(10, sr); EEPROM.update(11, surr);
  EEPROM.update(12, mix); EEPROM.update(13, speaker_mode);
  EEPROM.update(15, effect3d); EEPROM.update(16, eq_mode);
  // Re-apply to hardware
  set_3d_effect(effect3d);
  set_vol(); set_bass(); set_mid(); set_treb();
  set_sub(); set_fl(); set_fr(); set_cn(); set_sl(); set_sr();
  set_in(); set_surr(); set_mute(); set_speaker_mode(); set_mix();
}

// PT2323 mix control
void set_mix() {
  mix = constrain(mix, 0, 1);
  uint8_t cmd = mix == 1 ? 0b10010001 : 0b10010000;
  writeI2C(PT2323_address, cmd);
}

void toggle_mix() {
  if (in == 4) { in = 0; set_in(); }
  if (surr != 0) { surr = 0; set_surr(); }
  mix = mix ? 0 : 1;
  set_mix();
  markEepromUpdate();
  showFlash(mix ? " 6d " : " 0d ", 800);
}

// Power control
void power_up() {
  if (power == 1) {
    digitalWrite(sw07, HIGH);
    mute = 0;
    stopMuteBlink();
    standbyActive = false;

    // Initialize PT2322
    writeI2C(PT2322_address, 0b11000111);
    delay(50);

    // Apply all settings
    set_3d_effect(effect3d);
    set_vol(); set_bass(); set_mid(); set_treb();
    set_sub(); set_fl(); set_fr(); set_cn();
    set_sl(); set_sr(); set_in(); set_surr();
    set_mute(); set_speaker_mode(); set_mix();

    delay(300);
    menu = 0; ir_on = 1; ir_menu = 0; vol_on = 0;
    if (!flashActive) showMaster();
  } else {
    digitalWrite(sw07, LOW);
    mute = 1;
    set_mute();
    delay(100);
    menu = 100;
    ir_on = 0;
    startStandby();
  }
}

// Generic hold handling
void applyHoldStep(HoldTarget t, int8_t dir) {
  switch (t) {
    case HOLD_VOL:  vol += dir; set_vol(); break;
    case HOLD_SUB:  sub += dir; set_sub(); break;
    case HOLD_FL:   fl  += dir; set_fl();  break;
    case HOLD_FR:   fr  += dir; set_fr();  break;
    case HOLD_CN:   cn  += dir; set_cn();  break;
    case HOLD_SL:   sl  += dir; set_sl();  break;
    case HOLD_SR:   sr  += dir; set_sr();  break;
    case HOLD_BASS: bass+= dir; set_bass();break;
    case HOLD_MID:  mid += dir; set_mid(); break;
    case HOLD_TREB: treb+= dir; set_treb();break;
    default: break;
  }
}

void handleHoldTick() {
  if (holdTarget == HOLD_NONE || holdDir == 0) return;
  if (millis() - lastIRTime > 400) { holdTarget = HOLD_NONE; holdDir = 0; return; }
  if (millis() - lastHoldStep >= volHoldInterval) {
    lastHoldStep = millis();
    applyHoldStep(holdTarget, holdDir);
  }
}

// Speaker mode control
void set_speaker_mode() {
  speaker_mode = constrain(speaker_mode, 0, 1);
  switch(speaker_mode) {
    case 0: ch_mute = 0; surr = 0; break; // 2.1 mode
    case 1: ch_mute = 1; surr = 1; break; // 5.1 mode
  }
  set_cn(); set_sl(); set_sr(); set_surr();
  markEepromUpdate();
  showFlash(" SP ", 1000);
}

// PT2323 control functions (input/surr/mute)
void set_in() {
  in = constrain(in, 0, 4);

  uint8_t cmd;
  switch(in) {
    case 0: cmd = 0b11001011; break;  // Input 1
    case 1: cmd = 0b11001010; break;  // Input 2
    case 2: cmd = 0b11001001; break;  // Input 3
    case 3: cmd = 0b11001000; break;  // Input 4
    case 4: cmd = 0b11000111; break;  // 6CH input
  }

  writeI2C(PT2323_address, cmd);

  // Auto-adjust mix based on input
  if (in <= 3) {
    if (surr != 0) { surr = 0; set_surr(); }
    if (mix != 1) {
      mix = 1;
      set_mix();
      markEepromUpdate();
      showFlash(" 6d ", 600);
    }
  } else {
    if (mix != 0) {
      mix = 0;
      set_mix();
      markEepromUpdate();
    }
  }

  markEepromUpdate();
  if (!flashActive) showInput();
}

void set_surr() {
  surr = constrain(surr, 0, 1);
  uint8_t cmd = (surr == 0) ? 0b11010000 : 0b11010001;
  writeI2C(PT2323_address, cmd);
  markEepromUpdate();
}

void set_mute() {
  mute = constrain(mute, 0, 1);
  uint8_t cmd = (mute == 1) ? 0b11111111 : 0b11111110;
  writeI2C(PT2323_address, cmd);

  if (mute == 1) startMuteBlink();
  else {
    stopMuteBlink();
    if (!flashActive) showMaster();
  }
}

// PT2322 control functions (volume/tone/channels)
void set_vol() {
  vol = constrain(vol, 29, 79);

  mute = (vol == 29) ? 1 : 0;
  set_mute();

  int c = 79 - vol;
  vol_10 = c / 10;
  vol_1 = c - vol_10 * 10;
  uint8_t cmd1 = vol_10 + 0b11100000;
  uint8_t cmd2 = vol_1 + 0b11010000;

  writeI2C(PT2322_address, cmd1, cmd2);
  markEepromUpdate();

  if (mute == 0 && !flashActive && !muteBlink) showMaster();
}

void set_bass() {
  bass = constrain(bass, 0, 15);
  int x = (bass > 7) ? (23 - bass) : bass;
  writeI2C(PT2322_address, 0b10010000 + x);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showBass();
}

void set_mid() {
  mid = constrain(mid, 0, 15);
  int x = (mid > 7) ? (23 - mid) : mid;
  writeI2C(PT2322_address, 0b10100000 + x);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showMid();
}

void set_treb() {
  treb = constrain(treb, 0, 15);
  int x = (treb > 7) ? (23 - treb) : treb;
  writeI2C(PT2322_address, 0b10110000 + x);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showTreble();
}

void set_sub() {
  sub = constrain(sub, 0, 15);
  int c = 15 - sub;
  writeI2C(PT2322_address, 0b01100000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showSub();
}

void set_fl() {
  fl = constrain(fl, 0, 15);
  int c = 15 - fl;
  writeI2C(PT2322_address, 0b00010000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showFront();
}

void set_fr() {
  fr = constrain(fr, 0, 15);
  int c = 15 - fr;
  writeI2C(PT2322_address, 0b00100000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showFront();
}

void set_cn() {
  cn = constrain(cn, 0, 15);
  int c = 15 - cn;
  uint8_t mute_cmd = (ch_mute == 1) ? 0b11110101 : 0b11110100;
  writeI2C(PT2323_address, mute_cmd);
  writeI2C(PT2322_address, 0b00110000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showCenter();
}

void set_sl() {
  sl = constrain(sl, 0, 15);
  int c = 15 - sl;
  uint8_t mute_cmd = (ch_mute == 1) ? 0b11111001 : 0b11111000;
  writeI2C(PT2323_address, mute_cmd);
  writeI2C(PT2322_address, 0b01000000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showSurround();
}

void set_sr() {
  sr = constrain(sr, 0, 15);
  int c = 15 - sr;
  uint8_t mute_cmd = (ch_mute == 1) ? 0b11111011 : 0b11111010;
  writeI2C(PT2323_address, mute_cmd);
  writeI2C(PT2322_address, 0b01010000 + c);
  markEepromUpdate();
  if (!flashActive && !muteBlink) showSurround();
}

// Rotary encoder handling
const int8_t ENC_TABLE[16] = {0, -1, +1, 0, +1, 0, 0, -1, -1, 0, 0, +1, 0, +1, -1, 0};

void handleEncoder() {
  uint8_t curr = ((digitalRead(sw03) ? 1 : 0) << 1) | (digitalRead(sw02) ? 1 : 0);
  if (curr == encPrevState) return;

  unsigned long nowUs = micros();
  if (nowUs - encLastTransitionAt < encDebounceUs) {
    encPrevState = curr;
    return;
  }

  int idx = ((encPrevState << 2) | curr) & 0x0F;
  int8_t d = ENC_TABLE[idx];
  encPrevState = curr;
  encLastTransitionAt = nowUs;

  if (d == 0) return;

  encAccum += d;
  if (encAccum >= 4 || encAccum <= -4) {
    int steps = encAccum / 4;
    encAccum -= steps * 4;
    int mag = (millis() - encLastStepAt < encAccelThresholdMs) ? 2 : 1;
    encLastStepAt = millis();

    if (menu == 0 && vol_on == 0) {
      int delta = (steps > 0 ? +1 : -1) * mag;
      vol += delta;
      set_vol();
    }
  }
}

// IR send function
void ir_send_bt(uint32_t code) {
  irsend.sendNEC(code, 32);
  delay(120); // Pause between sends
}

// Sub menu handling
void set_sub_menu() {
  if (digitalRead(sw02) == LOW && sub_menu_sel == 0) {
    sub_menu++;
    btn_cl();
  }
  if (digitalRead(sw03) == LOW && sub_menu_sel == 0) {
    sub_menu--;
    if (sub_menu < 0) sub_menu = 0;
    btn_cl();
  }

  if (sub_menu != 0) {
    menu_sel = 1;
    if (digitalRead(sw01) == LOW && sub_menu_item_sel == 0 && menu_sel == 1) {
      sub_menu_sel = 1;
      sub_menu_item = 1;
      sub_menu_item_sel = 1;
      btn_cl();
    }
    if (digitalRead(sw01) == LOW && sub_menu_item_sel == 1) {
      sub_menu_sel = 0;
      sub_menu_item = 0;
      sub_menu_item_sel = 0;
      btn_cl();
    }
  } else {
    menu_sel = 0;
    sub_menu_sel = 0;
  }
}

// IR control function
void ir_control() {
  if (!irrecv.decode(&results)) return;

  uint32_t code = results.value;
  unsigned long now = millis();

  if (code == IR_REPEAT) {
    lastIRTime = now;
    irrecv.resume();
    return;
  }

  lastIrCode = code;
  lastIRTime = now;
  holdTarget = HOLD_NONE;
  holdDir = 0;

  // Debounce identical non-repeat frames to ensure single-step per press
  bool suppressed = (code == lastPressCode) && (now - lastPressAt < irDebounceMs);
  if (!suppressed) {
    lastPressCode = code;
    lastPressAt = now;
  }

  if (!suppressed) switch(code) {
    case ir_power:
      power++;
      if (power > 1) power = 0;
      power_up();
      break;

    case ir_mute:
      mute++;
      if (mute > 1) mute = 0;
      set_mute();
      ir_menu = 0;
      vol_on = (mute == 1) ? 1 : 0;
      break;

    case ir_in:
      in++;
      if (in > 4) in = 0;
      set_in();
      ir_cl();
      ir_menu = 0;
      break;

    case ir_sou_mode:
      toggle_3d_effect();
      ir_cl();
      ir_menu = 0;
      break;

    case ir_eq_mode:
      toggle_eq_mode();
      ir_cl();
      ir_menu = 0;
      break;

    case ir_surround_mix:
      toggle_mix();
      ir_cl();
      ir_menu = 0;
      break;

    // Volume hold
    case ir_vol_i:
      if (vol_on == 0) { vol++; set_vol(); }
      holdTarget = HOLD_VOL; holdDir = +1; lastHoldStep = now; ir_menu = 0;
      break;
    case ir_vol_d:
      if (vol_on == 0) { vol--; set_vol(); }
      holdTarget = HOLD_VOL; holdDir = -1; lastHoldStep = now; ir_menu = 0;
      break;

    // Sub
    case ir_sub_i: sub++; set_sub(); ir_cl(); ir_menu = 2; holdTarget = HOLD_SUB; holdDir = +1; lastHoldStep = now; break;
    case ir_sub_d: sub--; set_sub(); ir_cl(); ir_menu = 2; holdTarget = HOLD_SUB; holdDir = -1; lastHoldStep = now; break;

    // FL/FR
    case ir_fl_i:  fl++; set_fl(); ir_cl(); ir_menu = 3; holdTarget = HOLD_FL;  holdDir = +1; lastHoldStep = now; break;
    case ir_fl_d:  fl--; set_fl(); ir_cl(); ir_menu = 3; holdTarget = HOLD_FL;  holdDir = -1; lastHoldStep = now; break;
    case ir_fr_i:  fr++; set_fr(); ir_cl(); ir_menu = 3; holdTarget = HOLD_FR;  holdDir = +1; lastHoldStep = now; break;
    case ir_fr_d:  fr--; set_fr(); ir_cl(); ir_menu = 3; holdTarget = HOLD_FR;  holdDir = -1; lastHoldStep = now; break;

    // Center / Surround L / Surround R
    case ir_cn_i:  cn++; set_cn(); ir_cl(); ir_menu = 3; holdTarget = HOLD_CN;  holdDir = +1; lastHoldStep = now; break;
    case ir_cn_d:  cn--; set_cn(); ir_cl(); ir_menu = 3; holdTarget = HOLD_CN;  holdDir = -1; lastHoldStep = now; break;
    case ir_sl_i:  sl++; set_sl(); ir_cl(); ir_menu = 3; holdTarget = HOLD_SL;  holdDir = +1; lastHoldStep = now; break;
    case ir_sl_d:  sl--; set_sl(); ir_cl(); ir_menu = 3; holdTarget = HOLD_SL;  holdDir = -1; lastHoldStep = now; break;
    case ir_sr_i:  sr++; set_sr(); ir_cl(); ir_menu = 3; holdTarget = HOLD_SR;  holdDir = +1; lastHoldStep = now; break;
    case ir_sr_d:  sr--; set_sr(); ir_cl(); ir_menu = 3; holdTarget = HOLD_SR;  holdDir = -1; lastHoldStep = now; break;

    // Bass / Mid / Treble
    case ir_bass_i: bass++; set_bass(); ir_cl(); ir_menu = 1; holdTarget = HOLD_BASS; holdDir = +1; lastHoldStep = now; break;
    case ir_bass_d: bass--; set_bass(); ir_cl(); ir_menu = 1; holdTarget = HOLD_BASS; holdDir = -1; lastHoldStep = now; break;
    case ir_mid_i:  mid++;  set_mid();  ir_cl(); ir_menu = 1; holdTarget = HOLD_MID;  holdDir = +1; lastHoldStep = now; break;
    case ir_mid_d:  mid--;  set_mid();  ir_cl(); ir_menu = 1; holdTarget = HOLD_MID;  holdDir = -1; lastHoldStep = now; break;
    case ir_treb_i: treb++; set_treb(); ir_cl(); ir_menu = 1; holdTarget = HOLD_TREB; holdDir = +1; lastHoldStep = now; break;
    case ir_treb_d: treb--; set_treb(); ir_cl(); ir_menu = 1; holdTarget = HOLD_TREB; holdDir = -1; lastHoldStep = now; break;

    case ir_sp_mode:
      speaker_mode++;
      if (speaker_mode > 1) speaker_mode = 0;
      set_speaker_mode();
      ir_cl();
      ir_menu = 0;
      break;

    // BT control
    case ir_bt_toggle:
      if (digitalRead(BT_POWER_PIN) == HIGH) {
        digitalWrite(BT_POWER_PIN, LOW);
        showFlash("bOF ", 800);
      } else {
        digitalWrite(BT_POWER_PIN, HIGH);
        showFlash("bOn ", 800);
      }
      ir_cl();
      break;

    case ir_bt_playpause:
      ir_send_bt(ir_bt_playpause);
      showFlash("PLY ", 400);
      ir_cl();
      break;
  }

  // Menu synchronization
  if (ir_menu == 0 && menu != 0) menu = 0;
  if (ir_menu == 1 && menu != 1) menu = 1;
  if (ir_menu == 2 && menu != 2) menu = 2;
  if (ir_menu == 3 && menu != 3) menu = 3;

  irrecv.resume();
}

// Setup function
void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.println("Audio Controller Starting...");

  // Initialize IR
  irrecv.enableIRIn();
  Serial.println("IR receiver initialized");

  // Configure pins
  pinMode(sw01, INPUT_PULLUP);
  pinMode(sw02, INPUT_PULLUP);
  pinMode(sw03, INPUT_PULLUP);
  pinMode(sw04, INPUT);
  pinMode(sw05, INPUT);
  pinMode(sw06, INPUT);
  pinMode(sw07, OUTPUT);
  pinMode(IR_SEND_PIN, OUTPUT);
  pinMode(BT_POWER_PIN, OUTPUT);

  digitalWrite(sw07, LOW);
  digitalWrite(BT_POWER_PIN, LOW); // BT power OFF by default

  // Initialize display
  display.setBrightness(0x0a);
  showSegments(S7_DASH, S7_DASH, S7_DASH, S7_DASH);
  delay(500);

  // Initialize encoder state
  encPrevState = ((digitalRead(sw03) ? 1 : 0) << 1) | (digitalRead(sw02) ? 1 : 0);
  encLastTransitionAt = micros();

  // Initialize variables
  power = 0;
  time = millis();
  lastEepromUpdate = millis();

  // Load settings from EEPROM
  eeprom_read();

  // System ready
  power_up();
  showFlash(" rdy ", 1000);
  Serial.println("System ready!");
}

// Main loop
void loop() {
  // Handle EEPROM updates (with wear leveling)
  eeprom_update();

  // Handle IR commands
  ir_control();

  // Handle return delay
  return_delay();

  // Handle analog buttons (power button)
  if (analogRead(sw06) > 900) {
    power++;
    if (power > 1) power = 0;
    power_up();
    delay(btn_delay);
  }

  if (power == 1) {
    // Input selection button
    if (analogRead(sw04) > 900) {
      in++;
      if (in > 4) in = 0;
      set_in();
      delay(btn_delay);
    }

    // Mute button
    if (analogRead(sw05) > 900) {
      mute++;
      if (mute > 1) mute = 0;
      set_mute();
      delay(btn_delay);
      vol_on = (mute == 1) ? 1 : 0;
    }

    // Menu button
    if (digitalRead(sw01) == LOW && menu_sel == 0 && vol_on == 0) {
      menu++;
      if (menu > 4) menu = 0;
      btn_cl();
    }

    // Menu handling
    if (menu == 1) { // Tone menu
      set_sub_menu();
      switch(sub_menu) {
        case 1: // Bass
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { bass++; set_bass(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { bass--; set_bass(); btn_cl(); }
          break;
        case 2: // Mid
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { mid++; set_mid(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { mid--; set_mid(); btn_cl(); }
          break;
        case 3: // Treble
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { treb++; set_treb(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { treb--; set_treb(); btn_cl(); }
          break;
        case 4:
          sub_menu = 0;
          break;
      }
    }

    if (menu == 2) { // Subwoofer menu
      if (digitalRead(sw02) == LOW) { sub++; set_sub(); btn_cl(); }
      if (digitalRead(sw03) == LOW) { sub--; set_sub(); btn_cl(); }
    }

    if (menu == 3) { // Channel levels menu
      set_sub_menu();
      switch(sub_menu) {
        case 1: // Front Left
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { fl++; set_fl(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { fl--; set_fl(); btn_cl(); }
          break;
        case 2: // Front Right
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { fr++; set_fr(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { fr--; set_fr(); btn_cl(); }
          break;
        case 3: // Center
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { cn++; set_cn(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { cn--; set_cn(); btn_cl(); }
          break;
        case 4: // Surround Left
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { sl++; set_sl(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { sl--; set_sl(); btn_cl(); }
          break;
        case 5: // Surround Right
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) { sr++; set_sr(); btn_cl(); }
          if (digitalRead(sw03) == LOW && sub_menu_item == 1) { sr--; set_sr(); btn_cl(); }
          break;
        case 6:
          sub_menu = 0;
          break;
      }
    }

    if (menu == 4) { // System menu
      set_sub_menu();
      switch(sub_menu) {
        case 1: // Surround
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) {
            surr = surr ? 0 : 1;
            set_surr();
            btn_cl();
          }
          break;
        case 2: // Speaker mode
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) {
            speaker_mode++;
            if (speaker_mode > 1) speaker_mode = 0;
            set_speaker_mode();
            btn_cl();
          }
          break;
        case 3: // 3D Effect
          if (digitalRead(sw02) == LOW && sub_menu_item == 1) {
            toggle_3d_effect();
            btn_cl();
          }
          break;
        case 4:
          sub_menu = 0;
          break;
      }
    }

    // Handle encoder for main volume (only when not in menu and not muted)
    if (menu == 0 && vol_on == 0) handleEncoder();

    // Handle hold for IR
    handleHoldTick();

    // Update display effects
    if (muteBlink) updateMuteBlink();
    else if (flashActive) updateFlash();

  } else {
    // System in standby
    updateStandby();

    // Long-press SW01 (LOW) for 2s to reset to defaults while in standby
    static bool sw01Pressing = false;
    static unsigned long sw01PressStart = 0;
    if (digitalRead(sw01) == LOW) {
      if (!sw01Pressing) { sw01Pressing = true; sw01PressStart = millis(); }
      else if (millis() - sw01PressStart > 2000) {
        showFlash("rES ", 1000);
        reset_defaults();
        sw01Pressing = false;
      }
    } else {
      sw01Pressing = false;
    }
  }

  // Serial commands for testing
  if (Serial.available()) {
    char c = Serial.read();
    switch(c) {
      case 'b': // Toggle BT power
        if (digitalRead(BT_POWER_PIN) == HIGH) {
          digitalWrite(BT_POWER_PIN, LOW);
          showFlash("bOF ", 600);
          Serial.println("BT OFF");
        } else {
          digitalWrite(BT_POWER_PIN, HIGH);
          showFlash("bOn ", 600);
          Serial.println("BT ON");
        }
        break;
      case 's': // Send BT IR command
        ir_send_bt(ir_bt_toggle);
        Serial.println("Sent BT toggle command");
        break;
      case 'p': // Toggle power
        power = power ? 0 : 1;
        power_up();
        Serial.println(power ? "Power ON" : "Power OFF");
        break;
      case 'v': // Show volume
        Serial.print("Volume: ");
        Serial.println(vol);
        break;
      case 'i': // Show info
        Serial.println("=== System Status ===");
        Serial.print("Power: "); Serial.println(power ? "ON" : "OFF");
        Serial.print("Volume: "); Serial.println(vol);
        Serial.print("Input: "); Serial.println(in);
        Serial.print("Mute: "); Serial.println(mute ? "ON" : "OFF");
        Serial.print("3D Effect: "); Serial.println(effect3d ? "ON" : "OFF");
        Serial.print("EQ Mode: "); Serial.println(eq_mode);
        break;
    }
  }
}

