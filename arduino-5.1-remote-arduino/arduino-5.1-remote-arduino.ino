// Arduino 5.1 Remote Kit Controller (IR + Audio IC abstraction)
// Supports: PT2351 over I2C (requires command table), R2S15902FP via 3-wire bit-bang (requires command table)
// Optional: run I2C scanner from Serial to discover devices
//
// How to use:
// - Select one AUDIO_IC_* option below
// - For your chip, fill the command table constants in the CONFIG SECTION matching your datasheet
// - Wire IR receiver to IR_PIN, and (optionally) 16x2 LCD over I2C (not required)
// - Upload and use Serial Monitor for scanner/help

#include <Arduino.h>
#include <Wire.h>

// IRremote v3/v4 compatibility
#if __has_include(<IRremote.hpp>)
  #include <IRremote.hpp>
  #define IRREMOTE_V4
#else
  #include <IRremote.h>
#endif

// ========================= USER CONFIGURATION =========================

// Select exactly one audio IC target
//#define AUDIO_IC_PT2351_I2C
//#define AUDIO_IC_R2S15902FP_3WIRE
#define AUDIO_IC_NULL // Fallback that logs but does not drive hardware

// IR receiver pin
#ifndef IR_RECEIVE_PIN
  #define IR_RECEIVE_PIN 2
#endif

// Enable Serial help and I2C scan command menu
#define ENABLE_SERIAL_MENU 1

// I2C address for PT2351 (7-bit). Set based on your board (common: 0x44 or 0x46)
#define PT2351_I2C_ADDR 0x44

// R2S15902FP 2-wire control pins (set to your wiring)
#define R2S15902_CLK_PIN 8
#define R2S15902_DATA_PIN 9

// IR codes mapping (NEC or as per your remote). Replace with your remote's codes.
// Use 'irlearn' command in Serial to print incoming codes, then update these values.
#define IR_CODE_POWER      0xFFA25DUL
#define IR_CODE_MUTE       0xFFE21DUL
#define IR_CODE_VOL_UP     0xFF629DUL
#define IR_CODE_VOL_DOWN   0xFFA857UL
#define IR_CODE_INPUT_NEXT 0xFF22DDUL
#define IR_CODE_INPUT_PREV 0xFF02FDUL

// ---- CONFIG SECTION: Command tables (MUST be filled per datasheet) ----
// These are placeholders. Fill sequences as required by your IC.
// The code will gracefully no-op when tables are empty (length 0).

// PT2351 command sequences: examples (each entry is a 2-6 byte sequence)
// Provide functions to build commands below.
// For safety, these defaults are empty (no-op) until you configure.

// Map desired master volume [0..100] to one or more write sequences
static void pt2351_buildVolumeCommand(uint8_t percent, uint8_t* outBuf, size_t& outLen) {
  // TODO by user: use PT2351 datasheet to fill command bytes
  // Example shape (replace 0xAA,0xBB with real values):
  // outBuf[0] = 0xAA; outBuf[1] = map(percent, 0,100, 0x??, 0x??); outLen = 2;
  outLen = 0; // no-op until configured
}

// Select input [0..N-1]
static void pt2351_buildInputSelect(uint8_t inputIndex, uint8_t* outBuf, size_t& outLen) {
  outLen = 0; // fill per datasheet
}

// Set tone: bass and treble in [-14..+14] dB, step 2 dB
static void pt2351_buildTone(int8_t bassDb, int8_t trebleDb, uint8_t* outBuf, size_t& outLen) {
  outLen = 0; // fill per datasheet
}

// R2S15902FP uses 3-wire serial (CLK/DATA/LATCH). Build bitstreams MSB-first.
// Provide up to 24-bit frames; encode into outBits (LSB of uint32_t is lowest bit sent last if you flip order as needed).
static void r2s15902_buildVolumeBits(uint8_t percent, uint32_t& outBits, uint8_t& outLenBits) {
  // Fill from datasheet: set proper address/command fields
  outLenBits = 0; // no-op until configured
}

static void r2s15902_buildInputBits(uint8_t inputIndex, uint32_t& outBits, uint8_t& outLenBits) {
  outLenBits = 0; // no-op until configured
}

static void r2s15902_buildToneBits(int8_t bassDb, int8_t trebleDb, uint32_t& outBits, uint8_t& outLenBits) {
  outLenBits = 0; // no-op until configured
}

// ========================= END USER CONFIG ===========================

// Abstraction for audio processor
class AudioChip {
public:
  virtual void begin() = 0;
  virtual void setMasterVolumePercent(uint8_t volumePercent) = 0; // 0..100
  virtual void setMute(bool mute) = 0;
  virtual void setInput(uint8_t inputIndex) = 0; // 0..N-1
  virtual void setTone(int8_t bassDb, int8_t trebleDb) = 0; // -14..+14 dB
  virtual const char* name() const = 0;
  virtual ~AudioChip() {}
};

// Null driver (logs only)
class AudioChipNull : public AudioChip {
  bool isMuted = false;
  uint8_t volume = 50;
  uint8_t inputIdx = 0;
  int8_t bass = 0, treble = 0;
public:
  void begin() override {}
  void setMasterVolumePercent(uint8_t v) override {
    volume = constrain(v, 0u, 100u);
    Serial.print(F("[Null] Volume %")); Serial.println(volume);
  }
  void setMute(bool m) override {
    isMuted = m; Serial.print(F("[Null] Mute ")); Serial.println(isMuted ? F("ON") : F("OFF"));
  }
  void setInput(uint8_t idx) override {
    inputIdx = idx; Serial.print(F("[Null] Input ")); Serial.println(inputIdx);
  }
  void setTone(int8_t b, int8_t t) override {
    bass = b; treble = t; Serial.print(F("[Null] Bass/Treble "));
    Serial.print(bass); Serial.print('/'); Serial.println(treble);
  }
  const char* name() const override { return "Null"; }
};

#ifdef AUDIO_IC_PT2351_I2C
class AudioChipPT2351 : public AudioChip {
  bool isMuted = false;
  uint8_t volume = 50;
  uint8_t inputIdx = 0;
  int8_t bass = 0, treble = 0;
  void i2cWriteCmd(const uint8_t* bytes, size_t len) {
    if (len == 0) return;
    Wire.beginTransmission(PT2351_I2C_ADDR);
    for (size_t i = 0; i < len; ++i) { Wire.write(bytes[i]); }
    Wire.endTransmission();
  }
public:
  void begin() override { Wire.begin(); }
  void setMasterVolumePercent(uint8_t v) override {
    volume = constrain(v, 0u, 100u);
    uint8_t buf[8]; size_t len = 0; pt2351_buildVolumeCommand(volume, buf, len); i2cWriteCmd(buf, len);
  }
  void setMute(bool m) override {
    isMuted = m;
    // Optionally implement a mute command via tone/att registers (left empty)
  }
  void setInput(uint8_t idx) override {
    inputIdx = idx; uint8_t buf[8]; size_t len = 0; pt2351_buildInputSelect(inputIdx, buf, len); i2cWriteCmd(buf, len);
  }
  void setTone(int8_t b, int8_t t) override {
    bass = b; treble = t; uint8_t buf[8]; size_t len = 0; pt2351_buildTone(bass, treble, buf, len); i2cWriteCmd(buf, len);
  }
  const char* name() const override { return "PT2351(I2C)"; }
};
#endif

#ifdef AUDIO_IC_R2S15902FP_3WIRE
class TwoWireSerial {
  uint8_t clkPin, dataPin;
public:
  TwoWireSerial(uint8_t clk, uint8_t data) : clkPin(clk), dataPin(data) {}
  void begin() {
    pinMode(clkPin, OUTPUT); pinMode(dataPin, OUTPUT);
    digitalWrite(clkPin, LOW); digitalWrite(dataPin, LOW);
  }
  // Send MSB-first 'bitCount' bits from 'value'. Data is read on rising edge.
  void sendFrame(uint32_t value, uint8_t bitCount) {
    if (bitCount == 0) return;
    // Shift bits MSB-first
    for (int8_t i = bitCount - 1; i >= 0; --i) {
      // Data valid before rising edge
      digitalWrite(clkPin, LOW);
      digitalWrite(dataPin, (value & (1UL << i)) ? HIGH : LOW);
      delayMicroseconds(2);
      digitalWrite(clkPin, HIGH);
      delayMicroseconds(2);
    }
    // Generate latch per datasheet:
    // "When DATA is H, latch created at falling edge of CLOCK. When CLOCK is L and latch created, latch read at falling edge of DATA."
    // Implement: set DATA=HIGH, pulse CLOCK high->low to create latch, then drive DATA low to commit.
    digitalWrite(dataPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(clkPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(clkPin, LOW);
    delayMicroseconds(2);
    digitalWrite(dataPin, LOW);
    delayMicroseconds(2);
  }
};

class AudioChipR2S15902 : public AudioChip {
  TwoWireSerial bus{R2S15902_CLK_PIN, R2S15902_DATA_PIN};
  bool isMuted = false; uint8_t volume = 50; uint8_t inputIdx = 0; int8_t bass = 0, treble = 0;
  void sendBits(uint32_t bits, uint8_t nbits) { if (nbits) bus.sendFrame(bits, nbits); }
public:
  void begin() override { bus.begin(); }
  void setMasterVolumePercent(uint8_t v) override {
    volume = constrain(v, 0u, 100u);
    uint32_t bits = 0; uint8_t len = 0; r2s15902_buildVolumeBits(volume, bits, len); sendBits(bits, len);
  }
  void setMute(bool m) override {
    isMuted = m; // If datasheet provides a mute frame, build and send here
  }
  void setInput(uint8_t idx) override {
    inputIdx = idx; uint32_t bits = 0; uint8_t len = 0; r2s15902_buildInputBits(inputIdx, bits, len); sendBits(bits, len);
  }
  void setTone(int8_t b, int8_t t) override {
    bass = b; treble = t; uint32_t bits = 0; uint8_t len = 0; r2s15902_buildToneBits(bass, treble, bits, len); sendBits(bits, len);
  }
  const char* name() const override { return "R2S15902FP(2-wire)"; }
};
#endif

// Global: choose driver
static AudioChip* gAudio = nullptr;
static AudioChipNull gNull;
#ifdef AUDIO_IC_PT2351_I2C
static AudioChipPT2351 gPT2351;
#endif
#ifdef AUDIO_IC_R2S15902FP_3WIRE
static AudioChipR2S15902 gR2S;
#endif

// State
static uint8_t gVolume = 40; // percent
static uint8_t gInput = 0;   // 0..3 example
static bool gMute = false;
static int8_t gBass = 0, gTreble = 0;

// Helpers
static void applyState() {
  if (!gAudio) return;
  gAudio->setMasterVolumePercent(gVolume);
  gAudio->setInput(gInput);
  gAudio->setTone(gBass, gTreble);
  gAudio->setMute(gMute);
}

static void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help           - show this help"));
  Serial.println(F("  scan           - I2C scan (0x03..0x77)"));
  Serial.println(F("  status         - print current state"));
  Serial.println(F("  vol +/- n      - set/change volume percent (0..100)"));
  Serial.println(F("  input n        - select input index (0..5)"));
  Serial.println(F("  mute on|off    - toggle mute"));
  Serial.println(F("  tone b t       - set bass/treble in dB (-14..+14, step 2)"));
  Serial.println(F("  irlearn        - print incoming IR codes"));
}

static void i2cScan() {
  Serial.println(F("Scanning I2C..."));
  uint8_t count = 0;
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print(F("  Found 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      ++count;
    }
    delay(5);
  }
  Serial.print(F("Devices: ")); Serial.println(count);
}

static void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n'); cmd.trim();
  if (cmd.length() == 0) return;
  if (cmd == F("help")) { printHelp(); return; }
  if (cmd == F("scan")) { i2cScan(); return; }
  if (cmd == F("status")) {
    Serial.print(F("Target: ")); Serial.println(gAudio ? gAudio->name() : "?" );
    Serial.print(F("Vol=")); Serial.print(gVolume);
    Serial.print(F("  Input=")); Serial.print(gInput);
    Serial.print(F("  Mute=")); Serial.print(gMute ? F("ON") : F("OFF"));
    Serial.print(F("  Tone(b,t)=(")); Serial.print(gBass); Serial.print(','); Serial.print(gTreble); Serial.println(')');
    return;
  }
  if (cmd.startsWith(F("vol "))) {
    String arg = cmd.substring(4); arg.trim();
    if (arg.startsWith("+")) { gVolume = constrain<uint8_t>(gVolume + arg.substring(1).toInt(), 0, 100); }
    else if (arg.startsWith("-")) { int v = (int)gVolume - arg.substring(1).toInt(); gVolume = constrain(v, 0, 100); }
    else { gVolume = constrain(arg.toInt(), 0, 100); }
    if (gAudio) gAudio->setMasterVolumePercent(gVolume);
    Serial.print(F("Vol->")); Serial.println(gVolume);
    return;
  }
  if (cmd.startsWith(F("input "))) {
    gInput = constrain(cmd.substring(6).toInt(), 0, 7);
    if (gAudio) gAudio->setInput(gInput);
    Serial.print(F("Input->")); Serial.println(gInput);
    return;
  }
  if (cmd.startsWith(F("mute "))) {
    String arg = cmd.substring(5); arg.trim();
    gMute = (arg.equalsIgnoreCase(F("on")) || arg == F("1"));
    if (gAudio) gAudio->setMute(gMute);
    Serial.print(F("Mute->")); Serial.println(gMute ? F("ON") : F("OFF"));
    return;
  }
  if (cmd.startsWith(F("tone "))) {
    int sp1 = cmd.indexOf(' '); int sp2 = cmd.indexOf(' ', sp1 + 1);
    if (sp2 > 0) {
      gBass = constrain(cmd.substring(sp1 + 1, sp2).toInt(), -14, 14);
      gTreble = constrain(cmd.substring(sp2 + 1).toInt(), -14, 14);
      if (gAudio) gAudio->setTone(gBass, gTreble);
      Serial.print(F("Tone->(")); Serial.print(gBass); Serial.print(','); Serial.print(gTreble); Serial.println(')');
    }
    return;
  }
  if (cmd == F("irlearn")) {
    Serial.println(F("IR learn mode. Press remote keys... (Ctrl+C to stop)"));
  }
}

static void handleIR() {
#ifdef IRREMOTE_V4
  if (IrReceiver.decode()) {
    auto& dec = IrReceiver.decodedIRData;
    uint32_t code = dec.decodedRawData;
    if (Serial) { Serial.print(F("IR: 0x")); Serial.println(code, HEX); }
    // Map actions
    if (code == IR_CODE_POWER) { gMute = !gMute; gAudio->setMute(gMute); }
    else if (code == IR_CODE_MUTE) { gMute = !gMute; gAudio->setMute(gMute); }
    else if (code == IR_CODE_VOL_UP) { gVolume = min<uint8_t>(100, gVolume + 2); gAudio->setMasterVolumePercent(gVolume); }
    else if (code == IR_CODE_VOL_DOWN) { gVolume = (gVolume >= 2) ? gVolume - 2 : 0; gAudio->setMasterVolumePercent(gVolume); }
    else if (code == IR_CODE_INPUT_NEXT) { gInput = (gInput + 1) % 6; gAudio->setInput(gInput); }
    else if (code == IR_CODE_INPUT_PREV) { gInput = (gInput + 5) % 6; gAudio->setInput(gInput); }
    IrReceiver.resume();
  }
#else
  decode_results results;
  if (irrecv.decode(&results)) {
    uint32_t code = results.value;
    if (Serial) { Serial.print(F("IR: 0x")); Serial.println(code, HEX); }
    if (code == IR_CODE_POWER) { gMute = !gMute; gAudio->setMute(gMute); }
    else if (code == IR_CODE_MUTE) { gMute = !gMute; gAudio->setMute(gMute); }
    else if (code == IR_CODE_VOL_UP) { gVolume = min<uint8_t>(100, gVolume + 2); gAudio->setMasterVolumePercent(gVolume); }
    else if (code == IR_CODE_VOL_DOWN) { gVolume = (gVolume >= 2) ? gVolume - 2 : 0; gAudio->setMasterVolumePercent(gVolume); }
    else if (code == IR_CODE_INPUT_NEXT) { gInput = (gInput + 1) % 6; gAudio->setInput(gInput); }
    else if (code == IR_CODE_INPUT_PREV) { gInput = (gInput + 5) % 6; gAudio->setInput(gInput); }
    irrecv.resume();
  }
#endif
}

void setup() {
  Serial.begin(115200);
  delay(50);

#if ENABLE_SERIAL_MENU
  if (Serial) {
    Serial.println(F("Arduino 5.1 Remote Kit Controller"));
  }
#endif

#ifdef IRREMOTE_V4
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
#else
  irrecv.enableIRIn();
#endif

  Wire.begin();

  // Select driver
#ifdef AUDIO_IC_PT2351_I2C
  gAudio = &gPT2351;
#elif defined(AUDIO_IC_R2S15902FP_3WIRE)
  gAudio = &gR2S;
#else
  gAudio = &gNull;
#endif

  gAudio->begin();
  applyState();

#if ENABLE_SERIAL_MENU
  printHelp();
  Serial.print(F("Target driver: ")); Serial.println(gAudio->name());
#endif
}

void loop() {
#if ENABLE_SERIAL_MENU
  handleSerial();
#endif
  handleIR();
}