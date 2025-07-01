/* ------------- pin allocation (NO overlaps) -------------- */
/* LCD : RS 7, EN 8, D4 9, D5 4, D6 3, D7 2  */
LiquidCrystal lcd(7, 8, 9, 4, 3, 2);

const uint8_t IR_PIN       = 11;           // IR receiver OUT
const uint8_t RELAY_POWER  = 12;           // main power relay (HIGH = ON)

/* 5 input relays – LOW selects the input */
const uint8_t RELAY_IN[5]  = {A0, A1, A2, A3, A4};

/* ------------------ Rotary encoder pins ----------------- */
const uint8_t ENC_CLK = 5;                // encoder A (CLK)
const uint8_t ENC_DT  = 6;                // encoder B (DT)
const uint8_t ENC_SW  = 10;               // encoder push button

/* ========================================================= */
void setup() {
  Wire.begin();             delay(250);        // per datasheet
  pinMode(RELAY_POWER, OUTPUT);
  for (uint8_t p:RELAY_IN) pinMode(p, OUTPUT);

  // Rotary encoder setup
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);

  digitalWrite(RELAY_POWER, HIGH);             // power on
  for (uint8_t i=0;i<5;i++) digitalWrite(RELAY_IN[i], i==selInput ? LOW:HIGH);

  lcd.begin(16,2);
  lcdRefresh();
  updateAllChannels();

  irrecv.enableIRIn();
}

/* ========================================================= */
void loop() {
  /* ---------- rotary encoder handling ------------------- */
  static uint8_t lastEncCLK = HIGH;
  static uint8_t lastEncSW  = HIGH;

  uint8_t encCLK = digitalRead(ENC_CLK);
  uint8_t encDT  = digitalRead(ENC_DT);
  uint8_t encSW  = digitalRead(ENC_SW);

  // Detect rotation on falling edge of CLK
  if (lastEncCLK == HIGH && encCLK == LOW) {
    if (encDT != encCLK) {                 // clockwise
      if (masterAtt > 0 && !muted && powerOn) {
        masterAtt--; updateAllChannels(); lcdRefresh();
      }
    } else {                               // counter-clockwise
      if (masterAtt < 79 && !muted && powerOn) {
        masterAtt++; updateAllChannels(); lcdRefresh();
      }
    }
  }
  lastEncCLK = encCLK;

  // Push-button -> mute toggle
  if (encSW == LOW && lastEncSW == HIGH) {   // pressed
    if (powerOn) {
      muted = !muted; axSetMute(muted); lcdRefresh();
    }
    delay(20);                               // debounce
  }
  lastEncSW = encSW;

  /* ---------- IR receiver -------------------------------- */
  if (irrecv.decode(&res)) {
    uint32_t code = res.value;
    irrecv.resume();

    // ... existing IR handling unchanged ...
  }

  lcdRefresh();
}