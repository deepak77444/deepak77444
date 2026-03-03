/*
 * Ultra Digital 5.1 Home Theater Remote Kit - Ver 3.0
 * Factory-Grade Firmware
 * 
 * MCU: Arduino UNO (ATmega328P)
 * Audio IC: R2S15902FP (5.1 electronic volume processor)
 * Display: HD44780 LCD (16×2 or 20×4) - Parallel mode only
 * Rotary Encoder: Incremental (CLK, DT, SW)
 * IR Receiver: NEC protocol (32-bit)
 * USB MP3 Board: IR-controlled
 * FT003 selector IC: A, B, SW pins
 * 
 * Copyright (c) 2026 Ultra Digital
 * Market-Kit / Factory-Grade Firmware
 */

#include <LiquidCrystal.h>
#include <IRremote.h>
#include <EEPROM.h>

// ==================== PIN DEFINITIONS ====================
// LCD Pins (Parallel Mode - NO I2C)
#define LCD_RS    12
#define LCD_EN    11
#define LCD_D4    5
#define LCD_D5    4
#define LCD_D6    3
#define LCD_D7    2

// IR Receiver
#define IR_PIN    6

// Rotary Encoder
#define ENCODER_CLK  7
#define ENCODER_DT   8
#define ENCODER_SW   9

// FT003 Selector IC
#define FT003_A   A0
#define FT003_B   A1
#define FT003_SW  A2

// USB Control
#define USB_POWER  10

// R2S15902FP Audio IC Control (SPI/I2C - depends on IC interface)
#define AUDIO_IC_CS   13
#define AUDIO_IC_CLK  A3
#define AUDIO_IC_DATA A4

// ==================== IR REMOTE HEX CODES (NEC 32-bit) ====================
// SYSTEM
#define IR_POWER        0x807F827D
#define IR_MUTE         0x807F42BD
#define IR_RESET        0x807F1AE5
#define IR_SURROUND     0x807FA857
#define IR_TEST_TONE    0x807F02FD

// INPUT
#define IR_AUX1         0x807F52AD
#define IR_AUX2         0x807FA25D
#define IR_AUX3         0x807F22DD
#define IR_USB          0x807F629D

// MASTER VOLUME
#define IR_VOL_UP       0x807F906F
#define IR_VOL_DOWN     0x807FA05F

// CHANNEL VOLUME
#define IR_FRONT_UP     0x807F40BF
#define IR_FRONT_DOWN   0x807FC03F
#define IR_SURR_UP      0x807F00FF
#define IR_SURR_DOWN    0x807F807F
#define IR_CENTER_UP    0x807F50AF
#define IR_CENTER_DOWN  0x807F609F
#define IR_SUB_UP       0x807FD02F
#define IR_SUB_DOWN     0x807FE01F

// TONE / GAIN
#define IR_BASS_UP      0x807F48B7
#define IR_BASS_DOWN    0x807FC837
#define IR_TREBLE_UP    0x807F08F7
#define IR_TREBLE_DOWN  0x807F8877
#define IR_GAIN_UP      0x807F926D
#define IR_GAIN_DOWN    0x807FB04F

// USB MEDIA
#define IR_PLAY_PAUSE   0xFF30CF
#define IR_NEXT         0xFFA25D
#define IR_PREV         0xFFE21D
#define IR_MODE         0xFF6897
#define IR_EQ           0xFF20DF

// NUMERIC KEYS
#define IR_NUM_0        0x807F28D7
#define IR_NUM_1        0x807F18E7
#define IR_NUM_2        0x807F9867
#define IR_NUM_3        0x807F58A7
#define IR_NUM_4        0x807F30CF
#define IR_NUM_5        0x807FB04F
#define IR_NUM_6        0x807F708F
#define IR_NUM_7        0x807FF00F
#define IR_NUM_8        0x807F38C7
#define IR_NUM_9        0x807FB847

// ==================== EEPROM ADDRESSES ====================
#define EEPROM_MASTER_VOL     0
#define EEPROM_FRONT_VOL      1
#define EEPROM_SURROUND_VOL   2
#define EEPROM_CENTER_VOL     3
#define EEPROM_SUB_VOL        4
#define EEPROM_GAIN           5
#define EEPROM_BASS           6
#define EEPROM_TREBLE         7
#define EEPROM_INPUT_SELECT   8
#define EEPROM_MODEL_TYPE     9
#define EEPROM_USB_BOARD      10
#define EEPROM_FT003_MODE     11
#define EEPROM_SURROUND_MODE  12
#define EEPROM_WELCOME_NAME   20  // 20-35 (16 chars)

// ==================== MODE DEFINITIONS ====================
enum EncoderMode {
  MODE_MASTER = 0,
  MODE_FRONT,
  MODE_SURROUND,
  MODE_CENTER,
  MODE_SUB,
  MODE_GAIN,
  MODE_BASS,
  MODE_TREBLE,
  MODE_USB,
  MODE_AUX1,
  MODE_AUX2,
  MODE_AUX3,
  MODE_DVD,
  MODE_COUNT
};

enum InputSource {
  INPUT_AUX1 = 0,
  INPUT_AUX2,
  INPUT_AUX3,
  INPUT_USB,
  INPUT_DVD,
  INPUT_COUNT
};

enum FT003Mode {
  FT_AUX = 0,
  FT_OPTIC,
  FT_COAX,
  FT_HDMI,
  FT_COUNT
};

enum TestToneChannel {
  TEST_FL = 0,
  TEST_FR,
  TEST_SR,
  TEST_SL,
  TEST_CENTER,
  TEST_SUB,
  TEST_COUNT
};

// ==================== GLOBAL VARIABLES ====================
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
IRrecv irrecv(IR_PIN);
decode_results results;

// System State
bool standbyMode = false;
bool muteActive = false;
bool surroundOn = true;
bool testToneMode = false;
bool welcomeEditMode = false;
bool modelSetMode = false;

// Volume Levels (0-100)
int masterVolume = 50;
int frontVolume = 50;
int surroundVolume = 50;
int centerVolume = 50;
int subVolume = 50;

// Tone Controls (-10 to +10)
int bassLevel = 0;
int trebleLevel = 0;
int gainLevel = 0;

// Current Modes
EncoderMode currentMode = MODE_MASTER;
InputSource currentInput = INPUT_AUX1;
FT003Mode ft003Mode = FT_AUX;
TestToneChannel testToneChannel = TEST_FL;
int usbBoardType = 0; // 0=MP3, 1=Real-Play, 2=Wire
int modelType = 1;

// Rotary Encoder State
int lastEncoderCLK = HIGH;
unsigned long lastEncoderActivity = 0;
unsigned long encoderPressTime = 0;
bool encoderPressed = false;

// Welcome Message
char welcomeName[17] = "ULTRA DIGITAL   ";

// Display Update
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 100

// Encoder Auto-Return Timer
#define ENCODER_RETURN_TIMEOUT 5000

// Long Press Duration
#define LONG_PRESS_DURATION 2000

// ==================== SETUP ====================
void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  Serial.println(F("Ultra Digital 5.1 Home Theater v3.0"));
  
  // Initialize LCD (16x2 or 20x4)
  lcd.begin(16, 2);
  lcd.clear();
  
  // Display welcome message
  displayWelcomeScreen();
  delay(2000);
  
  // Initialize IR Receiver
  irrecv.enableIRIn();
  
  // Initialize Rotary Encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Initialize FT003
  pinMode(FT003_A, OUTPUT);
  pinMode(FT003_B, OUTPUT);
  pinMode(FT003_SW, OUTPUT);
  
  // Initialize USB Control
  pinMode(USB_POWER, OUTPUT);
  digitalWrite(USB_POWER, HIGH);
  
  // Initialize Audio IC
  pinMode(AUDIO_IC_CS, OUTPUT);
  pinMode(AUDIO_IC_CLK, OUTPUT);
  pinMode(AUDIO_IC_DATA, OUTPUT);
  digitalWrite(AUDIO_IC_CS, HIGH);
  
  // Load settings from EEPROM
  loadSettings();
  
  // Initialize Audio IC
  initAudioIC();
  
  // Set initial volume and tone
  updateAllAudioSettings();
  
  // Display main screen
  updateDisplay();
}

// ==================== MAIN LOOP ====================
void loop() {
  // Handle IR Remote
  handleIR();
  
  // Handle Rotary Encoder
  handleEncoder();
  
  // Handle Encoder Auto-Return
  if (!standbyMode && currentMode != MODE_MASTER) {
    if (millis() - lastEncoderActivity > ENCODER_RETURN_TIMEOUT) {
      currentMode = MODE_MASTER;
      updateDisplay();
    }
  }
  
  // Update Display
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
}

// ==================== IR HANDLER ====================
void handleIR() {
  if (irrecv.decode(&results)) {
    unsigned long irCode = results.value;
    
    // Ignore repeat codes
    if (irCode != 0xFFFFFFFF) {
      processIRCommand(irCode);
    }
    
    irrecv.resume();
  }
}

void processIRCommand(unsigned long code) {
  Serial.print(F("IR Code: 0x"));
  Serial.println(code, HEX);
  
  // SYSTEM COMMANDS
  if (code == IR_POWER) {
    toggleStandby();
  }
  else if (code == IR_MUTE) {
    toggleMute();
  }
  else if (code == IR_RESET) {
    resetToDefaults();
  }
  else if (code == IR_SURROUND) {
    toggleSurround();
  }
  else if (code == IR_TEST_TONE) {
    toggleTestTone();
  }
  
  // INPUT SELECTION
  else if (code == IR_AUX1) {
    selectInput(INPUT_AUX1);
  }
  else if (code == IR_AUX2) {
    selectInput(INPUT_AUX2);
  }
  else if (code == IR_AUX3) {
    selectInput(INPUT_AUX3);
  }
  else if (code == IR_USB) {
    selectInput(INPUT_USB);
  }
  
  // MASTER VOLUME
  else if (code == IR_VOL_UP) {
    adjustVolume(masterVolume, 1);
    updateAudioVolume();
  }
  else if (code == IR_VOL_DOWN) {
    adjustVolume(masterVolume, -1);
    updateAudioVolume();
  }
  
  // CHANNEL VOLUME
  else if (code == IR_FRONT_UP) {
    adjustVolume(frontVolume, 1);
    updateAudioVolume();
  }
  else if (code == IR_FRONT_DOWN) {
    adjustVolume(frontVolume, -1);
    updateAudioVolume();
  }
  else if (code == IR_SURR_UP) {
    adjustVolume(surroundVolume, 1);
    updateAudioVolume();
  }
  else if (code == IR_SURR_DOWN) {
    adjustVolume(surroundVolume, -1);
    updateAudioVolume();
  }
  else if (code == IR_CENTER_UP) {
    adjustVolume(centerVolume, 1);
    updateAudioVolume();
  }
  else if (code == IR_CENTER_DOWN) {
    adjustVolume(centerVolume, -1);
    updateAudioVolume();
  }
  else if (code == IR_SUB_UP) {
    adjustVolume(subVolume, 1);
    updateAudioVolume();
  }
  else if (code == IR_SUB_DOWN) {
    adjustVolume(subVolume, -1);
    updateAudioVolume();
  }
  
  // TONE CONTROLS
  else if (code == IR_BASS_UP) {
    adjustTone(bassLevel, 1);
    updateAudioTone();
  }
  else if (code == IR_BASS_DOWN) {
    adjustTone(bassLevel, -1);
    updateAudioTone();
  }
  else if (code == IR_TREBLE_UP) {
    adjustTone(trebleLevel, 1);
    updateAudioTone();
  }
  else if (code == IR_TREBLE_DOWN) {
    adjustTone(trebleLevel, -1);
    updateAudioTone();
  }
  else if (code == IR_GAIN_UP) {
    adjustTone(gainLevel, 1);
    updateAudioGain();
  }
  else if (code == IR_GAIN_DOWN) {
    adjustTone(gainLevel, -1);
    updateAudioGain();
  }
  
  // USB MEDIA CONTROLS
  else if (code == IR_PLAY_PAUSE) {
    sendUSBCommand(IR_PLAY_PAUSE);
  }
  else if (code == IR_NEXT) {
    sendUSBCommand(IR_NEXT);
  }
  else if (code == IR_PREV) {
    sendUSBCommand(IR_PREV);
  }
  else if (code == IR_MODE) {
    sendUSBCommand(IR_MODE);
  }
  else if (code == IR_EQ) {
    sendUSBCommand(IR_EQ);
  }
  
  // NUMERIC KEYS
  else if (code == IR_NUM_0) {
    handleNumericKey(0);
  }
  else if (code == IR_NUM_1) {
    handleNumericKey(1);
  }
  else if (code == IR_NUM_2) {
    handleNumericKey(2);
  }
  else if (code == IR_NUM_3) {
    handleNumericKey(3);
  }
  else if (code == IR_NUM_4) {
    handleNumericKey(4);
  }
  else if (code == IR_NUM_5) {
    handleNumericKey(5);
  }
  else if (code == IR_NUM_6) {
    handleNumericKey(6);
  }
  else if (code == IR_NUM_7) {
    handleNumericKey(7);
  }
  else if (code == IR_NUM_8) {
    handleNumericKey(8);
  }
  else if (code == IR_NUM_9) {
    handleNumericKey(9);
  }
  
  updateDisplay();
}

// ==================== ROTARY ENCODER HANDLER ====================
void handleEncoder() {
  // Read rotation
  int currentCLK = digitalRead(ENCODER_CLK);
  
  if (currentCLK != lastEncoderCLK && currentCLK == LOW) {
    int dtValue = digitalRead(ENCODER_DT);
    
    if (dtValue == HIGH) {
      // Clockwise rotation - Volume Up
      handleEncoderRotation(1);
    } else {
      // Counter-clockwise rotation - Volume Down
      handleEncoderRotation(-1);
    }
    
    lastEncoderActivity = millis();
    updateDisplay();
  }
  lastEncoderCLK = currentCLK;
  
  // Handle button press
  int swState = digitalRead(ENCODER_SW);
  
  if (swState == LOW && !encoderPressed) {
    encoderPressed = true;
    encoderPressTime = millis();
  }
  else if (swState == HIGH && encoderPressed) {
    unsigned long pressDuration = millis() - encoderPressTime;
    encoderPressed = false;
    
    if (pressDuration >= LONG_PRESS_DURATION) {
      // Long press - Standby
      toggleStandby();
    } else {
      // Short press - Mode cycle
      cycleEncoderMode();
    }
    
    lastEncoderActivity = millis();
    updateDisplay();
  }
}

void handleEncoderRotation(int direction) {
  if (standbyMode) return;
  
  if (testToneMode) {
    // In test tone mode, adjust only active channel
    adjustTestToneVolume(direction);
  } else {
    // Normal mode - adjust based on current mode
    switch (currentMode) {
      case MODE_MASTER:
        adjustVolume(masterVolume, direction);
        break;
      case MODE_FRONT:
        adjustVolume(frontVolume, direction);
        break;
      case MODE_SURROUND:
        adjustVolume(surroundVolume, direction);
        break;
      case MODE_CENTER:
        adjustVolume(centerVolume, direction);
        break;
      case MODE_SUB:
        adjustVolume(subVolume, direction);
        break;
      case MODE_GAIN:
        adjustTone(gainLevel, direction);
        updateAudioGain();
        return;
      case MODE_BASS:
        adjustTone(bassLevel, direction);
        updateAudioTone();
        return;
      case MODE_TREBLE:
        adjustTone(trebleLevel, direction);
        updateAudioTone();
        return;
      case MODE_USB:
      case MODE_AUX1:
      case MODE_AUX2:
      case MODE_AUX3:
      case MODE_DVD:
        // Input selection via encoder
        selectNextInput(direction);
        return;
    }
    updateAudioVolume();
  }
}

void cycleEncoderMode() {
  if (standbyMode) return;
  
  currentMode = (EncoderMode)((currentMode + 1) % MODE_COUNT);
  lastEncoderActivity = millis();
}

// ==================== VOLUME & TONE FUNCTIONS ====================
void adjustVolume(int &volume, int delta) {
  volume += delta;
  if (volume < 0) volume = 0;
  if (volume > 100) volume = 100;
}

void adjustTone(int &tone, int delta) {
  tone += delta;
  if (tone < -10) tone = -10;
  if (tone > 10) tone = 10;
}

// ==================== SYSTEM FUNCTIONS ====================
void toggleStandby() {
  standbyMode = !standbyMode;
  
  if (standbyMode) {
    // Enter standby
    muteActive = true;
    lcd.noDisplay();
    saveSettings();
  } else {
    // Exit standby
    muteActive = false;
    lcd.display();
    updateAllAudioSettings();
  }
}

void toggleMute() {
  if (standbyMode) return;
  
  muteActive = !muteActive;
  updateAudioVolume();
}

void toggleSurround() {
  if (standbyMode) return;
  
  surroundOn = !surroundOn;
  updateAudioSurround();
}

void toggleTestTone() {
  if (standbyMode) return;
  
  testToneMode = !testToneMode;
  
  if (testToneMode) {
    testToneChannel = TEST_FL;
    activateTestTone();
  } else {
    deactivateTestTone();
  }
}

void resetToDefaults() {
  masterVolume = 50;
  frontVolume = 50;
  surroundVolume = 50;
  centerVolume = 50;
  subVolume = 50;
  bassLevel = 0;
  trebleLevel = 0;
  gainLevel = 0;
  currentInput = INPUT_AUX1;
  surroundOn = true;
  
  updateAllAudioSettings();
  lcd.clear();
  lcd.print(F("RESET TO DEFAULT"));
  delay(1000);
}

void selectInput(InputSource input) {
  if (standbyMode) return;
  
  currentInput = input;
  
  // Control USB power
  if (input == INPUT_USB) {
    digitalWrite(USB_POWER, HIGH);
  } else {
    digitalWrite(USB_POWER, LOW);
  }
  
  updateAudioInput();
  saveSettings();
}

void selectNextInput(int direction) {
  int newInput = (int)currentInput + direction;
  if (newInput < 0) newInput = INPUT_COUNT - 1;
  if (newInput >= INPUT_COUNT) newInput = 0;
  
  selectInput((InputSource)newInput);
}

// ==================== TEST TONE MODE ====================
void activateTestTone() {
  // Activate test tone on first channel
  sendAudioICCommand(0x10, testToneChannel);
}

void deactivateTestTone() {
  // Deactivate test tone
  sendAudioICCommand(0x10, 0xFF);
}

void adjustTestToneVolume(int direction) {
  // Adjust volume of active test tone channel
  switch (testToneChannel) {
    case TEST_FL:
    case TEST_FR:
      adjustVolume(frontVolume, direction);
      break;
    case TEST_SL:
    case TEST_SR:
      adjustVolume(surroundVolume, direction);
      break;
    case TEST_CENTER:
      adjustVolume(centerVolume, direction);
      break;
    case TEST_SUB:
      adjustVolume(subVolume, direction);
      break;
  }
  updateAudioVolume();
}

void cycleTestToneChannel() {
  testToneChannel = (TestToneChannel)((testToneChannel + 1) % TEST_COUNT);
  activateTestTone();
}

// ==================== SPECIAL MODES ====================
void handleNumericKey(int num) {
  if (welcomeEditMode) {
    handleWelcomeEdit(num);
  } else if (modelSetMode) {
    handleModelSet(num);
  } else {
    // Check for special mode activation
    static unsigned long lastNum1Press = 0;
    static unsigned long lastNum0Press = 0;
    static unsigned long lastAux1Press = 0;
    static unsigned long last4Press = 0;
    static unsigned long last5Press = 0;
    static unsigned long last6Press = 0;
    
    unsigned long now = millis();
    
    if (num == 1) {
      if (now - lastNum0Press < 1000) {
        // Long-press NUM-1 + NUM-0 detected
        enterModelSetMode();
      }
      lastNum1Press = now;
    }
    else if (num == 0) {
      lastNum0Press = now;
    }
    else if (num == 4) {
      if (now - last4Press < 500) {
        // Double press = long press
        usbBoardType = 0; // USB MP3
        saveSettings();
      }
      last4Press = now;
    }
    else if (num == 5) {
      if (now - last5Press < 500) {
        usbBoardType = 1; // USB Real-Play
        saveSettings();
      }
      last5Press = now;
    }
    else if (num == 6) {
      if (now - last6Press < 500) {
        usbBoardType = 2; // USB Wire
        saveSettings();
      }
      last6Press = now;
    }
  }
}

void enterWelcomeEditMode() {
  welcomeEditMode = true;
  lcd.clear();
  lcd.print(F("EDIT NAME:"));
  lcd.setCursor(0, 1);
  lcd.print(welcomeName);
}

void handleWelcomeEdit(int num) {
  // Implementation for welcome name editing
  // 1-6: scroll letters, 0: space, NEXT: next char, RESET: exit & save
}

void enterModelSetMode() {
  modelSetMode = true;
  lcd.clear();
  lcd.print(F("MODEL SELECT:"));
  lcd.setCursor(0, 1);
  lcd.print(F("Press 1-7"));
}

void handleModelSet(int num) {
  if (num >= 1 && num <= 7) {
    modelType = num;
    saveSettings();
    modelSetMode = false;
  } else if (num == 0) {
    // A/B mode toggle
    modelType = (modelType == 0) ? 1 : 0;
  }
}

// ==================== FT003 CONTROL ====================
void setFT003Mode(FT003Mode mode) {
  ft003Mode = mode;
  
  // FT003 truth table:
  // AUX:   A=0, B=0
  // OPTIC: A=0, B=1
  // COAX:  A=1, B=0
  // HDMI:  A=1, B=1
  
  switch (mode) {
    case FT_AUX:
      digitalWrite(FT003_A, LOW);
      digitalWrite(FT003_B, LOW);
      break;
    case FT_OPTIC:
      digitalWrite(FT003_A, LOW);
      digitalWrite(FT003_B, HIGH);
      break;
    case FT_COAX:
      digitalWrite(FT003_A, HIGH);
      digitalWrite(FT003_B, LOW);
      break;
    case FT_HDMI:
      digitalWrite(FT003_A, HIGH);
      digitalWrite(FT003_B, HIGH);
      break;
  }
  
  saveSettings();
}

void cycleFT003Mode() {
  ft003Mode = (FT003Mode)((ft003Mode + 1) % FT_COUNT);
  setFT003Mode(ft003Mode);
}

// ==================== AUDIO IC CONTROL (R2S15902FP) ====================
void initAudioIC() {
  // Initialize R2S15902FP
  // Send initialization sequence
  sendAudioICCommand(0x00, 0x00); // Reset
  delay(10);
  sendAudioICCommand(0x01, 0xFF); // Power on all channels
  delay(10);
}

void sendAudioICCommand(byte reg, byte data) {
  // Simple SPI-like communication
  digitalWrite(AUDIO_IC_CS, LOW);
  delayMicroseconds(1);
  
  // Send register address
  shiftOut(AUDIO_IC_DATA, AUDIO_IC_CLK, MSBFIRST, reg);
  
  // Send data
  shiftOut(AUDIO_IC_DATA, AUDIO_IC_CLK, MSBFIRST, data);
  
  delayMicroseconds(1);
  digitalWrite(AUDIO_IC_CS, HIGH);
}

void updateAudioVolume() {
  if (muteActive) {
    sendAudioICCommand(0x20, 0);
    return;
  }
  
  // Map volume 0-100 to IC range (typically 0-63 or 0-79)
  byte masterVol = map(masterVolume, 0, 100, 0, 79);
  byte frontVol = map(frontVolume, 0, 100, 0, 79);
  byte surrVol = map(surroundVolume, 0, 100, 0, 79);
  byte centerVol = map(centerVolume, 0, 100, 0, 79);
  byte subVol = map(subVolume, 0, 100, 0, 79);
  
  // Send to R2S15902FP
  sendAudioICCommand(0x20, masterVol);      // Master volume
  sendAudioICCommand(0x21, frontVol);       // Front L
  sendAudioICCommand(0x22, frontVol);       // Front R
  sendAudioICCommand(0x23, surrVol);        // Surround L
  sendAudioICCommand(0x24, surrVol);        // Surround R
  sendAudioICCommand(0x25, centerVol);      // Center
  sendAudioICCommand(0x26, subVol);         // Subwoofer
}

void updateAudioTone() {
  // Map tone -10 to +10 to IC range
  byte bassVal = map(bassLevel, -10, 10, 0, 20);
  byte trebleVal = map(trebleLevel, -10, 10, 0, 20);
  
  sendAudioICCommand(0x30, bassVal);
  sendAudioICCommand(0x31, trebleVal);
}

void updateAudioGain() {
  byte gainVal = map(gainLevel, -10, 10, 0, 20);
  sendAudioICCommand(0x32, gainVal);
}

void updateAudioInput() {
  // Select input on audio IC
  sendAudioICCommand(0x40, (byte)currentInput);
}

void updateAudioSurround() {
  sendAudioICCommand(0x50, surroundOn ? 0x01 : 0x00);
}

void updateAllAudioSettings() {
  updateAudioVolume();
  updateAudioTone();
  updateAudioGain();
  updateAudioInput();
  updateAudioSurround();
}

// ==================== USB CONTROL ====================
void sendUSBCommand(unsigned long cmd) {
  // For IR-controlled USB boards, we might need to retransmit IR
  // or use serial communication depending on the board type
  
  // This is a placeholder - actual implementation depends on USB board interface
  Serial.print(F("USB CMD: 0x"));
  Serial.println(cmd, HEX);
}

// ==================== DISPLAY FUNCTIONS ====================
void displayWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(welcomeName);
  lcd.setCursor(0, 1);
  lcd.print(F("Ver 3.0"));
}

void updateDisplay() {
  if (standbyMode) return;
  
  lcd.clear();
  
  if (testToneMode) {
    displayTestTone();
  } else if (welcomeEditMode) {
    // Welcome edit display
    lcd.setCursor(0, 0);
    lcd.print(F("EDIT NAME:"));
    lcd.setCursor(0, 1);
    lcd.print(welcomeName);
  } else if (modelSetMode) {
    // Model set display
    lcd.setCursor(0, 0);
    lcd.print(F("MODEL:"));
    lcd.print(modelType);
  } else {
    displayMainScreen();
  }
}

void displayMainScreen() {
  // Line 1: Mode/Input and Volume
  lcd.setCursor(0, 0);
  
  switch (currentMode) {
    case MODE_MASTER:
      lcd.print(F("VOL"));
      break;
    case MODE_FRONT:
      lcd.print(F("FRN"));
      break;
    case MODE_SURROUND:
      lcd.print(F("SUR"));
      break;
    case MODE_CENTER:
      lcd.print(F("CEN"));
      break;
    case MODE_SUB:
      lcd.print(F("SUB"));
      break;
    case MODE_GAIN:
      lcd.print(F("GAN"));
      break;
    case MODE_BASS:
      lcd.print(F("BAS"));
      break;
    case MODE_TREBLE:
      lcd.print(F("TRE"));
      break;
    default:
      lcd.print(F("INP"));
      break;
  }
  
  lcd.print(F(":"));
  
  // Display appropriate value
  int displayValue = 0;
  switch (currentMode) {
    case MODE_MASTER:
      displayValue = masterVolume;
      break;
    case MODE_FRONT:
      displayValue = frontVolume;
      break;
    case MODE_SURROUND:
      displayValue = surroundVolume;
      break;
    case MODE_CENTER:
      displayValue = centerVolume;
      break;
    case MODE_SUB:
      displayValue = subVolume;
      break;
    case MODE_GAIN:
      displayValue = gainLevel + 10; // Offset for display
      break;
    case MODE_BASS:
      displayValue = bassLevel + 10;
      break;
    case MODE_TREBLE:
      displayValue = trebleLevel + 10;
      break;
  }
  
  if (displayValue < 10) lcd.print(F(" "));
  if (displayValue < 100) lcd.print(F(" "));
  lcd.print(displayValue);
  
  // Display status indicators
  lcd.setCursor(8, 0);
  if (muteActive) lcd.print(F("M"));
  if (surroundOn) lcd.print(F("S"));
  
  // Line 2: Input source and FT003
  lcd.setCursor(0, 1);
  switch (currentInput) {
    case INPUT_AUX1:
      lcd.print(F("AUX-1"));
      break;
    case INPUT_AUX2:
      lcd.print(F("AUX-2"));
      break;
    case INPUT_AUX3:
      lcd.print(F("AUX-3"));
      break;
    case INPUT_USB:
      lcd.print(F("USB  "));
      break;
    case INPUT_DVD:
      lcd.print(F("DVD  "));
      break;
  }
  
  lcd.setCursor(9, 1);
  lcd.print(F("FT:"));
  switch (ft003Mode) {
    case FT_AUX:
      lcd.print(F("AUX"));
      break;
    case FT_OPTIC:
      lcd.print(F("OPT"));
      break;
    case FT_COAX:
      lcd.print(F("COX"));
      break;
    case FT_HDMI:
      lcd.print(F("HDM"));
      break;
  }
}

void displayTestTone() {
  lcd.setCursor(0, 0);
  lcd.print(F("TEST TONE MODE"));
  
  lcd.setCursor(0, 1);
  switch (testToneChannel) {
    case TEST_FL:
      lcd.print(F("FRONT LEFT"));
      break;
    case TEST_FR:
      lcd.print(F("FRONT RIGHT"));
      break;
    case TEST_SR:
      lcd.print(F("SURROUND R"));
      break;
    case TEST_SL:
      lcd.print(F("SURROUND L"));
      break;
    case TEST_CENTER:
      lcd.print(F("CENTER"));
      break;
    case TEST_SUB:
      lcd.print(F("SUBWOOFER"));
      break;
  }
}

// ==================== EEPROM FUNCTIONS ====================
void saveSettings() {
  EEPROM.update(EEPROM_MASTER_VOL, masterVolume);
  EEPROM.update(EEPROM_FRONT_VOL, frontVolume);
  EEPROM.update(EEPROM_SURROUND_VOL, surroundVolume);
  EEPROM.update(EEPROM_CENTER_VOL, centerVolume);
  EEPROM.update(EEPROM_SUB_VOL, subVolume);
  EEPROM.update(EEPROM_GAIN, gainLevel + 10);
  EEPROM.update(EEPROM_BASS, bassLevel + 10);
  EEPROM.update(EEPROM_TREBLE, trebleLevel + 10);
  EEPROM.update(EEPROM_INPUT_SELECT, currentInput);
  EEPROM.update(EEPROM_MODEL_TYPE, modelType);
  EEPROM.update(EEPROM_USB_BOARD, usbBoardType);
  EEPROM.update(EEPROM_FT003_MODE, ft003Mode);
  EEPROM.update(EEPROM_SURROUND_MODE, surroundOn ? 1 : 0);
  
  // Save welcome name
  for (int i = 0; i < 16; i++) {
    EEPROM.update(EEPROM_WELCOME_NAME + i, welcomeName[i]);
  }
}

void loadSettings() {
  masterVolume = EEPROM.read(EEPROM_MASTER_VOL);
  frontVolume = EEPROM.read(EEPROM_FRONT_VOL);
  surroundVolume = EEPROM.read(EEPROM_SURROUND_VOL);
  centerVolume = EEPROM.read(EEPROM_CENTER_VOL);
  subVolume = EEPROM.read(EEPROM_SUB_VOL);
  gainLevel = EEPROM.read(EEPROM_GAIN) - 10;
  bassLevel = EEPROM.read(EEPROM_BASS) - 10;
  trebleLevel = EEPROM.read(EEPROM_TREBLE) - 10;
  currentInput = (InputSource)EEPROM.read(EEPROM_INPUT_SELECT);
  modelType = EEPROM.read(EEPROM_MODEL_TYPE);
  usbBoardType = EEPROM.read(EEPROM_USB_BOARD);
  ft003Mode = (FT003Mode)EEPROM.read(EEPROM_FT003_MODE);
  surroundOn = EEPROM.read(EEPROM_SURROUND_MODE) == 1;
  
  // Load welcome name
  for (int i = 0; i < 16; i++) {
    welcomeName[i] = EEPROM.read(EEPROM_WELCOME_NAME + i);
    if (welcomeName[i] == 0xFF) welcomeName[i] = ' '; // Handle uninitialized EEPROM
  }
  welcomeName[16] = '\0';
  
  // Validate loaded values
  if (masterVolume > 100) masterVolume = 50;
  if (frontVolume > 100) frontVolume = 50;
  if (surroundVolume > 100) surroundVolume = 50;
  if (centerVolume > 100) centerVolume = 50;
  if (subVolume > 100) subVolume = 50;
  if (gainLevel < -10 || gainLevel > 10) gainLevel = 0;
  if (bassLevel < -10 || bassLevel > 10) bassLevel = 0;
  if (trebleLevel < -10 || trebleLevel > 10) trebleLevel = 0;
  if (currentInput >= INPUT_COUNT) currentInput = INPUT_AUX1;
  if (ft003Mode >= FT_COUNT) ft003Mode = FT_AUX;
}
