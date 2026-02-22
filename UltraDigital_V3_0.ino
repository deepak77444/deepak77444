#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <IRremote.h>

// ----------------------------- Pin Map -----------------------------
// LCD parallel pins (adjust to your wiring)
const uint8_t LCD_RS = 12;
const uint8_t LCD_EN = 11;
const uint8_t LCD_D4 = 5;
const uint8_t LCD_D5 = 4;
const uint8_t LCD_D6 = 3;
const uint8_t LCD_D7 = 2;

// IR receiver pin
const uint8_t IR_PIN = A5;

// Rotary encoder pins
const uint8_t ENC_CLK = 6;
const uint8_t ENC_DT  = 7;
const uint8_t ENC_SW  = 8;

// Audio / relay / mute / standby control (adjust to hardware)
const uint8_t PIN_MUTE_RELAY = 9;
const uint8_t PIN_STANDBY_RELAY = 10;

// --------------------------- LCD Setup -----------------------------
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Custom character set for big digits
byte cgram[8][8] = {
  {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111}, // full block
  {B11111, B11111, B11111, B00000, B00000, B00000, B00000, B00000}, // top half
  {B00000, B00000, B00000, B00000, B11111, B11111, B11111, B11111}, // bottom half
  {B11111, B00000, B00000, B00000, B00000, B00000, B00000, B11111}, // left vertical
  {B00001, B00001, B00001, B00001, B00001, B00001, B00001, B00001}, // right vertical thin
  {B11111, B00000, B00000, B11111, B11111, B00000, B00000, B11111}, // middle with sides
  {B11111, B00000, B00000, B11111, B11111, B00000, B00000, B00000}, // upper middle
  {B00000, B00000, B00000, B11111, B11111, B00000, B00000, B11111}  // lower middle
};

// -------------------------- Constants ------------------------------
const uint16_t LONG_PRESS_MS = 2200;
const uint16_t DEBOUNCE_MS = 25;
const uint16_t AUTO_RETURN_MS = 3000;
const uint16_t POP_DELAY_MS = 600;
const uint16_t RAMP_STEP_MS = 40;

const int8_t VOLUME_MIN = 0;
const int8_t VOLUME_MAX = 79;
const int8_t TONE_MIN = -14;
const int8_t TONE_MAX = 14;
const int8_t GAIN_MIN = 0;
const int8_t GAIN_MAX = 31;
const int8_t TRIM_MIN = -15;
const int8_t TRIM_MAX = 15;

// ---------------------------- Enums --------------------------------
enum InputSource : uint8_t { IN_USB, IN_AUX1, IN_AUX2, IN_AUX3, IN_DVD };
enum USBBoardType : uint8_t { USB_MP3, USB_REALPLAY, USB_WIRE };
enum AudioRushInput : uint8_t { RUSH_HDMI_ARC, RUSH_OPT1, RUSH_OPT2, RUSH_COAX, RUSH_AUX };
enum Channel : uint8_t { CH_FL, CH_FR, CH_SR, CH_SL, CH_C, CH_SUB, CH_COUNT };

enum ControlTarget : uint8_t {
  CT_MASTER,
  CT_FRONT,
  CT_SURROUND,
  CT_CENTER,
  CT_SUB,
  CT_GAIN,
  CT_BASS,
  CT_TREBLE,
  CT_INPUT_USB,
  CT_INPUT_AUX1,
  CT_INPUT_AUX2,
  CT_INPUT_AUX3,
  CT_INPUT_DVD,
  CT_COUNT
};

enum UiMode : uint8_t {
  UI_NORMAL,
  UI_TEST_TONE,
  UI_WELCOME_EDIT,
  UI_AUDIO_RUSH,
  UI_MODEL_SET,
  UI_STANDBY
};

// -------------------------- Persistent -----------------------------
struct PersistData {
  uint16_t signature;
  uint8_t version;
  uint8_t input;
  uint8_t usbBoard;
  uint8_t rushInput;
  uint8_t modelSet;
  bool modeAB;
  int8_t master;
  int8_t front;
  int8_t surround;
  int8_t center;
  int8_t sub;
  int8_t gain;
  int8_t bass;
  int8_t treble;
  uint8_t encoderTarget;
  bool mute;
  bool surroundOn;
  char welcome[17];
};

PersistData data;
const uint16_t EEPROM_SIGNATURE = 0xA35C;
const uint8_t EEPROM_VERSION = 3;

// -------------------------- Runtime -------------------------------
UiMode uiMode = UI_NORMAL;
ControlTarget activeTarget = CT_MASTER;
InputSource currentInput = IN_AUX1;

bool standby = false;
bool pendingUnmute = false;
unsigned long unmuteAt = 0;
unsigned long lastUiActivity = 0;
unsigned long lastRampAt = 0;

// test tone
bool testToneEnabled = false;
Channel testToneChannel = CH_FL;

// encoder
uint8_t encoderPrev = 0;
bool swPrevRaw = HIGH;
bool swStable = HIGH;
unsigned long swDebounceAt = 0;
unsigned long swPressAt = 0;
bool swLongHandled = false;

// display caching
char row0Cache[21] = {0};
char row1Cache[21] = {0};

// ------------------------- Helpers --------------------------------
uint32_t lastIRCode = 0;

const char* inputLabel(InputSource in) {
  switch (in) {
    case IN_USB: return "<USB>";
    case IN_AUX1: return "<AUX1>";
    case IN_AUX2: return "<AUX2>";
    case IN_AUX3: return "<AUX3>";
    case IN_DVD: return "<DVD>";
  }
  return "<AUX1>";
}

const char* targetLabel(ControlTarget t) {
  switch (t) {
    case CT_MASTER: return "<MASTER>";
    case CT_FRONT: return "<FRONT>";
    case CT_SURROUND: return "<SURR>";
    case CT_CENTER: return "<CENTER>";
    case CT_SUB: return "<SUB>";
    case CT_GAIN: return "<GAIN>";
    case CT_BASS: return "<BASS>";
    case CT_TREBLE: return "<TREBLE>";
    case CT_INPUT_USB: return "<USB>";
    case CT_INPUT_AUX1: return "<AUX1>";
    case CT_INPUT_AUX2: return "<AUX2>";
    case CT_INPUT_AUX3: return "<AUX3>";
    case CT_INPUT_DVD: return "<DVD>";
    default: return "<MASTER>";
  }
}

bool isInputTarget(ControlTarget t) {
  return t >= CT_INPUT_USB;
}

template<typename T>
T clampT(T v, T lo, T hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

void wearSafeWrite(const PersistData& src) {
  const uint8_t* p = (const uint8_t*)&src;
  for (unsigned i = 0; i < sizeof(PersistData); ++i) {
    if (EEPROM.read(i) != p[i]) EEPROM.update(i, p[i]);
  }
}

void setDefaults() {
  memset(&data, 0, sizeof(data));
  data.signature = EEPROM_SIGNATURE;
  data.version = EEPROM_VERSION;
  data.input = IN_AUX1;
  data.usbBoard = USB_MP3;
  data.rushInput = RUSH_HDMI_ARC;
  data.modelSet = 1;
  data.modeAB = false;
  data.master = 30;
  data.front = 0;
  data.surround = 0;
  data.center = 0;
  data.sub = 0;
  data.gain = 16;
  data.bass = 0;
  data.treble = 0;
  data.encoderTarget = CT_MASTER;
  data.mute = false;
  data.surroundOn = true;
  strncpy(data.welcome, "ULTRA DIGITAL", 16);
  data.welcome[16] = '\0';
}

void loadState() {
  EEPROM.get(0, data);
  if (data.signature != EEPROM_SIGNATURE || data.version != EEPROM_VERSION) {
    setDefaults();
    wearSafeWrite(data);
  }

  data.master = clampT<int8_t>(data.master, VOLUME_MIN, VOLUME_MAX);
  data.front = clampT<int8_t>(data.front, TRIM_MIN, TRIM_MAX);
  data.surround = clampT<int8_t>(data.surround, TRIM_MIN, TRIM_MAX);
  data.center = clampT<int8_t>(data.center, TRIM_MIN, TRIM_MAX);
  data.sub = clampT<int8_t>(data.sub, TRIM_MIN, TRIM_MAX);
  data.gain = clampT<int8_t>(data.gain, GAIN_MIN, GAIN_MAX);
  data.bass = clampT<int8_t>(data.bass, TONE_MIN, TONE_MAX);
  data.treble = clampT<int8_t>(data.treble, TONE_MIN, TONE_MAX);
  data.encoderTarget = clampT<uint8_t>(data.encoderTarget, 0, CT_COUNT - 1);
  if (data.welcome[0] == '\0') strncpy(data.welcome, "ULTRA DIGITAL", 16);

  activeTarget = (ControlTarget)data.encoderTarget;
  currentInput = (InputSource)clampT<uint8_t>(data.input, 0, IN_DVD);
}

void saveState() {
  data.input = currentInput;
  data.encoderTarget = activeTarget;
  wearSafeWrite(data);
}

void applyAudioToHardware() {
  // Factory-level control abstraction point.
  // Replace with exact R2S15902FP and relay write sequence for your PCB.
  // Keep non-blocking and idempotent.
}

void setMute(bool on) {
  data.mute = on;
  digitalWrite(PIN_MUTE_RELAY, on ? HIGH : LOW);
}

void safePowerStage(bool on) {
  if (on) {
    digitalWrite(PIN_STANDBY_RELAY, HIGH);
    setMute(true);
    pendingUnmute = true;
    unmuteAt = millis() + POP_DELAY_MS;
  } else {
    setMute(true);
    digitalWrite(PIN_STANDBY_RELAY, LOW);
    pendingUnmute = false;
  }
}

void setStandby(bool on) {
  standby = on;
  if (standby) {
    saveState();
    safePowerStage(false);
    lcd.noDisplay();
    uiMode = UI_STANDBY;
  } else {
    safePowerStage(true);
    uiMode = UI_NORMAL;
    lcd.display();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(data.welcome);
    lastUiActivity = millis();
  }
}

void confirmInputFromTarget(ControlTarget t) {
  if (t == CT_INPUT_USB) currentInput = IN_USB;
  else if (t == CT_INPUT_AUX1) currentInput = IN_AUX1;
  else if (t == CT_INPUT_AUX2) currentInput = IN_AUX2;
  else if (t == CT_INPUT_AUX3) currentInput = IN_AUX3;
  else if (t == CT_INPUT_DVD) currentInput = IN_DVD;
  applyAudioToHardware();
  saveState();
}

void adjustByTarget(ControlTarget t, int8_t delta) {
  switch (t) {
    case CT_MASTER: data.master = clampT<int8_t>(data.master + delta, VOLUME_MIN, VOLUME_MAX); break;
    case CT_FRONT: data.front = clampT<int8_t>(data.front + delta, TRIM_MIN, TRIM_MAX); break;
    case CT_SURROUND: data.surround = clampT<int8_t>(data.surround + delta, TRIM_MIN, TRIM_MAX); break;
    case CT_CENTER: data.center = clampT<int8_t>(data.center + delta, TRIM_MIN, TRIM_MAX); break;
    case CT_SUB: data.sub = clampT<int8_t>(data.sub + delta, TRIM_MIN, TRIM_MAX); break;
    case CT_GAIN: data.gain = clampT<int8_t>(data.gain + delta, GAIN_MIN, GAIN_MAX); break;
    case CT_BASS: data.bass = clampT<int8_t>(data.bass + delta, TONE_MIN, TONE_MAX); break;
    case CT_TREBLE: data.treble = clampT<int8_t>(data.treble + delta, TONE_MIN, TONE_MAX); break;
    default: break;
  }
  applyAudioToHardware();
}

void nextControlTarget() {
  activeTarget = (ControlTarget)((activeTarget + 1) % CT_COUNT);
  lastUiActivity = millis();
}

void registerActivity() {
  lastUiActivity = millis();
}

void writeLineCached(uint8_t row, const char* txt) {
  char* cache = (row == 0) ? row0Cache : row1Cache;
  if (strncmp(cache, txt, LCD_COLS) == 0) return;
  lcd.setCursor(0, row);
  lcd.print(txt);
  strncpy(cache, txt, LCD_COLS);
  cache[LCD_COLS] = '\0';
}

void drawBigDigit(uint8_t col, uint8_t row, uint8_t d) {
  // 2x2 composition using custom chars
  const uint8_t top[10][2] = {
    {1, 1}, {32, 4}, {6, 1}, {6, 1}, {3, 4}, {1, 6}, {1, 6}, {1, 4}, {1, 1}, {1, 1}
  };
  const uint8_t bot[10][2] = {
    {2, 2}, {32, 4}, {2, 7}, {7, 2}, {32, 4}, {7, 2}, {2, 2}, {32, 4}, {2, 2}, {7, 2}
  };

  lcd.setCursor(col, row);
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t c = top[d][i];
    if (c == 32) lcd.print(' '); else lcd.write(c);
  }
  lcd.setCursor(col, row + 1);
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t c = bot[d][i];
    if (c == 32) lcd.print(' '); else lcd.write(c);
  }
}

void drawBig2DigitValue(uint8_t value) {
  uint8_t tens = (value / 10) % 10;
  uint8_t ones = value % 10;
  uint8_t baseCol = LCD_COLS >= 16 ? LCD_COLS - 5 : 10;
  drawBigDigit(baseCol, 0, tens);
  drawBigDigit(baseCol + 2, 0, ones);
}

void renderNormal() {
  char line0[21];
  char line1[21];
  snprintf(line0, sizeof(line0), "%-8s", targetLabel(activeTarget));

  if (data.mute) snprintf(line1, sizeof(line1), "MUTE %-8s", inputLabel(currentInput));
  else snprintf(line1, sizeof(line1), "%-8s", inputLabel(currentInput));

  for (uint8_t i = strlen(line0); i < LCD_COLS; i++) line0[i] = ' ';
  line0[LCD_COLS] = 0;
  for (uint8_t i = strlen(line1); i < LCD_COLS; i++) line1[i] = ' ';
  line1[LCD_COLS] = 0;

  writeLineCached(0, line0);
  writeLineCached(1, line1);

  uint8_t showVal = (activeTarget == CT_MASTER) ? data.master :
                    (activeTarget == CT_GAIN ? data.gain :
                    (activeTarget == CT_BASS ? data.bass + 14 :
                    (activeTarget == CT_TREBLE ? data.treble + 14 :
                    (activeTarget == CT_FRONT ? data.front + 15 :
                    (activeTarget == CT_SURROUND ? data.surround + 15 :
                    (activeTarget == CT_CENTER ? data.center + 15 :
                    (activeTarget == CT_SUB ? data.sub + 15 : data.master)))))));

  drawBig2DigitValue(showVal);
}

void renderTestTone() {
  const char* ch[] = {"FL", "FR", "SR", "SL", "C", "SUB"};
  char l0[17], l1[17];
  snprintf(l0, sizeof(l0), "<TEST> %s      ", ch[testToneChannel]);
  snprintf(l1, sizeof(l1), "LEVEL %2d       ", data.master);
  writeLineCached(0, l0);
  writeLineCached(1, l1);
}

void renderUi() {
  if (standby) return;
  if (uiMode == UI_TEST_TONE) renderTestTone();
  else renderNormal();
}

void handleEncoder() {
  uint8_t a = digitalRead(ENC_CLK);
  uint8_t b = digitalRead(ENC_DT);
  uint8_t now = (a << 1) | b;
  int8_t move = 0;

  // quadrature state table
  static const int8_t tbl[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t idx = (encoderPrev << 2) | now;
  move = tbl[idx];
  encoderPrev = now;

  if (move != 0) {
    registerActivity();
    if (uiMode == UI_TEST_TONE) {
      adjustByTarget((ControlTarget)(CT_FRONT + testToneChannel / 2), move);
    } else if (!isInputTarget(activeTarget)) {
      adjustByTarget(activeTarget, move);
    }
  }
}

void onShortPress() {
  registerActivity();
  if (uiMode == UI_TEST_TONE) {
    testToneChannel = (Channel)((testToneChannel + 1) % CH_COUNT);
    return;
  }

  if (isInputTarget(activeTarget)) confirmInputFromTarget(activeTarget);
  else nextControlTarget();
}

void onLongPress() {
  setStandby(!standby);
}

void handleButton() {
  bool raw = digitalRead(ENC_SW);
  unsigned long now = millis();

  if (raw != swPrevRaw) {
    swPrevRaw = raw;
    swDebounceAt = now;
  }

  if ((now - swDebounceAt) > DEBOUNCE_MS && raw != swStable) {
    swStable = raw;
    if (swStable == LOW) {
      swPressAt = now;
      swLongHandled = false;
    } else {
      if (!swLongHandled) onShortPress();
    }
  }

  if (swStable == LOW && !swLongHandled && (now - swPressAt >= LONG_PRESS_MS)) {
    swLongHandled = true;
    onLongPress();
  }
}

void handleIRCode(uint32_t code) {
  lastIRCode = code;
  registerActivity();

  switch (code) {
    case 0x807F827D: setStandby(!standby); break; // power
    case 0x807F42BD: setMute(!data.mute); break; // mute
    case 0x807FA857: data.surroundOn = !data.surroundOn; applyAudioToHardware(); break;
    case 0x807F1AE5: setDefaults(); applyAudioToHardware(); saveState(); break;

    case 0x807F52AD: currentInput = IN_AUX1; break;
    case 0x807FA25D: currentInput = IN_AUX2; break;
    case 0x807F22DD: currentInput = IN_AUX3; break;
    case 0x807F629D: currentInput = IN_USB; break;

    case 0x807F906F: adjustByTarget(CT_MASTER, +1); break;
    case 0x807FA05F: adjustByTarget(CT_MASTER, -1); break;
    case 0x807F40BF: adjustByTarget(CT_FRONT, +1); break;
    case 0x807FC03F: adjustByTarget(CT_FRONT, -1); break;
    case 0x807F00FF: adjustByTarget(CT_SURROUND, +1); break;
    case 0x807F807F: adjustByTarget(CT_SURROUND, -1); break;
    case 0x807F50AF: adjustByTarget(CT_CENTER, +1); break;
    case 0x807F609F: adjustByTarget(CT_CENTER, -1); break;
    case 0x807FD02F: adjustByTarget(CT_SUB, +1); break;
    case 0x807FE01F: adjustByTarget(CT_SUB, -1); break;

    case 0x807F48B7: adjustByTarget(CT_BASS, +1); break;
    case 0x807FC837: adjustByTarget(CT_BASS, -1); break;
    case 0x807F08F7: adjustByTarget(CT_TREBLE, +1); break;
    case 0x807F8877: adjustByTarget(CT_TREBLE, -1); break;

    case 0x807F926D: adjustByTarget(CT_GAIN, +1); break;
    case 0x807FB04F: adjustByTarget(CT_GAIN, -1); break;

    // numeric and service entry hooks
    case 0x807F28D7: uiMode = UI_AUDIO_RUSH; break; // 0
    case 0x807F9867: uiMode = UI_AUDIO_RUSH; break; // 2

    default: break;
  }

  applyAudioToHardware();
}

void handleIR() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    if (code != 0) handleIRCode(code);
    IrReceiver.resume();
  }
}

void serviceTimers() {
  unsigned long now = millis();

  if (pendingUnmute && now >= unmuteAt) {
    pendingUnmute = false;
    setMute(false);
  }

  if (uiMode == UI_NORMAL && activeTarget != CT_MASTER && (now - lastUiActivity >= AUTO_RETURN_MS)) {
    activeTarget = CT_MASTER;
  }

  if (!standby && (now - lastRampAt >= RAMP_STEP_MS)) {
    lastRampAt = now;
  }
}

void setupCustomChars() {
  for (uint8_t i = 0; i < 8; i++) lcd.createChar(i, cgram[i]);
}

void setup() {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(PIN_MUTE_RELAY, OUTPUT);
  pinMode(PIN_STANDBY_RELAY, OUTPUT);

  digitalWrite(PIN_MUTE_RELAY, HIGH);
  digitalWrite(PIN_STANDBY_RELAY, LOW);

  lcd.begin(LCD_COLS, LCD_ROWS);
  setupCustomChars();

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  loadState();
  encoderPrev = (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(data.welcome);

  setStandby(false);
  applyAudioToHardware();
  registerActivity();
}

void loop() {
  handleEncoder();
  handleButton();
  handleIR();
  serviceTimers();

  if (!standby) renderUi();
}
