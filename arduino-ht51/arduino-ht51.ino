#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <R2S15902FP.h>
#include <Encoder.h>
#include <IRremote.h>
#include <EEPROM.h>
#include <MsTimer2.h>

// IRremote compatibility: use IrReceiver for v3+, or IRrecv for legacy versions
#if defined(IRREMOTE_VERSION_MAJOR) && (IRREMOTE_VERSION_MAJOR >= 3)
  #define IRREMOTE_V3_COMPAT 1
#else
  #define IRREMOTE_V3_COMPAT 0
#endif
#if !IRREMOTE_V3_COMPAT
  // Legacy IRremote API
  IRrecv irrecv(IR_PIN);
  decode_results irResults;
#endif

// -----------------------------
// IR HEX codes (from user)
// -----------------------------
#define STANDBY        0x807F827D
#define FT003          0x807F02FD
#define USB            0x807FA25D
#define AUX2           0x807F22DD
#define AUX3           0x807F20DF
#define AUX4           0x807F629D
#define GAIN_UP        0x807F926D
#define GAIN_DOWN      0x807FB04F
#define MUTE_KEY       0x807FF00F
#define FRONT_LR_UP    0x807F40BF
#define FRONT_LR_DOWN  0x807FC03F
#define REAR_LR_UP     0x807F00FF
#define REAR_LR_DOWN   0x807F807F
#define CEN_UP         0x807F50AF
#define CEN_DOWN       0x807F609F
#define SUB_UP         0x807FD02F
#define SUB_DOWN       0x807FE01F
#define VOLUME_UP      0x807F906F
#define VOLUME_DOWN    0x807FA05F
#define BASS_UP        0x807F48B7
#define BASS_DOWN      0x807FC837
#define TREBLE_UP      0x807F08F7
#define TREBLE_DOWN    0x807F8877

// -----------------------------
// Hardware configuration
// -----------------------------
// LCD I2C address and geometry
static const uint8_t LCD_I2C_ADDR = 0x27; // adjust if needed (0x27 or 0x3F common)
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

// IR receiver pin (Arduino Nano). IRremote v3+ style
static const uint8_t IR_PIN = 11; // change to your IR receiver output pin

// Rotary encoder pins (use external pull-ups or internal INPUT_PULLUP)
static const uint8_t ENCODER_PIN_A = 2;  // interrupt-capable
static const uint8_t ENCODER_PIN_B = 3;  // interrupt-capable
static const uint8_t ENCODER_BUTTON_PIN = 4; // push button if available
Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);

// A simple activity LED (optional)
static const uint8_t LED_PIN = 13;

// R2S15902FP data/clock pins (change to your wiring)
static const uint8_t AMP_DATA_PIN = 6;
static const uint8_t AMP_CLK_PIN  = 7;

// Audio processor
R2S15902FP amp(AMP_DATA_PIN, AMP_CLK_PIN);

// -----------------------------
// Settings and state
// -----------------------------
struct Settings {
  uint8_t version;         // structure version
  uint8_t inputIndex;      // 0..4 => FT003, USB, AUX2, AUX3, AUX4
  uint8_t masterVolume;    // 0..63 (0=min, 63=max). Internally mapped to attenuation
  int8_t bass;             // -14..+14
  int8_t treble;           // -14..+14
  int8_t frontBalance;     // -15..+15 negative=Left, positive=Right
  int8_t rearBalance;      // -15..+15 negative=Left, positive=Right
  int8_t centerLevel;      // -31..+31 (trim)
  int8_t subLevel;         // -31..+31 (trim)
  bool isMuted;            // mute flag
  int8_t preampGain;       // -15..+15 input gain
};

static const uint8_t SETTINGS_VERSION = 1;
static const int EEPROM_ADDR = 0; // store at address 0

Settings settings;

// Encoder handling
volatile long encoderLast = 0;
volatile uint32_t lastInteractionMs = 0;
bool inMenu = false;
uint8_t menuIndex = 0; // 0=Volume,1=Bass,2=Treble,3=Input,4=FrontBal,5=RearBal,6=Center,7=Sub,8=Gain,9=Mute

// Periodic tasks
static const unsigned long UI_REFRESH_MS = 200;
static const unsigned long AUTO_SAVE_IDLE_MS = 5000;
volatile bool tick50ms = false;

// Helper for timing
unsigned long lastUiRefresh = 0;
unsigned long lastSaveAttempt = 0;

// -----------------------------
// Forward declarations
// -----------------------------
void loadSettings();
void saveSettings();
void constrainSettings();
void applyAllSettings();
void applyInput();
void applyMasterVolume();
void applyTone();
void applyChannelLevels();
void applyMute();
void applyStandby(bool standbyOn);
void showHome();
void showDetail();
void handleIr();
void handleEncoder();
void handleButton();
void writeAmp();

// Timer2 ISR callback every 50ms
void onTimer50ms() {
  tick50ms = true;
}

// Map input index to label
const char* inputName(uint8_t idx) {
  switch (idx) {
    case 0: return "FT003";
    case 1: return "USB";
    case 2: return "AUX2";
    case 3: return "AUX3";
    case 4: return "AUX4";
    default: return "IN?";
  }
}

// -----------------------------
// Arduino lifecycle
// -----------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

  // I2C
  Wire.begin();

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("5.1 Controller");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // IR
#if IRREMOTE_V3_COMPAT
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
#else
  irrecv.enableIRIn();
#endif

  // Load persisted settings
  loadSettings();
  constrainSettings();

  // Apply settings to hardware
  applyAllSettings();

  // Encoder baseline
  encoderLast = encoder.read();

  // Periodic timer: 50ms
  MsTimer2::set(50, onTimer50ms);
  MsTimer2::start();

  lastInteractionMs = millis();
  lastUiRefresh = millis();
  lcd.clear();
  showHome();
}

void loop() {
  if (tick50ms) {
    tick50ms = false;
    handleEncoder();
    handleButton();
    handleIr();
  }

  const unsigned long now = millis();
  if (now - lastUiRefresh >= UI_REFRESH_MS) {
    lastUiRefresh = now;
    if (inMenu) {
      showDetail();
    } else {
      showHome();
    }
  }

  // Auto-save after idle
  if (now - lastInteractionMs >= AUTO_SAVE_IDLE_MS && now - lastSaveAttempt >= AUTO_SAVE_IDLE_MS) {
    lastSaveAttempt = now;
    saveSettings();
  }
}

// -----------------------------
// Persistence
// -----------------------------
void loadSettings() {
  EEPROM.get(EEPROM_ADDR, settings);
  if (settings.version != SETTINGS_VERSION) {
    settings.version = SETTINGS_VERSION;
    settings.inputIndex = 0;
    settings.masterVolume = 40; // mid
    settings.bass = 0;
    settings.treble = 0;
    settings.frontBalance = 0;
    settings.rearBalance = 0;
    settings.centerLevel = 0;
    settings.subLevel = 0;
    settings.isMuted = false;
    settings.preampGain = 0;
    saveSettings();
  }
}

void saveSettings() {
  settings.version = SETTINGS_VERSION;
  EEPROM.put(EEPROM_ADDR, settings);
}

void constrainSettings() {
  if (settings.inputIndex > 4) settings.inputIndex = 0;
  if (settings.masterVolume > 63) settings.masterVolume = 63;
  if ((int)settings.masterVolume < 0) settings.masterVolume = 0;

  if (settings.bass < -14) settings.bass = -14;
  if (settings.bass > 14) settings.bass = 14;
  if (settings.treble < -14) settings.treble = -14;
  if (settings.treble > 14) settings.treble = 14;

  if (settings.frontBalance < -15) settings.frontBalance = -15;
  if (settings.frontBalance > 15) settings.frontBalance = 15;
  if (settings.rearBalance < -15) settings.rearBalance = -15;
  if (settings.rearBalance > 15) settings.rearBalance = 15;

  if (settings.centerLevel < -31) settings.centerLevel = -31;
  if (settings.centerLevel > 31) settings.centerLevel = 31;
  if (settings.subLevel < -31) settings.subLevel = -31;
  if (settings.subLevel > 31) settings.subLevel = 31;

  if (settings.preampGain < -15) settings.preampGain = -15;
  if (settings.preampGain > 15) settings.preampGain = 15;
}

// -----------------------------
// Apply to hardware (R2S15902FP slots)
// -----------------------------
void applyAllSettings() {
  constrainSettings();
  writeAmp();
}

void applyInput() { writeAmp(); }
void applyMasterVolume() { writeAmp(); }
void applyTone() { writeAmp(); }
void applyChannelLevels() { writeAmp(); }
void applyMute() { writeAmp(); }

void applyStandby(bool standbyOn) {
  // No explicit standby in provided header; could be implemented externally if needed
  (void)standbyOn;
}

// Map settings to R2S15902FP slots
static inline int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void writeAmp() {
  // Base attenuation from master volume. If muted, force max attenuation.
  int baseAtt = settings.isMuted ? 63 : clampInt(63 - (int)settings.masterVolume, 0, 63);

  // slot1: global/input & tone
  // Fields: in, rec_out, rec_gain, att, lr_in, bass, treble, sl_sr_c_sw_in, in_gain
  int in = settings.inputIndex;                // 0..4
  int rec_out = 0;                             // disabled
  int rec_gain = 0;                            // 0 dB
  int att = baseAtt;                           // use base attenuation here
  int lr_in = 0;                               // normal L/R
  int bass = settings.bass;                    // -14..+14
  int treble = settings.treble;                // -14..+14
  int sl_sr_c_sw_in = 0;                       // normal surround/center/sub inputs
  int in_gain = settings.preampGain;           // -15..+15
  amp.slot1(in, rec_out, rec_gain, att, lr_in, bass, treble, sl_sr_c_sw_in, in_gain);

  // Compute per-channel trims from balances and center/sub levels.
  // We put trims into the 'gain' parameters (signed), keep volumes equal to baseAtt.
  int fl_gain = clampInt(-settings.frontBalance, -31, 31);
  int fr_gain = clampInt( settings.frontBalance, -31, 31);
  int rl_gain = clampInt(-settings.rearBalance, -31, 31);
  int rr_gain = clampInt( settings.rearBalance, -31, 31);
  int c_gain  = clampInt(settings.centerLevel, -31, 31);
  int sw_gain = clampInt(settings.subLevel, -31, 31);

  int fl_vol = baseAtt;
  int fr_vol = baseAtt;
  int rl_vol = baseAtt;
  int rr_vol = baseAtt;
  int c_vol  = baseAtt;
  int sw_vol = baseAtt;

  // slot2: front L/R
  amp.slot2(fl_gain, fl_vol, fr_gain, fr_vol);

  // slot3: center & subwoofer
  amp.slot3(c_gain, c_vol, sw_gain, sw_vol);

  // slot4: surround L/R
  amp.slot4(rl_gain, rl_vol, rr_gain, rr_vol);
}

// -----------------------------
// UI helpers
// -----------------------------
void showHome() {
  lcd.setCursor(0, 0);
  char line1[17];
  // Example: FT003 V40  M
  snprintf(line1, sizeof(line1), "%-4s V%-3u %c  ", inputName(settings.inputIndex), settings.masterVolume, settings.isMuted ? 'M' : ' ');
  lcd.print(line1);

  lcd.setCursor(0, 1);
  char line2[17];
  // Example: B0 T0 C0 S0
  snprintf(line2, sizeof(line2), "B%+d T%+d C%+d S%+d  ", settings.bass, settings.treble, settings.centerLevel, settings.subLevel);
  lcd.print(line2);
}

const char* menuLabel(uint8_t idx) {
  switch (idx) {
    case 0: return "Volume";
    case 1: return "Bass";
    case 2: return "Treble";
    case 3: return "Input";
    case 4: return "FrBal";
    case 5: return "RrBal";
    case 6: return "Center";
    case 7: return "Sub";
    case 8: return "Gain";
    case 9: return "Mute";
    default: return "?";
  }
}

int menuValue(uint8_t idx) {
  switch (idx) {
    case 0: return settings.masterVolume;
    case 1: return settings.bass;
    case 2: return settings.treble;
    case 3: return settings.inputIndex;
    case 4: return settings.frontBalance;
    case 5: return settings.rearBalance;
    case 6: return settings.centerLevel;
    case 7: return settings.subLevel;
    case 8: return settings.preampGain;
    case 9: return settings.isMuted ? 1 : 0;
    default: return 0;
  }
}

void showDetail() {
  lcd.setCursor(0, 0);
  char line1[17];
  snprintf(line1, sizeof(line1), ">%-6s        ", menuLabel(menuIndex));
  lcd.print(line1);

  lcd.setCursor(0, 1);
  char line2[17];
  if (menuIndex == 3) {
    snprintf(line2, sizeof(line2), "%-6s          ", inputName(settings.inputIndex));
  } else if (menuIndex == 9) {
    snprintf(line2, sizeof(line2), "%s             ", settings.isMuted ? "Muted" : "Unmuted");
  } else {
    snprintf(line2, sizeof(line2), "%+d              ", menuValue(menuIndex));
  }
  lcd.print(line2);
}

// -----------------------------
// Input handling
// -----------------------------
void handleIr() {
#if IRREMOTE_V3_COMPAT
  if (!IrReceiver.decode()) return;
  uint32_t code = IrReceiver.decodedIRData.decodedRawData;
  IrReceiver.resume();
#else
  if (!irrecv.decode(&irResults)) return;
  uint32_t code = irResults.value;
  irrecv.resume();
#endif

  lastInteractionMs = millis();
  digitalWrite(LED_PIN, HIGH);

  switch (code) {
    case STANDBY:
      applyStandby(true);
      break;
    case FT003:
      settings.inputIndex = 0; applyInput(); break;
    case USB:
      settings.inputIndex = 1; applyInput(); break;
    case AUX2:
      settings.inputIndex = 2; applyInput(); break;
    case AUX3:
      settings.inputIndex = 3; applyInput(); break;
    case AUX4:
      settings.inputIndex = 4; applyInput(); break;
    case MUTE_KEY:
      settings.isMuted = !settings.isMuted; applyMute(); break;
    case VOLUME_UP:
      if (settings.masterVolume < 63) { settings.masterVolume++; applyMasterVolume(); }
      break;
    case VOLUME_DOWN:
      if (settings.masterVolume > 0) { settings.masterVolume--; applyMasterVolume(); }
      break;
    case BASS_UP:
      if (settings.bass < 14) { settings.bass++; applyTone(); }
      break;
    case BASS_DOWN:
      if (settings.bass > -14) { settings.bass--; applyTone(); }
      break;
    case TREBLE_UP:
      if (settings.treble < 14) { settings.treble++; applyTone(); }
      break;
    case TREBLE_DOWN:
      if (settings.treble > -14) { settings.treble--; applyTone(); }
      break;
    case FRONT_LR_UP:
      if (settings.frontBalance < 15) { settings.frontBalance++; applyChannelLevels(); }
      break;
    case FRONT_LR_DOWN:
      if (settings.frontBalance > -15) { settings.frontBalance--; applyChannelLevels(); }
      break;
    case REAR_LR_UP:
      if (settings.rearBalance < 15) { settings.rearBalance++; applyChannelLevels(); }
      break;
    case REAR_LR_DOWN:
      if (settings.rearBalance > -15) { settings.rearBalance--; applyChannelLevels(); }
      break;
    case CEN_UP:
      if (settings.centerLevel < 31) { settings.centerLevel++; applyChannelLevels(); }
      break;
    case CEN_DOWN:
      if (settings.centerLevel > -31) { settings.centerLevel--; applyChannelLevels(); }
      break;
    case SUB_UP:
      if (settings.subLevel < 31) { settings.subLevel++; applyChannelLevels(); }
      break;
    case SUB_DOWN:
      if (settings.subLevel > -31) { settings.subLevel--; applyChannelLevels(); }
      break;
    case GAIN_UP:
      if (settings.preampGain < 15) { settings.preampGain++; applyChannelLevels(); }
      break;
    case GAIN_DOWN:
      if (settings.preampGain > -15) { settings.preampGain--; applyChannelLevels(); }
      break;
    default:
      break;
  }

  digitalWrite(LED_PIN, LOW);
}

void handleEncoder() {
  long pos = encoder.read();
  long delta = pos - encoderLast;
  if (delta == 0) return;

  encoderLast = pos;
  lastInteractionMs = millis();

  // Each detent => 4 counts commonly; scale steps
  int steps = (int)(delta / 4);
  if (steps == 0) return;

  if (!inMenu) {
    // Adjust volume
    int newVol = (int)settings.masterVolume + steps;
    if (newVol < 0) newVol = 0;
    if (newVol > 63) newVol = 63;
    if (newVol != settings.masterVolume) {
      settings.masterVolume = (uint8_t)newVol;
      applyMasterVolume();
    }
  } else {
    // Adjust selected menu param
    switch (menuIndex) {
      case 0: { // Volume
        int v = (int)settings.masterVolume + steps;
        settings.masterVolume = (uint8_t)constrain(v, 0, 63);
        applyMasterVolume();
      } break;
      case 1: settings.bass = constrain(settings.bass + steps, -14, 14); applyTone(); break;
      case 2: settings.treble = constrain(settings.treble + steps, -14, 14); applyTone(); break;
      case 3: settings.inputIndex = (uint8_t)constrain((int)settings.inputIndex + steps, 0, 4); applyInput(); break;
      case 4: settings.frontBalance = constrain(settings.frontBalance + steps, -15, 15); applyChannelLevels(); break;
      case 5: settings.rearBalance = constrain(settings.rearBalance + steps, -15, 15); applyChannelLevels(); break;
      case 6: settings.centerLevel = constrain(settings.centerLevel + steps, -31, 31); applyChannelLevels(); break;
      case 7: settings.subLevel = constrain(settings.subLevel + steps, -31, 31); applyChannelLevels(); break;
      case 8: settings.preampGain = constrain(settings.preampGain + steps, -15, 15); applyChannelLevels(); break;
      case 9: settings.isMuted = (steps > 0) ? true : false; applyMute(); break;
      default: break;
    }
  }
}

void handleButton() {
  static bool lastState = HIGH;
  static unsigned long pressedAt = 0;

  bool s = digitalRead(ENCODER_BUTTON_PIN);
  if (s != lastState) {
    delay(5); // debounce
    s = digitalRead(ENCODER_BUTTON_PIN);
  }

  if (s != lastState) {
    lastState = s;
    if (s == LOW) {
      pressedAt = millis();
    } else {
      unsigned long held = millis() - pressedAt;
      if (held > 600) {
        // long press: next menu item
        inMenu = true;
        menuIndex = (menuIndex + 1) % 10;
      } else {
        // short press: toggle menu / toggle mute if already in mute menu
        if (!inMenu) {
          inMenu = true;
          menuIndex = 0; // start at Volume
        } else {
          if (menuIndex == 9) {
            settings.isMuted = !settings.isMuted; applyMute();
          } else {
            inMenu = false;
          }
        }
      }
      lastInteractionMs = millis();
    }
  }
}