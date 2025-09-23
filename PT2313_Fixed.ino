#include <Wire.h>
#include <PT2313.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(7, 6, 2, 3, 4, 5); // RS,E,D4,D5,D6,D7
PT2313 audioChip;

// Bar Characters
byte a1[8] = {0b00000,0b11011,0b11011,0b11011,0b11011,0b11011,0b11011,0b00000};
byte a2[8] = {0b00000,0b11000,0b11000,0b11000,0b11000,0b11000,0b11000,0b00000};

// Mute Characters
byte mute1[] = { B00000,B00000,B00000,B00000,B00000,B00111,B00111,B00111 };
byte mute2[] = { B00111,B00111,B00111,B00000,B00000,B00000,B00000,B00000 };
byte mute3[] = { B00000,B00011,B00011,B01111,B11111,B11111,B11111,B11111 };
byte mute4[] = { B11111,B11111,B11111,B11111,B01111,B00011,B00011,B00000 };
byte mutex1[] = { B00000,B00000,B00000,B00000,B01000,B00100,B00010,B00001 };
byte mutex2[] = { B10000,B01000,B00100,B00010,B00000,B00000,B00000,B00000 };
byte mutex3[] = { B00000,B00000,B00000,B00000,B00010,B00100,B01000,B10000 };
byte mutex4[] = { B00001,B00010,B00100,B01000,B00000,B00000,B00000,B00000 };

// Pins (rotary encoder and buttons)
enum PinAssignments {
  dtPinA = 8,      // DT
  clkPinB = 9,     // CLK
  menuSwitch = 10, // SW (active LOW with INPUT_PULLUP)
  muteButton = 12, // Tactile Switch1 (HIGH active in original circuit)
  inButton   = 11  // Tactile Switch2 (HIGH active in original circuit)
};

// Rotary encoder state (polling)
int lastClkState = HIGH;

// Flags / states
static boolean rotating = false; // kept for compatibility; not used for ISRs anymore
boolean A_set = false;           // legacy
boolean B_set = false;           // legacy

boolean loud;

byte menu, in, w, save_eeprom;
int a, b, c, z, hit, menu_active, mute, vol, bassz, treb, balanz, gainz;

unsigned long time;

const int SHORT_PRESS_TIME = 500;
const int LONG_PRESS_TIME  = 500;

// Variables for long-press detection on muteButton (active HIGH per original comment)
int lastState = HIGH;
int currentState;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

void eepromRead() {
  vol    = EEPROM.read(0);
  bassz  = EEPROM.read(1) - 7;
  treb   = EEPROM.read(2) - 7;
  balanz = EEPROM.read(3) - 4;
  in     = EEPROM.read(4);
  loud   = EEPROM.read(5);
  gainz  = EEPROM.read(6);

  // Clamp to valid ranges
  if (vol   < 0)  vol = 0;       if (vol   > 62) vol = 62;
  if (bassz < -7) bassz = -7;    if (bassz > 7)  bassz = 7;
  if (treb  < -7) treb = -7;     if (treb  > 7)  treb = 7;
  if (balanz < -4) balanz = -4;  if (balanz > 4) balanz = 4;
  if (in < 0) in = 0;            if (in > 3) in = 0; // 0..3 valid on PT2313
  if (gainz < 0) gainz = 0;      if (gainz > 3) gainz = 3;
}

void eepromUpdate() {
  EEPROM.update(0, vol);
  EEPROM.update(1, bassz + 7);
  EEPROM.update(2, treb + 7);
  EEPROM.update(3, balanz + 4);
  EEPROM.update(4, in);
  EEPROM.update(5, loud);
  EEPROM.update(6, gainz);
}

void cl() {
  delay(50);
  lcd.clear();
}

void audio() {
  audioChip.source(in);         // 0..3
  audioChip.volume(vol);        // 0..62 (63=muted)
  audioChip.gain(gainz);        // 0..3
  audioChip.bass(bassz);        // -7..+7
  audioChip.treble(treb);       // -7..+7
  audioChip.balance(balanz);    // -31..+31 (we use -4..+4 steps of 2 dB in UI)
  audioChip.loudness(loud);     // true/false
}

void start_up() {
  vol = 0;
  audio();
  lcd.setCursor(0, 1);
  lcd.print("  ABHI TECH  ");
  delay(1500);
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("LOADING   ");
  for (int x = 6; x < 15; x++) {
    lcd.setCursor(x + 1, 1);
    lcd.print(".");
    delay(300);
  }
  delay(500);
  lcd.clear();
  vol = EEPROM.read(0);
  if (vol < 0) vol = 0; if (vol > 62) vol = 62;
  menu = 0;
}

// Simplified LCD update: normal-sized numeric volume instead of broken big-number table
void lcd_update() {
  if (menu_active == 0) {
    lcd.setCursor(0, 0);
    lcd.print("IN-");
    lcd.print(in + 1);

    lcd.setCursor(0, 1);
    lcd.print("MAS-VOL ");

    // Show volume in two digits (00..62)
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", vol);
    lcd.setCursor(9, 1);
    lcd.print(buf);
    lcd.print("  ");
  }
}

// Rotary encoder polling; adjust values based on menu
void updateEncoder() {
  int clkState = digitalRead(clkPinB);
  if (clkState != lastClkState) {
    if (clkState == HIGH) {
      // Determine direction from DT relative to CLK
      int dtState = digitalRead(dtPinA);
      bool clockwise = (dtState != clkState);

      if (clockwise) {
        if (menu == 0)      { if (vol   < 62) { vol++;   w = 1; } }
        else if (menu == 1) { if (bassz < 7)  { bassz++; w = 1; } }
        else if (menu == 2) { if (treb  < 7)  { treb++;  w = 1; } }
        else if (menu == 3) { if (balanz< 4)  { balanz++;w = 1; } }
        else if (menu == 4) { hit++; if (hit > 1) hit = 0; w = 1; }
        else if (menu == 5) { if (gainz< 3)   { gainz++; w = 1; } }
      } else {
        if (menu == 0)      { if (vol   > 0)  { vol--;   w = 1; } }
        else if (menu == 1) { if (bassz > -7) { bassz--; w = 1; } }
        else if (menu == 2) { if (treb  > -7) { treb--;  w = 1; } }
        else if (menu == 3) { if (balanz> -4) { balanz--;w = 1; } }
        else if (menu == 4) { hit--; if (hit < 0) hit = 1; w = 1; }
        else if (menu == 5) { if (gainz> 0)   { gainz--; w = 1; } }
      }
    }
    lastClkState = clkState;
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(16, 2);

  audioChip.initialize(0, true);

  // Buttons (keep original HIGH-active behavior as noted in comments)
  pinMode(muteButton, INPUT); // expects external pulldown, active HIGH
  pinMode(inButton, INPUT);   // expects external pulldown, active HIGH

  // Rotary encoder and menu switch use internal pullups
  pinMode(dtPinA, INPUT_PULLUP);
  pinMode(clkPinB, INPUT_PULLUP);
  pinMode(menuSwitch, INPUT_PULLUP); // active LOW

  // LED for input indication
  pinMode(13, OUTPUT);

  lastClkState = digitalRead(clkPinB);

  eepromRead();
  start_up();
}

void loop() {
  // Poll rotary encoder
  updateEncoder();

  // Input selector (active HIGH per original wiring)
  if (digitalRead(inButton) == HIGH) {
    in++;
    time = millis();
    save_eeprom = 1;
    audio();
    if (in == 1) {
      digitalWrite(13, HIGH);
    } else {
      digitalWrite(13, LOW);
    }
    delay(200);
    if (in > 2) {
      in = 0;
    }
  }

  // Rotary menu rendering and actions
  rotaryE();

  // Mute button long-press (active HIGH per original wiring)
  currentState = digitalRead(muteButton);
  if (lastState == LOW && currentState == HIGH) {
    pressedTime = millis();
  } else if (lastState == HIGH && currentState == LOW) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if (pressDuration > LONG_PRESS_TIME) {
      mute++;
      if (mute > 1) mute = 0;
      delay(200);
    }

    if (mute == 0) {
      menu_active = 1;
      vol = 0;
      lcd.clear();
      delay(300);
      lcd.setCursor(0, 1);
      lcd.print("MUTE ");

      // Draw mute icon using custom chars
      lcd.createChar(0, mute1);
      lcd.createChar(1, mute2);
      lcd.createChar(2, mute3);
      lcd.createChar(3, mute4);
      lcd.createChar(4, mutex1);
      lcd.createChar(5, mutex2);
      lcd.createChar(6, mutex3);
      lcd.createChar(7, mutex4);

      lcd.setCursor(12, 0); lcd.write((uint8_t)0);
      lcd.setCursor(12, 1); lcd.write((uint8_t)1);
      lcd.setCursor(13, 0); lcd.write((uint8_t)2);
      lcd.setCursor(13, 1); lcd.write((uint8_t)3);
      lcd.setCursor(14, 0); lcd.write((uint8_t)4);
      lcd.setCursor(15, 1); lcd.write((uint8_t)5);
      lcd.setCursor(15, 0); lcd.write((uint8_t)6);
      lcd.setCursor(14, 1); lcd.write((uint8_t)7);

      menu = 99;
    }
    if (mute == 1 && menu_active == 1) {
      vol = EEPROM.read(0);
      lcd.clear();
      menu_active = 0;
      menu = 0;
    }
    audio();
  }
  lastState = currentState;

  // Auto-save to EEPROM after 10 seconds of inactivity
  if (millis() - time > 10000 && save_eeprom == 1) {
    eepromUpdate();
    menu = 0;
    save_eeprom = 0;
    cl();
  }
}

// Rotary menu and UI drawing
void rotaryE() {
  if (menu == 0) {
    if (w == 1) { audio(); time = millis(); save_eeprom = 1; w = 0; }
    c = vol;
    lcd_update();
  }

  if (menu == 1) {
    if (w == 1) { audio(); cl(); time = millis(); save_eeprom = 1; w = 0; }
    lcd.setCursor(0, 0); lcd.print("Bass");
    lcd.setCursor(12, 0); lcd.print(bassz);
    lcd.setCursor(14, 0); lcd.print("dB");
    lcd.createChar(0, a1); lcd.createChar(1, a2);
    if (bassz < 0) {
      for (int x = -1; x >= bassz; x--) { lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">"); lcd.setCursor(x + 7, 1); lcd.write((uint8_t)0); }
    } else if (bassz > 0) {
      for (int x = 1; x <= bassz; x++) { lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">"); lcd.setCursor(x + 8, 1); lcd.write((uint8_t)0); }
    } else {
      lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">");
    }
  }

  if (menu == 2) {
    if (w == 1) { audio(); cl(); time = millis(); save_eeprom = 1; w = 0; }
    lcd.setCursor(0, 0); lcd.print("Trebble");
    lcd.setCursor(12, 0); lcd.print(treb);
    lcd.setCursor(14, 0); lcd.print("dB");
    lcd.createChar(0, a1); lcd.createChar(1, a2);
    if (treb < 0) {
      for (int x = -1; x >= treb; x--) { lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">"); lcd.setCursor(x + 7, 1); lcd.write((uint8_t)0); }
    } else if (treb > 0) {
      for (int x = 1; x <= treb; x++) { lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">"); lcd.setCursor(x + 8, 1); lcd.write((uint8_t)0); }
    } else {
      lcd.setCursor(7, 1); lcd.print("<"); lcd.setCursor(8, 1); lcd.print(">");
    }
  }

  if (menu == 3) {
    lcd.setCursor(0, 0);
    if (balanz >= 0) { lcd.print("-"); } else { lcd.print("+"); }
    lcd.print(abs(balanz * 2)); lcd.print(" dB "); lcd.print(" <>  ");
    if (balanz >= 0) { lcd.print("+"); } else { lcd.print("-"); }
    lcd.print(abs(balanz * 2)); lcd.print(" dB ");
    lcd.setCursor(0, 1); lcd.print("R");
    lcd.setCursor(15, 1); lcd.print("L");
    lcd.createChar(0, a1);
    if (balanz < 0) { lcd.setCursor(balanz + 7, 1); lcd.write((uint8_t)0); }
    if (balanz > 0) { lcd.setCursor(balanz + 8, 1); lcd.write((uint8_t)0); }
    if (balanz == 0) { lcd.setCursor(7, 1); lcd.write((uint8_t)0); lcd.setCursor(8, 1); lcd.write((uint8_t)0); }
    if (w == 1) { audio(); cl(); time = millis(); save_eeprom = 1; w = 0; }
  }

  if (menu == 4) {
    if (hit == 1) { loud = true; } else { loud = false; }
    delay(100);
    lcd.setCursor(2, 0); lcd.print("- LOUDNESS -");
    lcd.setCursor(5, 1);
    if (!loud) { lcd.print(">>OFF"); } else { lcd.print(">>ON "); }
    if (w == 1) { audio(); time = millis(); save_eeprom = 1; w = 0; }
  }

  if (menu == 5) {
    if (w == 1) { audio(); cl(); time = millis(); save_eeprom = 1; w = 0; }
    lcd.setCursor(0, 0); lcd.print("Gain");
    lcd.setCursor(12, 0); lcd.print(gainz);
    lcd.setCursor(14, 0); lcd.print("dB");
    lcd.createChar(0, a1); lcd.createChar(1, a2);
    for (z = 0; z <= gainz; z++) {
      lcd.setCursor(z * 3 + 2, 1);
      lcd.write((uint8_t)0);
      lcd.write((uint8_t)0);
      lcd.write((uint8_t)0);
    }
  }

  // Menu button (active LOW)
  if (digitalRead(menuSwitch) == LOW) {
    menu++;
    time = millis();
    save_eeprom = 1;
    w = 1;
    cl();
    if (menu > 5) { menu = 0; }
    delay(200);
  }
}

