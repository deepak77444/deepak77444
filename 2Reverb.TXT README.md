/*
 * Ultra Digital 5.1 Remote Kit (NEW Ver.3.0)
 * Factory-style firmware for Arduino UNO + R2S15902FP audio processor
 *
 * Notes:
 * - Parallel HD44780 LCD only (LiquidCrystal).
 * - NEC 32-bit IR handling for full key map.
 * - Rotary encoder with short/double/long press state logic.
 * - Input-select modal behavior with auto-confirm timeout.
 * - EEPROM persistence for all user-facing settings.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <IRremote.hpp>
#include <LiquidCrystal.h>

// ----------------------------- Hardware Pins ------------------------------
constexpr uint8_t PIN_LCD_RS = 8;
constexpr uint8_t PIN_LCD_EN = 9;
constexpr uint8_t PIN_LCD_D4 = 10;
constexpr uint8_t PIN_LCD_D5 = 11;
constexpr uint8_t PIN_LCD_D6 = 12;
constexpr uint8_t PIN_LCD_D7 = 13;

constexpr uint8_t PIN_IR_RECV = 2;
constexpr uint8_t PIN_ENC_CLK = 3;
constexpr uint8_t PIN_ENC_DT  = 4;
constexpr uint8_t PIN_ENC_SW  = 5;

constexpr uint8_t PIN_STANDBY_CTRL = 6;
constexpr uint8_t PIN_INPUT_A = A0;
constexpr uint8_t PIN_INPUT_B = A1;

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

// ----------------------------- Timing -------------------------------------
constexpr uint16_t LONG_PRESS_MS = 2200;
constexpr uint16_t DOUBLE_PRESS_MS = 350;
constexpr uint16_t INPUT_SELECT_TIMEOUT_MS = 3000;
constexpr uint16_t UI_REFRESH_MS = 80;

// ----------------------------- IR Mapping ---------------------------------
namespace IRCode {
  constexpr uint32_t POWER      = 0x807F827D;
  constexpr uint32_t MUTE       = 0x807F42BD;
  constexpr uint32_t SURROUND   = 0x807FA857;
  constexpr uint32_t RESET_ESC  = 0x807F1AE5;

  constexpr uint32_t AUX1       = 0x807F52AD;
  constexpr uint32_t AUX2       = 0x807FA25D;
  constexpr uint32_t AUX3       = 0x807F22DD;
  constexpr uint32_t USB        = 0x807F629D;

  constexpr uint32_t MASTER_UP  = 0x807F906F;
  constexpr uint32_t MASTER_DN  = 0x807FA05F;
  constexpr uint32_t FRONT_UP   = 0x807F40BF;
  constexpr uint32_t FRONT_DN   = 0x807FC03F;
  constexpr uint32_t SURR_UP    = 0x807F00FF;
  constexpr uint32_t SURR_DN    = 0x807F807F;
  constexpr uint32_t CENTER_UP  = 0x807F50AF;
  constexpr uint32_t CENTER_DN  = 0x807F609F;
  constexpr uint32_t SUB_UP     = 0x807FD02F;
  constexpr uint32_t SUB_DN     = 0x807FE01F;

  constexpr uint32_t BASS_UP    = 0x807F48B7;
  constexpr uint32_t BASS_DN    = 0x807FC837;
  constexpr uint32_t TREBLE_UP  = 0x807F08F7;
  constexpr uint32_t TREBLE_DN  = 0x807F8877;
  constexpr uint32_t GAIN_UP    = 0x807F926D;
  constexpr uint32_t GAIN_DN    = 0x807FB04F;

  constexpr uint32_t USB_PLAY   = 0x00FF30CF;
  constexpr uint32_t USB_NEXT   = 0x00FFA25D;
  constexpr uint32_t USB_PREV   = 0x00FFE21D;
  constexpr uint32_t USB_MODE   = 0x00FF6897;
  constexpr uint32_t USB_EQ     = 0x00FF20DF;

  constexpr uint32_t NUM_0      = 0x807F28D7;
  constexpr uint32_t NUM_1      = 0x807F18E7;
  constexpr uint32_t NUM_2      = 0x807F9867;
  constexpr uint32_t NUM_3      = 0x807F58A7;
  constexpr uint32_t NUM_4      = 0x807F30CF;
  constexpr uint32_t NUM_5      = 0x807FB04F;
  constexpr uint32_t NUM_6      = 0x807F708F;
  constexpr uint32_t NUM_7      = 0x807FF00F;
  constexpr uint32_t NUM_8      = 0x807F38C7;
  constexpr uint32_t NUM_9      = 0x807FB847;
}

// ----------------------------- Model --------------------------------------
enum class InputSource : uint8_t { AUX1, AUX2, AUX3, USB };
enum class Target : uint8_t { MASTER, FRONT, SURR, CENTER, SUB, GAIN };

struct Settings {
  uint8_t signature = 0xA5;
  InputSource input = InputSource::AUX1;
  int8_t master = 20;
  int8_t front = 20;
  int8_t surround = 20;
  int8_t center = 20;
  int8_t sub = 20;
  int8_t bass = 0;
  int8_t treble = 0;
  int8_t gain = 0;
  Target target = Target::MASTER;
  bool usbBoardReal = true;
  bool audioRush = false;
  uint8_t modelSetAB = 0;
  bool muted = false;
  bool surroundOn = true;
  char welcome[13] = "ULTRA DIGITAL";
};

Settings settings;

bool standby = false;
bool inputSelectMode = false;
InputSource pendingInput = InputSource::AUX1;
uint32_t inputSelectLastActivityMs = 0;
uint32_t lastUiRefreshMs = 0;

// Encoder state
uint8_t encLastState = 0;
bool swPressed = false;
uint32_t swPressedAtMs = 0;
uint32_t lastShortPressMs = 0;
bool longPressHandled = false;

// ----------------------------- Helpers ------------------------------------
int8_t clampVal(int8_t v, int8_t lo = -20, int8_t hi = 60) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

const __FlashStringHelper* inputLabel(InputSource in) {
  switch (in) {
    case InputSource::AUX1: return F("<AUX 1>");
    case InputSource::AUX2: return F("<AUX 2>");
    case InputSource::AUX3: return F("<AUX 3>");
    default: return F("<USB>");
  }
}

const __FlashStringHelper* targetLabel(Target t) {
  switch (t) {
    case Target::MASTER: return F("<MASTER>");
    case Target::FRONT: return F("<FRONT>");
    case Target::SURR: return F("<SURR>");
    case Target::CENTER: return F("<CENTER>");
    case Target::SUB: return F("<SUB>");
    default: return F("<GAIN>");
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
}

void loadSettings() {
  EEPROM.get(0, settings);
  if (settings.signature != 0xA5) {
    settings = Settings();
    saveSettings();
  }
}

void setInputHardware(InputSource in) {
  // A/B coding: AUX1=00 AUX2=01 AUX3=10 USB=11
  uint8_t code = static_cast<uint8_t>(in);
  digitalWrite(PIN_INPUT_A, code & 0x01);
  digitalWrite(PIN_INPUT_B, (code >> 1) & 0x01);
}

void applyStandbyState() {
  digitalWrite(PIN_STANDBY_CTRL, standby ? LOW : HIGH);
  if (standby) {
    lcd.noDisplay();
  } else {
    lcd.display();
  }
}

void applyAudioProcessor() {
  // Placeholder hook for R2S15902FP write sequence.
  // Existing settings object tracks all required state fields.
}

void showNormalScreen() {
  lcd.setCursor(0, 0);
  lcd.print(inputLabel(settings.input));
  lcd.print(F(" "));
  lcd.print(targetLabel(settings.target));

  lcd.setCursor(0, 1);
  lcd.print(F("VAL:"));

  int8_t shown = settings.master;
  switch (settings.target) {
    case Target::MASTER: shown = settings.master; break;
    case Target::FRONT:  shown = settings.front; break;
    case Target::SURR:   shown = settings.surround; break;
    case Target::CENTER: shown = settings.center; break;
    case Target::SUB:    shown = settings.sub; break;
    case Target::GAIN:   shown = settings.gain; break;
  }

  if (shown >= 0) lcd.print(' ');
  if (shown < 10 && shown > -10) lcd.print(' ');
  lcd.print(shown);
  lcd.print(settings.muted ? F(" MUTE") : F("     "));
}

void showInputSelectScreen() {
  lcd.setCursor(0, 0);
  lcd.print(F("INPUT SELECT   "));
  lcd.setCursor(0, 1);
  lcd.print(inputLabel(pendingInput));
  lcd.print(F(" CONFIRM>SW "));
}

void refreshUi() {
  if (standby) return;
  lcd.clear();
  if (inputSelectMode) {
    showInputSelectScreen();
  } else {
    showNormalScreen();
  }
}

void adjustTarget(int8_t delta) {
  if (inputSelectMode) return;
  switch (settings.target) {
    case Target::MASTER: settings.master = clampVal(settings.master + delta); break;
    case Target::FRONT: settings.front = clampVal(settings.front + delta); break;
    case Target::SURR: settings.surround = clampVal(settings.surround + delta); break;
    case Target::CENTER: settings.center = clampVal(settings.center + delta); break;
    case Target::SUB: settings.sub = clampVal(settings.sub + delta); break;
    case Target::GAIN: settings.gain = clampVal(settings.gain + delta); break;
  }
  applyAudioProcessor();
}

void cycleTarget() {
  uint8_t t = static_cast<uint8_t>(settings.target);
  t = (t + 1) % 6;
  settings.target = static_cast<Target>(t);
}

void enterInputSelectMode() {
  inputSelectMode = true;
  pendingInput = settings.input;
  inputSelectLastActivityMs = millis();
}

void stepInput(bool clockwise) {
  int8_t i = static_cast<int8_t>(pendingInput);
  i += clockwise ? 1 : -1;
  if (i > 3) i = 0;
  if (i < 0) i = 3;
  pendingInput = static_cast<InputSource>(i);
  inputSelectLastActivityMs = millis();
}

void confirmInputSelect() {
  settings.input = pendingInput;
  setInputHardware(settings.input);
  inputSelectMode = false;
  saveSettings();
}

void toggleStandby() {
  standby = !standby;
  settings.muted = standby;
  applyStandbyState();
  applyAudioProcessor();
  saveSettings();
}

void processIr(uint32_t code) {
  switch (code) {
    case IRCode::POWER: toggleStandby(); return;
    case IRCode::MUTE: settings.muted = !settings.muted; break;
    case IRCode::SURROUND: settings.surroundOn = !settings.surroundOn; break;
    case IRCode::RESET_ESC: settings = Settings(); break;

    case IRCode::AUX1: settings.input = InputSource::AUX1; break;
    case IRCode::AUX2: settings.input = InputSource::AUX2; break;
    case IRCode::AUX3: settings.input = InputSource::AUX3; break;
    case IRCode::USB: settings.input = InputSource::USB; break;

    case IRCode::MASTER_UP: settings.master = clampVal(settings.master + 1); break;
    case IRCode::MASTER_DN: settings.master = clampVal(settings.master - 1); break;
    case IRCode::FRONT_UP: settings.front = clampVal(settings.front + 1); break;
    case IRCode::FRONT_DN: settings.front = clampVal(settings.front - 1); break;
    case IRCode::SURR_UP: settings.surround = clampVal(settings.surround + 1); break;
    case IRCode::SURR_DN: settings.surround = clampVal(settings.surround - 1); break;
    case IRCode::CENTER_UP: settings.center = clampVal(settings.center + 1); break;
    case IRCode::CENTER_DN: settings.center = clampVal(settings.center - 1); break;
    case IRCode::SUB_UP: settings.sub = clampVal(settings.sub + 1); break;
    case IRCode::SUB_DN: settings.sub = clampVal(settings.sub - 1); break;

    case IRCode::BASS_UP: settings.bass = clampVal(settings.bass + 1); break;
    case IRCode::BASS_DN: settings.bass = clampVal(settings.bass - 1); break;
    case IRCode::TREBLE_UP: settings.treble = clampVal(settings.treble + 1); break;
    case IRCode::TREBLE_DN: settings.treble = clampVal(settings.treble - 1); break;
    case IRCode::GAIN_UP: settings.gain = clampVal(settings.gain + 1); break;
    case IRCode::GAIN_DN: settings.gain = clampVal(settings.gain - 1); break;

    case IRCode::NUM_0: case IRCode::NUM_1: case IRCode::NUM_2: case IRCode::NUM_3: case IRCode::NUM_4:
    case IRCode::NUM_5: case IRCode::NUM_6: case IRCode::NUM_7: case IRCode::NUM_8: case IRCode::NUM_9:
      return; // Service-only keys ignored in normal mode.

    default:
      // USB media keys are valid only in USB input; routed externally if needed.
      if (settings.input != InputSource::USB) return;
      break;
  }

  setInputHardware(settings.input);
  applyAudioProcessor();
  saveSettings();
}

void handleEncoderRotation() {
  uint8_t s = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  if (s == encLastState) return;

  // Gray decode transitions
  bool clockwise = (encLastState == 0b00 && s == 0b01) ||
                   (encLastState == 0b01 && s == 0b11) ||
                   (encLastState == 0b11 && s == 0b10) ||
                   (encLastState == 0b10 && s == 0b00);

  if (inputSelectMode) {
    stepInput(clockwise);
  } else {
    adjustTarget(clockwise ? +1 : -1);
  }

  encLastState = s;
}

void handleEncoderSwitch() {
  bool down = digitalRead(PIN_ENC_SW) == LOW;
  uint32_t now = millis();

  if (down && !swPressed) {
    swPressed = true;
    swPressedAtMs = now;
    longPressHandled = false;
  }

  if (down && swPressed && !longPressHandled && (now - swPressedAtMs >= LONG_PRESS_MS)) {
    longPressHandled = true;
    toggleStandby();
  }

  if (!down && swPressed) {
    swPressed = false;
    if (!longPressHandled) {
      if (inputSelectMode) {
        confirmInputSelect();
      } else if (now - lastShortPressMs <= DOUBLE_PRESS_MS) {
        enterInputSelectMode();
      } else {
        cycleTarget();
      }
      lastShortPressMs = now;
    }
  }

  if (inputSelectMode && (now - inputSelectLastActivityMs >= INPUT_SELECT_TIMEOUT_MS)) {
    confirmInputSelect();
  }
}

void setup() {
  pinMode(PIN_STANDBY_CTRL, OUTPUT);
  pinMode(PIN_INPUT_A, OUTPUT);
  pinMode(PIN_INPUT_B, OUTPUT);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();

  loadSettings();
  setInputHardware(settings.input);
  applyAudioProcessor();

  IrReceiver.begin(PIN_IR_RECV, ENABLE_LED_FEEDBACK);

  encLastState = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  applyStandbyState();
  refreshUi();
}

void loop() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    processIr(code);
    IrReceiver.resume();
  }

  handleEncoderRotation();
  handleEncoderSwitch();

  uint32_t now = millis();
  if (now - lastUiRefreshMs >= UI_REFRESH_MS) {
    refreshUi();
    lastUiRefreshMs = now;
  }
}
