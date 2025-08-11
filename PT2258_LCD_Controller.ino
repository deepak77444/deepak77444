// Fast PT2258 + CS4852/CS4853 controller with 16x2 I2C LCD display
// - Rotary encoder acceleration (non-blocking)
// - IR hold-to-accelerate (repeat handling)
// - Per-group trims (Front L/R, Rear L/R, Center, Sub)
// - EEPROM save/restore (debounced)
// - LCD shows volume; briefly shows input name on change
// - Standby pin control, LEDs for status

#include <Wire.h>
#include <IRremote.h>      // classic 2.x (IRrecv/decode_results)
#include <Encoder.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include "PT2258.h"

// I2C device addresses
#define CS4852_ADDR 0x4A
#define CS4853_ADDR 0x4B

// Pins
#define IR_RECEIVER_PIN 7
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_BUTTON_PIN 4
#define LED_PIN 13
#define STANDBY_PIN 12
#define STANDBY_LED_PIN 10
#define VOLUME_UP_LED_PIN 5
#define VOLUME_DOWN_LED_PIN 6
#define MUTE_LED_PIN 9
#define PROLOGIC_LED_PIN 8

// Add second 5V control pin (USB 5V high-side switch enable)
#define USB_5V_CTRL_PIN 11

// IR Remote HEX Codes
#define IR_STANDBY        0x807F827D
#define IR_5_1            0x807F02FD
#define IR_AUX            0x807FA25D
#define IR_MUTE           0x807FF00F
#define IR_FRONT_LR_UP    0x807F40BF
#define IR_FRONT_LR_DOWN  0x807FC03F
#define IR_REAR_LR_UP     0x807F00FF
#define IR_REAR_LR_DOWN   0x807F807F
#define IR_CEN_UP         0x807F50AF
#define IR_CEN_DOWN       0x807F609F
#define IR_SUB_UP         0x807FD02F
#define IR_SUB_DOWN       0x807FE01F
#define IR_VOLUME_UP      0x807F906F
#define IR_VOLUME_DOWN    0x807FA05F

// EEPROM layout
#define EE_MARKER_ADDR  0
#define EE_MARKER_VAL   0xA5
#define EE_VOL_ADDR     1   // 0..100
#define EE_INPUT_ADDR   2   // 1..4
#define EE_MUTE_ADDR    3   // 0/1
#define EE_STANDBY_ADDR 4   // 0/1
#define EE_PROLOGIC_ADDR 5  // 0/1
#define EE_FTRIM_ADDR   6   // -20..+20 stored +20
#define EE_RTRIM_ADDR   7
#define EE_CTRIM_ADDR   8
#define EE_SUBTRIM_ADDR 9

// State
volatile long encLast = 0;
int masterVol = 50;         // 0..100
bool mute = false;
int inputSource = 1;        // 1..4 (1=AUX by IR_AUX)
bool standby = false;
bool prologicMode = false;

// Group trims (-20..+20)
int8_t trimFront = 0;       // FL/FR
int8_t trimRear = 0;        // SL/SR
int8_t trimCenter = 0;      // CN
int8_t trimSub = 0;         // SUB

bool encoderMode = false;   // false: volume, true: input
unsigned long lastBtnMs = 0;
unsigned long lastChangeMs = 0;
unsigned long volLedMs = 0;
unsigned long lastDrawMs = 0;
unsigned long tmShowInputUntil = 0; // reused timing for LCD input popup
// Show last unknown IR code briefly on LCD for debugging
unsigned long irShowUntil = 0;
uint32_t lastIrShown = 0;

const unsigned long debounceMs = 150;
const unsigned long saveDelayMs = 2000;
const unsigned long volLedDurMs = 120;

// IR accel
uint32_t lastIrCode = 0;
uint8_t irRepeatCount = 0;
unsigned long lastIrMs = 0;

// Objects
#if (IRREMOTE_VERSION >= 30000)
  // IRremote v3+: use IrReceiver singleton
#else
  IRrecv ir(IR_RECEIVER_PIN);
  decode_results irRes;
#endif
Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
LiquidCrystal_I2C lcd(0x27, 16, 2); // adjust address if needed: 0x27/0x3F

// PT2258 (8-bit I2C address; 0x88 typical)
PT2258 pt2258(0x88);

// Input names for LCD (1..4)
const char* INPUT_NAMES[4] = {"AUX", "5.1", "TV", "BT"};

// Helpers
inline void clampInt(int &v, int lo, int hi) { if (v < lo) v = lo; if (v > hi) v = hi; }
inline void clampI8(int8_t &v, int8_t lo, int8_t hi) { if (v < lo) v = lo; if (v > hi) v = hi; }

inline uint8_t irStep(bool isRepeat) {
  if (!isRepeat) { irRepeatCount = 0; return 1; }
  if (millis() - lastIrMs > 200) { irRepeatCount = 0; return 1; }
  if (irRepeatCount < 10) irRepeatCount++;
  if (irRepeatCount < 3) return 1;
  if (irRepeatCount < 6) return 2;
  return 3;
}

// Return true if 'code' (or its byte-swapped form) equals 'target'.
inline bool irCodeIs(uint32_t code, uint32_t target) {
  uint32_t swapped = ((code & 0x000000FFUL) << 24) |
                     ((code & 0x0000FF00UL) << 8)  |
                     ((code & 0x00FF0000UL) >> 8)  |
                     ((code & 0xFF000000UL) >> 24);
  return (code == target) || (swapped == target);
}

inline uint8_t necAddrFrom(uint32_t nec32) { return (uint8_t)((nec32 >> 24) & 0xFF); }
inline uint8_t necCmdFrom(uint32_t nec32)  { return (uint8_t)((nec32 >> 8)  & 0xFF); }

#if (IRREMOTE_VERSION >= 30000)
inline bool irV3Is(uint32_t expected) {
  return (IrReceiver.decodedIRData.protocol == NEC &&
          IrReceiver.decodedIRData.address  == necAddrFrom(expected) &&
          IrReceiver.decodedIRData.command  == necCmdFrom(expected));
}
#endif

int readEncoderAccel() {
  static unsigned long lastTick = 0;
  long pos = encoder.read() / 4;
  long delta = pos - encLast;
  if (delta == 0) return 0;
  encLast = pos;
  unsigned long now = millis();
  unsigned long dt = now - lastTick;
  lastTick = now;
  int accel = 1;
  if (dt < 30) accel = 4;
  else if (dt < 90) accel = 2;
  if (delta > 8) delta = 8;
  if (delta < -8) delta = -8;
  return (int)delta * accel;
}

inline uint8_t attFrom(int vol, int8_t trim) {
  int v = vol + trim;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  int att = 79 - (v * 79) / 100;
  if (att < 0) att = 0;
  if (att > 79) att = 79;
  return (uint8_t)att;
}

// LCD rendering
void lcdShowVolume() {
  lcd.setCursor(0, 0);
  lcd.print("VOL ");
  if (masterVol < 100) lcd.print(' ');
  if (masterVol < 10)  lcd.print(' ');
  lcd.print(masterVol);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print("IN: ");
  int idx = inputSource - 1;
  if (idx < 0 || idx > 3) idx = 0;
  lcd.print(INPUT_NAMES[idx]);
  lcd.print("     ");

  if (mute) { lcd.setCursor(12, 0); lcd.print("MUTE"); }
  else      { lcd.setCursor(12, 0); lcd.print("    "); }
}

void lcdShowInput() {
  lcd.setCursor(0, 0);
  lcd.print("Select Input   ");
  lcd.setCursor(0, 1);
  int idx = inputSource - 1;
  if (idx < 0 || idx > 3) idx = 0;
  lcd.print(INPUT_NAMES[idx]);
  lcd.print("            ");
}

void lcdShowIR(uint32_t code) {
  char buf[9];
  for (int i = 0; i < 8; i++) {
    uint8_t nib = (code >> (28 - 4 * i)) & 0xF;
    buf[i] = (nib < 10) ? (char)('0' + nib) : (char)('A' + (nib - 10));
  }
  buf[8] = '\0';
  lcd.setCursor(0, 0);
  lcd.print("IR ");
  lcd.print(buf);
  lcd.print("   ");
  lcd.setCursor(0, 1);
  lcd.print("            ");
}

void updateDisplay() {
  if (irShowUntil && millis() < irShowUntil) {
    lcdShowIR(lastIrShown);
  } else if (tmShowInputUntil && millis() < tmShowInputUntil) {
    lcdShowInput();
  } else {
    irShowUntil = 0;
    tmShowInputUntil = 0;
    lcdShowVolume();
  }
}

void setStandby(bool on) {
  standby = on;
  // 8050 standby sink: HIGH pulls AMP_STBY low (enter standby), LOW releases (active)
  digitalWrite(STANDBY_PIN, standby ? HIGH : LOW);
  // USB 5V rail control: HIGH = ON, LOW = OFF
  digitalWrite(USB_5V_CTRL_PIN, standby ? LOW : HIGH);

  digitalWrite(STANDBY_LED_PIN, standby ? HIGH : LOW);
  if (!standby) {
    pt2258.begin();
    pt2258.mute(false);
    lcd.backlight();
  } else {
    lcd.noBacklight();
  }
  lastChangeMs = millis();
}

void selectInput(int src) {
  if (standby) return;
  inputSource = ((src - 1 + 4) % 4) + 1;

  Wire.beginTransmission(CS4852_ADDR);
  Wire.write(0x00);
  Wire.write((inputSource - 1) << 1);
  Wire.endTransmission();

  // Brief LCD input display
  lcdShowInput();
  tmShowInputUntil = millis() + 1200;

  lastChangeMs = millis();
}

void setVolumePercent(int v, bool tickUp = false) {
  if (standby) return;
  clampInt(v, 0, 100);
  masterVol = v;
  if (mute && masterVol > 0) mute = false;

  // Per-channel attenuations with group trims
  uint8_t attFL = attFrom(masterVol, trimFront);
  uint8_t attFR = attFrom(masterVol, trimFront);
  uint8_t attCN = attFrom(masterVol, trimCenter);
  uint8_t attSL = attFrom(masterVol, trimRear);
  uint8_t attSR = attFrom(masterVol, trimRear);
  uint8_t attSUB= attFrom(masterVol, trimSub);

  if (mute || masterVol == 0) attFL = attFR = attCN = attSL = attSR = attSUB = 79;

  pt2258.attenuation(1, attFL);
  pt2258.attenuation(2, attFR);
  pt2258.attenuation(3, attCN);
  pt2258.attenuation(4, attSL);
  pt2258.attenuation(5, attSR);
  pt2258.attenuation(6, attSUB);

  // LCD update (volume)
  if (!tmShowInputUntil) lcdShowVolume();

  // Volume tick LED
  digitalWrite(tickUp ? VOLUME_UP_LED_PIN : VOLUME_DOWN_LED_PIN, HIGH);
  volLedMs = millis();

  lastChangeMs = millis();
}

void toggleMute() {
  if (standby) return;
  mute = !mute;
  pt2258.mute(mute);
  lastChangeMs = millis();
}

void handleIR() {
#if (IRREMOTE_VERSION >= 30000)
  if (!IrReceiver.decode()) return;
  bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  uint8_t step = irStep(isRepeat);
  lastIrMs = millis();
  IrReceiver.resume();

  if (irV3Is(IR_STANDBY)) { setStandby(!standby); return; }
  if (standby) return;

  if (irV3Is(IR_VOLUME_UP))   { setVolumePercent(masterVol + step, true);  return; }
  if (irV3Is(IR_VOLUME_DOWN)) { setVolumePercent(masterVol - step, false); return; }

  if (irV3Is(IR_MUTE)) { toggleMute(); return; }

  if (irV3Is(IR_AUX)) { selectInput(1); return; }

  if (irV3Is(IR_FRONT_LR_UP))   { trimFront += step; clampI8(trimFront, -20, 20); setVolumePercent(masterVol, true);  return; }
  if (irV3Is(IR_FRONT_LR_DOWN)) { trimFront -= step; clampI8(trimFront, -20, 20); setVolumePercent(masterVol, false); return; }

  if (irV3Is(IR_REAR_LR_UP))    { trimRear += step;  clampI8(trimRear, -20, 20);  setVolumePercent(masterVol, true);  return; }
  if (irV3Is(IR_REAR_LR_DOWN))  { trimRear -= step;  clampI8(trimRear, -20, 20);  setVolumePercent(masterVol, false); return; }

  if (irV3Is(IR_CEN_UP))        { trimCenter += step; clampI8(trimCenter, -20, 20); setVolumePercent(masterVol, true);  return; }
  if (irV3Is(IR_CEN_DOWN))      { trimCenter -= step; clampI8(trimCenter, -20, 20); setVolumePercent(masterVol, false); return; }

  if (irV3Is(IR_SUB_UP))        { trimSub += step;   clampI8(trimSub, -20, 20);   setVolumePercent(masterVol, true);  return; }
  if (irV3Is(IR_SUB_DOWN))      { trimSub -= step;   clampI8(trimSub, -20, 20);   setVolumePercent(masterVol, false); return; }

  if (irV3Is(IR_5_1)) {
    lcdShowInput();
    tmShowInputUntil = millis() + 1200;
    return;
  }

  // Unknown code: show it on LCD briefly for troubleshooting
  if (!isRepeat) {
    lastIrShown = IrReceiver.decodedIRData.value; // Use IrReceiver.decodedIRData.value
    irShowUntil = millis() + 1200;
  }
#else
  if (!ir.decode(&irRes)) return;
  bool isRepeat = (irRes.value == 0xFFFFFFFF);
  uint32_t code = isRepeat ? lastIrCode : irRes.value;
  uint8_t step = irStep(isRepeat);
  lastIrMs = millis();
  if (!isRepeat) lastIrCode = code;
  ir.resume();

  if (irCodeIs(code, IR_STANDBY)) { setStandby(!standby); return; }
  if (standby) return;

  if (irCodeIs(code, IR_VOLUME_UP))   { setVolumePercent(masterVol + step, true);  return; }
  if (irCodeIs(code, IR_VOLUME_DOWN)) { setVolumePercent(masterVol - step, false); return; }

  if (irCodeIs(code, IR_MUTE)) { toggleMute(); return; }

  if (irCodeIs(code, IR_AUX)) { selectInput(1); return; }

  if (irCodeIs(code, IR_FRONT_LR_UP))   { trimFront += step; clampI8(trimFront, -20, 20); setVolumePercent(masterVol, true);  return; }
  if (irCodeIs(code, IR_FRONT_LR_DOWN)) { trimFront -= step; clampI8(trimFront, -20, 20); setVolumePercent(masterVol, false); return; }

  if (irCodeIs(code, IR_REAR_LR_UP))    { trimRear += step;  clampI8(trimRear, -20, 20);  setVolumePercent(masterVol, true);  return; }
  if (irCodeIs(code, IR_REAR_LR_DOWN))  { trimRear -= step;  clampI8(trimRear, -20, 20);  setVolumePercent(masterVol, false); return; }

  if (irCodeIs(code, IR_CEN_UP))        { trimCenter += step; clampI8(trimCenter, -20, 20); setVolumePercent(masterVol, true);  return; }
  if (irCodeIs(code, IR_CEN_DOWN))      { trimCenter -= step; clampI8(trimCenter, -20, 20); setVolumePercent(masterVol, false); return; }

  if (irCodeIs(code, IR_SUB_UP))        { trimSub += step;   clampI8(trimSub, -20, 20);   setVolumePercent(masterVol, true);  return; }
  if (irCodeIs(code, IR_SUB_DOWN))      { trimSub -= step;   clampI8(trimSub, -20, 20);   setVolumePercent(masterVol, false); return; }

  if (irCodeIs(code, IR_5_1)) {
    lcdShowInput();
    tmShowInputUntil = millis() + 1200;
    return;
  }

  // Unknown code: show it on LCD briefly for troubleshooting
  if (!isRepeat) {
    lastIrShown = code;
    irShowUntil = millis() + 1200;
  }
#endif
}

void handleEncoderButton() {
  if (standby) return;
  bool pressed = (digitalRead(ENCODER_BUTTON_PIN) == LOW);
  static bool prev = false;
  if (pressed && !prev && millis() - lastBtnMs > debounceMs) {
    encoderMode = !encoderMode;       // toggle volume/input mode
    encoder.write(0);
    lastBtnMs = millis();
  }
  prev = pressed;
}

void handleEncoder() {
  if (standby) return;
  int steps = readEncoderAccel();
  if (steps == 0) return;

  if (encoderMode) {
    // input mode (any rotation shows input briefly)
    if (steps > 0) selectInput(inputSource + 1);
    else           selectInput(inputSource - 1);
  } else {
    // volume mode
    setVolumePercent(masterVol + steps, steps > 0);
  }
}

// LEDs and LCD
void updateLEDs() {
  // Main LED: ON active, blink in standby, off if muted
  if (standby) digitalWrite(LED_PIN, (millis() % 1800) < 700 ? HIGH : LOW);
  else if (mute) digitalWrite(LED_PIN, LOW);
  else digitalWrite(LED_PIN, HIGH);

  // Volume tick LED autoclear
  if (volLedMs && millis() - volLedMs > volLedDurMs) {
    digitalWrite(VOLUME_UP_LED_PIN, LOW);
    digitalWrite(VOLUME_DOWN_LED_PIN, LOW);
    volLedMs = 0;
  }

  // Mute LED blink
  digitalWrite(MUTE_LED_PIN, mute ? ((millis() % 800) < 400) : LOW);

  // ProLogic LED (steady state)
  digitalWrite(PROLOGIC_LED_PIN, prologicMode ? HIGH : LOW);
}

void saveIfIdle() {
  static bool pending = false;
  static unsigned long armedAt = 0;
  if (millis() - lastChangeMs < 200) { pending = true; armedAt = millis(); }
  if (pending && millis() - armedAt > saveDelayMs) {
    EEPROM.update(EE_MARKER_ADDR, EE_MARKER_VAL);
    EEPROM.update(EE_VOL_ADDR, (uint8_t)masterVol);
    EEPROM.update(EE_INPUT_ADDR, (uint8_t)inputSource);
    EEPROM.update(EE_MUTE_ADDR, (uint8_t)mute);
    EEPROM.update(EE_STANDBY_ADDR, (uint8_t)standby);
    EEPROM.update(EE_PROLOGIC_ADDR, (uint8_t)prologicMode);
    EEPROM.update(EE_FTRIM_ADDR,  (uint8_t)(trimFront + 20));
    EEPROM.update(EE_RTRIM_ADDR,  (uint8_t)(trimRear + 20));
    EEPROM.update(EE_CTRIM_ADDR,  (uint8_t)(trimCenter + 20));
    EEPROM.update(EE_SUBTRIM_ADDR,(uint8_t)(trimSub + 20));
    pending = false;
  }
}

void loadSettings() {
  if (EEPROM.read(EE_MARKER_ADDR) == EE_MARKER_VAL) {
    masterVol   = EEPROM.read(EE_VOL_ADDR);
    inputSource = EEPROM.read(EE_INPUT_ADDR);
    mute        = EEPROM.read(EE_MUTE_ADDR);
    standby     = EEPROM.read(EE_STANDBY_ADDR);
    prologicMode= EEPROM.read(EE_PROLOGIC_ADDR);
    trimFront   = (int8_t)EEPROM.read(EE_FTRIM_ADDR)  - 20;
    trimRear    = (int8_t)EEPROM.read(EE_RTRIM_ADDR)  - 20;
    trimCenter  = (int8_t)EEPROM.read(EE_CTRIM_ADDR)  - 20;
    trimSub     = (int8_t)EEPROM.read(EE_SUBTRIM_ADDR)- 20;
  }
  clampInt(masterVol, 0, 100);
  if (inputSource < 1 || inputSource > 4) inputSource = 1;
  clampI8(trimFront, -20, 20);
  clampI8(trimRear, -20, 20);
  clampI8(trimCenter, -20, 20);
  clampI8(trimSub, -20, 20);
}

// Setup/Loop
void setup() {
  Wire.begin();
  Wire.setClock(100000);

#if (IRREMOTE_VERSION >= 30000)
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
#else
  ir.enableIRIn(); // IRremote 2.x
  ir.blink13(false); // disable LED13 blink (we use D13 for status)
#endif

  pinMode(LED_PIN, OUTPUT);
  pinMode(STANDBY_PIN, OUTPUT);
  pinMode(STANDBY_LED_PIN, OUTPUT);
  pinMode(VOLUME_UP_LED_PIN, OUTPUT);
  pinMode(VOLUME_DOWN_LED_PIN, OUTPUT);
  pinMode(MUTE_LED_PIN, OUTPUT);
  pinMode(PROLOGIC_LED_PIN, OUTPUT);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
  // Initialize USB 5V control pin
  pinMode(USB_5V_CTRL_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(STANDBY_PIN, LOW);           // default: active (not standby)
  digitalWrite(STANDBY_LED_PIN, LOW);
  digitalWrite(VOLUME_UP_LED_PIN, LOW);
  digitalWrite(VOLUME_DOWN_LED_PIN, LOW);
  digitalWrite(MUTE_LED_PIN, LOW);
  digitalWrite(PROLOGIC_LED_PIN, LOW);
  digitalWrite(USB_5V_CTRL_PIN, HIGH);      // default: USB 5V ON

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // PT2258 init
  pt2258.begin();
  pt2258.mute(false);

  loadSettings();

  setStandby(standby);
  selectInput(inputSource);
  setVolumePercent(masterVol);

  encLast = encoder.read()/4;

  // Initial LCD view
  lcdShowVolume();
}

void loop() {
  handleIR();
  handleEncoderButton();
  handleEncoder();

  updateLEDs();
  updateDisplay();
  saveIfIdle();
}