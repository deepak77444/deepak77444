// 6-Channel Digital Volume Controller (6CH DVC)
// by DaacWave <www.daacwave.com / www.daacwaves.com>

#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <IRremote.h>   //Version - IRremote-2.0.1
#include <PT2258.h>     //www.daacwaves.com

#define RotaryB 11
#define RotaryA 12
#define RotarySW 13
#define SW1 A1        // Power
#define SW2 A2        // Mute
#define SW3 A3        // Input/Mode
#define RECV_PIN 8    // IR input

#define powerOut 10   // Out

// NEW: CD4052 (stereo source select) and CD4053 (6CH vs ProLogic) control pins
// Assumptions:
// - CD4052 uses A (LSB) and B (MSB) select lines to choose one of 4 stereo sources for the Pro Logic decoder input
// - Two CD4053 ICs switch 6 channels between DVD 5.1 direct and Pro Logic decoded outputs. All six sections share a single select line.
// - Pins 0/1 are shared with Serial on Arduino UNO. This sketch does not use Serial. If uploads fail, temporarily disconnect 0/1 from external circuitry during flashing,
//   or remap to other available pins in your hardware.
#define SEL4052_A A0  // 4052 select A (LSB)
#define SEL4052_B 0   // 4052 select B (MSB) [uses D0]
#define SEL4053_SEL 9 // 4053 select. 0 = DVD 6CH direct, 1 = Pro Logic

// IR HEX code
#define ir_power 0x807F827D     // IR Power ON/OFF
#define ir_mute 0x807F42BD      // IR Mute
#define ir_vol_up 0x807F906F    // IR Vol++
#define ir_vol_down 0x807FA05F  // IR Vol--
#define ir_ch_up 0x807F40BF     // IR CH++ (menu page up)
#define ir_ch_down 0x807FC03F   // IR CH-- (menu page down)

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);  // RS,E,D4,D5,D6,D7

byte custom_num[8][8] = {
  { B00111, B01111, B11111, B11111, B11111, B11111, B11111, B11111 },
  { B11111, B11111, B11111, B00000, B00000, B00000, B00000, B00000 },
  { B11100, B11110, B11111, B11111, B11111, B11111, B11111, B11111 },
  { B11111, B11111, B11111, B11111, B11111, B11111, B01111, B00111 },
  { B00000, B00000, B00000, B00000, B00000, B11111, B11111, B11111 },
  { B11111, B11111, B11111, B11111, B11111, B11111, B11110, B11100 },
  { B11111, B11111, B11111, B00000, B00000, B00000, B11111, B11111 },
  { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 }
};

const int digit_width = 3;
const char custom_num_top[10][digit_width] = { 0, 1, 2, 1, 2, 32, 6, 6, 2, 6, 6, 2, 3, 4, 7, 7, 6, 6, 0, 6, 6, 1, 1, 2, 0, 6, 2, 0, 6, 2 };
const char custom_num_bot[10][digit_width] = { 3, 4, 5, 4, 7, 4, 7, 4, 4, 4, 4, 5, 32, 32, 7, 4, 4, 5, 3, 4, 5, 32, 32, 7, 3, 4, 5, 4, 4, 5 };

unsigned long returnTime;
unsigned long lastRotaryTime = 0;
unsigned long lastButtonTime = 0;
unsigned long muteLcdTime = 0;
int rotaryDelayTime = 200;
int buttonDelayTime = 100;
int muteLcdDelay = 0;
int muteLcdOnTime = 500;
int muteLcdOffTime = 500;

int masVolLimitMax = 69;
int masVolLimitMin = 19;
int chVolLimitMax = 10;
int chVolLimitMin = -10;
int lastEncoded = 0;

int vol_menu, mas_vol, ch1_vol, ch2_vol, ch3_vol, ch4_vol, ch5_vol, ch6_vol, mute, ch1, ch2, ch3, ch4, ch5, ch6;
int return_d, power, menu_active;

// NEW: source/mode state
int st_src = 0;          // 0..3 for CD4052 stereo source select
int surround_mode = 0;   // 0 = DVD 6CH direct (CD4053=0), 1 = Pro Logic (CD4053=1)

// Optional names for display
const char* stereoSourceNames[4] = { "ST0", "ST1", "ST2", "ST3" }; // Map to your actual inputs (e.g., "DVD", "AUX1", ...)

IRrecv irrecv(RECV_PIN);
decode_results results;

// NEW: helpers to control the analog switch ICs
void applyStereoSelect() {
  digitalWrite(SEL4052_A, (st_src & 0x01) ? HIGH : LOW);
  digitalWrite(SEL4052_B, (st_src & 0x02) ? HIGH : LOW);
}

void applySurroundMode() {
  // 0 = DVD 6CH direct, 1 = Pro Logic decoded
  digitalWrite(SEL4053_SEL, surround_mode ? HIGH : LOW);
}

void setup() {
  Wire.begin();
  lcd.begin(16, 2);
  irrecv.enableIRIn();

  pinMode(RotaryA, INPUT);
  pinMode(RotaryB, INPUT);
  pinMode(RotarySW, INPUT_PULLUP);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(SW3, INPUT);
  pinMode(powerOut, OUTPUT);

  // NEW: set up control pins
  pinMode(SEL4052_A, OUTPUT);
  pinMode(SEL4052_B, OUTPUT);
  pinMode(SEL4053_SEL, OUTPUT);

  digitalWrite(RotaryA, HIGH);
  digitalWrite(RotaryB, HIGH);

  power = 0;
  eepromRead();

  // Apply persisted hardware select state early
  applyStereoSelect();
  applySurroundMode();

  startUp();
  powerUp();
}

void loop() {
  lcdDisplay();
  eepromUpdate();
  IRControl();
  returnDelay();


  //Power -------------------------------------------------//
  if (analogRead(SW1) > 900) {
    if (millis() - lastButtonTime > buttonDelayTime) {
      power++;
      if (power > 1) power = 0;
      powerUp();
    }
    lastButtonTime = millis();
  }

  if (power == 1) {
    updateEncoder();
    //Mute -------------------------------------------------//
    if (analogRead(SW2) > 900) {
      if (millis() - lastButtonTime > buttonDelayTime) {
        soundMute();
      }
      lastButtonTime = millis();
    }

    // NEW: SW3 toggles surround mode (DVD 6CH <-> Pro Logic)
    if (analogRead(SW3) > 900) {
      if (millis() - lastButtonTime > buttonDelayTime) {
        surround_mode ^= 1;
        applySurroundMode();
        btnPress();
      }
      lastButtonTime = millis();
    }

    if (digitalRead(RotarySW) == LOW) {
      if (millis() - lastButtonTime > buttonDelayTime) {
        volumeMenuUp();
      }
      lastButtonTime = millis();
    }
  }
}

void updateEncoder() {
  int CLK = digitalRead(RotaryA);
  int DT = digitalRead(RotaryB);

  int encoded = (CLK << 1) | DT;
  int addData = (lastEncoded << 2) | encoded;

  if (addData == 0b1101 || addData == 0b0100 || addData == 0b0010 || addData == 0b1011) {
    if ((millis() - lastRotaryTime) > rotaryDelayTime) {
      volumeUp();
      lastRotaryTime = millis();
    }
  }
  if (addData == 0b1110 || addData == 0b0111 || addData == 0b0001 || addData == 0b1000) {
    if ((millis() - lastRotaryTime) > rotaryDelayTime) {
      volumeDown();
      lastRotaryTime = millis();
    }
  }
  lastEncoded = encoded;
}

void volumeMenuUp() {
  vol_menu++;
  if (vol_menu > 8) vol_menu = 0; // NEW: pages 7 and 8 added
  btnPress();
}

void volumeMenuDown() {
  vol_menu--;
  if (vol_menu < 0) vol_menu = 8; // NEW: pages 7 and 8 added
  btnPress();
}

void volumeUp() {
  if (vol_menu == 0) {
    mas_vol++;
    masVol();
  }
  if (vol_menu == 1) {
    ch1_vol++;
    ch1Vol(1);
  }
  if (vol_menu == 2) {
    ch2_vol++;
    ch2Vol(1);
  }
  if (vol_menu == 3) {
    ch3_vol++;
    ch3Vol(1);
  }
  if (vol_menu == 4) {
    ch4_vol++;
    ch4Vol(1);
  }
  if (vol_menu == 5) {
    ch5_vol++;
    ch5Vol(1);
  }
  if (vol_menu == 6) {
    ch6_vol++;
    ch6Vol(1);
  }
  // NEW: stereo source select
  if (vol_menu == 7) {
    st_src++;
    if (st_src > 3) st_src = 0;
    applyStereoSelect();
  }
  // NEW: mode select
  if (vol_menu == 8) {
    surround_mode ^= 1;
    applySurroundMode();
  }
  btnPress();
}

void volumeDown() {
  if (vol_menu == 0) {
    mas_vol--;
    masVol();
  }
  if (vol_menu == 1) {
    ch1_vol--;
    ch1Vol(1);
  }
  if (vol_menu == 2) {
    ch2_vol--;
    ch2Vol(1);
  }
  if (vol_menu == 3) {
    ch3_vol--;
    ch3Vol(1);
  }
  if (vol_menu == 4) {
    ch4_vol--;
    ch4Vol(1);
  }
  if (vol_menu == 5) {
    ch5_vol--;
    ch5Vol(1);
  }
  if (vol_menu == 6) {
    ch6_vol--;
    ch6Vol(1);
  }
  // NEW: stereo source select
  if (vol_menu == 7) {
    st_src--;
    if (st_src < 0) st_src = 3;
    applyStereoSelect();
  }
  // NEW: mode select
  if (vol_menu == 8) {
    surround_mode ^= 1;
    applySurroundMode();
  }
  btnPress();
}

void soundMute() {
  mute++;
  if (mute > 1) mute = 0;
  set_mute(mute);
}

void masVol() {
  if (mas_vol > masVolLimitMax) mas_vol = masVolLimitMax;
  if (mas_vol < masVolLimitMin) mas_vol = masVolLimitMin;

  if (mute == 0) {
    if (mas_vol == masVolLimitMin) set_mute(1);
    else set_mute(0);
  }

  for (int i = 1; i < 5; i++) {
    ch1Vol(i);
    ch2Vol(i);
    ch3Vol(i);
    ch4Vol(i);
    ch5Vol(i);
    ch6Vol(i);
  }
}

void ch1Vol(int adr) {
  if (ch1_vol > chVolLimitMax) ch1_vol = chVolLimitMax;
  if (ch1_vol < chVolLimitMin) ch1_vol = chVolLimitMin;
  ch1 = mas_vol + ch1_vol;
  set_address(adr);
  set_ch1(ch1);
}

void ch2Vol(int adr) {
  if (ch2_vol > chVolLimitMax) ch2_vol = chVolLimitMax;
  if (ch2_vol < chVolLimitMin) ch2_vol = chVolLimitMin;
  ch2 = mas_vol + ch2_vol;
  set_address(adr);
  set_ch2(ch2);
}

void ch3Vol(int adr) {
  if (ch3_vol > chVolLimitMax) ch3_vol = chVolLimitMax;
  if (ch3_vol < chVolLimitMin) ch3_vol = chVolLimitMin;
  ch3 = mas_vol + ch3_vol;
  set_address(adr);
  set_ch3(ch3);
}

void ch4Vol(int adr) {
  if (ch4_vol > chVolLimitMax) ch4_vol = chVolLimitMax;
  if (ch4_vol < chVolLimitMin) ch4_vol = chVolLimitMin;
  ch4 = mas_vol + ch4_vol;
  set_address(adr);
  set_ch4(ch4);
}

void ch5Vol(int adr) {
  if (ch5_vol > chVolLimitMax) ch5_vol = chVolLimitMax;
  if (ch5_vol < chVolLimitMin) ch5_vol = chVolLimitMin;
  ch5 = mas_vol + ch5_vol;
  set_address(adr);
  set_ch5(ch5);
}

void ch6Vol(int adr) {
  if (ch6_vol > chVolLimitMax) ch6_vol = chVolLimitMax;
  if (ch6_vol < chVolLimitMin) ch6_vol = chVolLimitMin;
  ch6 = mas_vol + ch6_vol;
  set_address(adr);
  set_ch6(ch6);
}

void powerUp() {
  if (power == 1) {
    lcd.clear();
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("   LOADING...   ");
    delay(1000);
    lcd.clear();
    if (mas_vol == masVolLimitMin) {
      set_mute(1);
    } else {
      mute = 0;
      set_mute(mute);
    }
    vol_menu = 0;
    menu_active = 0;
    digitalWrite(powerOut, HIGH);
    delay(300);

  } else {

    set_mute(1);
    delay(100);
    digitalWrite(powerOut, LOW);
    menu_active = 100;
  }
}

void startUp() {
  delay(500);
  pt2258();
  set_mute(1);
  lcd.setCursor(0, 0);
  lcd.print("    DaacWave    ");
  delay(300);
  lcd.setCursor(0, 1);
  lcd.print("     AUDIOS     ");
  delay(1500);
  lcd.clear();
  delay(300);
  lcd.setCursor(0, 0);
  lcd.print("   6CH-DVC      ");
  lcd.setCursor(0, 1);
  lcd.print("   LOADING...   ");
  delay(1200);
  lcd.clear();
  delay(200);
  for (int i = 1; i < 5; i++) {
    ch1Vol(i);
    ch2Vol(i);
    ch3Vol(i);
    ch4Vol(i);
    ch5Vol(i);
    ch6Vol(i);
  }
}

//IR control --------------------------------------------------------------------------------//
void IRControl() {
  if (irrecv.decode(&results)) {
    switch (results.value) {
      //power -------------------------------------------------//
      case ir_power:
        power++;
        if (power > 1) power = 0;
        powerUp();
        break;
    }
    if (power == 1) {
      switch (results.value) {
        //mute -------------------------------------------------//
        case ir_mute:
          soundMute();
          break;

        //VOL --------------------------------------------------//
        case ir_vol_up:
          if (mute == 0) volumeUp();
          break;

        case ir_vol_down:
          if (mute == 0) volumeDown();
          break;

        //MENU NAV ---------------------------------------------//
        case ir_ch_up:
          volumeMenuUp();
          break;

        case ir_ch_down:
          volumeMenuDown();
          break;
      }
    }
    irrecv.resume();
  }
}

void lcdDisplay() {
  int a, b, c;
  int showBigDigits = 1; // NEW: control whether to show big digits zone
  switch (menu_active) {
    case 0:
      //vol ----------------------------------------------//
      switch (vol_menu) {
        case 0:
          lcd.setCursor(0, 1);
          lcd.print("MAS-VOL");
          lcd.setCursor(0, 0);
          c = mas_vol - masVolLimitMin;
          break;

        case 1:
          lcd.setCursor(0, 1);
          lcd.print("CH1-VOL");
          c = ch1_vol;
          break;

        case 2:
          lcd.setCursor(0, 1);
          lcd.print("CH2-VOL");
          c = ch2_vol;
          break;

        case 3:
          lcd.setCursor(0, 1);
          lcd.print("CH3-VOL");
          c = ch3_vol;
          break;

        case 4:
          lcd.setCursor(0, 1);
          lcd.print("CH4-VOL");
          c = ch4_vol;
          break;

        case 5:
          lcd.setCursor(0, 1);
          lcd.print("CH5-VOL");
          c = ch5_vol;
          break;

        case 6:
          lcd.setCursor(0, 1);
          lcd.print("CH6-VOL");
          c = ch6_vol;
          break;

        // NEW: Stereo source select page
        case 7:
          lcd.setCursor(0, 1);
          lcd.print("ST-SRC  ");
          lcd.print(stereoSourceNames[st_src]);
          showBigDigits = 0;
          break;

        // NEW: Surround mode select page
        case 8:
          lcd.setCursor(0, 1);
          lcd.print("MODE:   ");
          if (surround_mode == 0) lcd.print("DVD 6CH");
          else lcd.print("PROLG  ");
          showBigDigits = 0;
          break;
      }
      break;

    case 100:
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      if (muteLcdDelay == 0) {
        if ((millis() - muteLcdTime) >= muteLcdOnTime) {
          lcd.print("                ");
          muteLcdDelay = 1;
          muteLcdTime = millis();
        }
      } else {
        if ((millis() - muteLcdTime) >= muteLcdOffTime) {
          lcd.print("    STANDBY     ");
          muteLcdDelay = 0;
          muteLcdTime = millis();
        }
      }
      break;
  }
  if (menu_active == 0) {
    if (mute == 1) {
      lcd.setCursor(0, 0);
      if (muteLcdDelay == 0) {
        if ((millis() - muteLcdTime) >= muteLcdOnTime) {
          lcd.print("       ");
          muteLcdDelay = 1;
          muteLcdTime = millis();
        }
      } else {
        if ((millis() - muteLcdTime) >= muteLcdOffTime) {
          lcd.print("MUTE   ");
          muteLcdDelay = 0;
          muteLcdTime = millis();
        }
      }

    } else {
      lcd.setCursor(0, 0);
      // NEW: show short status: mode + source
      if (surround_mode == 0) {
        lcd.print("6CH-DVC");
      } else {
        lcd.print("PRLG ");
        lcd.print(stereoSourceNames[st_src][0]); // print first letter to keep width 7
        lcd.print(" ");
      }
    }

    if (showBigDigits) {
      for (int i = 0; i < 8; i++)
        lcd.createChar(i, custom_num[i]);

      int y;
      if (c < 0) {
        lcd.setCursor(8, 1);
        lcd.print("-");
        y = chVolLimitMax - (c + chVolLimitMax);
      } else if (c == chVolLimitMin) {
        lcd.setCursor(8, 1);
        lcd.print("-");
        y = chVolLimitMax;
      } else {
        lcd.setCursor(8, 1);
        lcd.print(" ");
        y = c;
      }
      a = y / 10;
      b = y - a * 10;

      lcd.setCursor(9, 0);
      for (int i = 0; i < digit_width; i++)
        lcd.print(custom_num_top[a][i]);

      lcd.setCursor(9, 1);
      for (int i = 0; i < digit_width; i++)
        lcd.print(custom_num_bot[a][i]);

      lcd.setCursor(13, 0);
      for (int i = 0; i < digit_width; i++)
        lcd.print(custom_num_top[b][i]);

      lcd.setCursor(13, 1);
      for (int i = 0; i < digit_width; i++)
        lcd.print(custom_num_bot[b][i]);
    } else {
      // Clear big digit area when not used
      lcd.setCursor(8, 0); lcd.print("        ");
      lcd.setCursor(8, 1); lcd.print("        ");
    }
  }
}

//EEPROM -----------------------------------------------------//
void eepromUpdate() {
  EEPROM.update(0, mas_vol);
  EEPROM.update(1, ch1_vol + 10);
  EEPROM.update(2, ch2_vol + 10);
  EEPROM.update(3, ch3_vol + 10);
  EEPROM.update(4, ch4_vol + 10);
  EEPROM.update(5, ch5_vol + 10);
  EEPROM.update(6, ch6_vol + 10);
  // NEW: store source/mode
  EEPROM.update(7, st_src);
  EEPROM.update(8, surround_mode);
}

void eepromRead() {
  mas_vol = EEPROM.read(0);
  ch1_vol = EEPROM.read(1) - 10;
  ch2_vol = EEPROM.read(2) - 10;
  ch3_vol = EEPROM.read(3) - 10;
  ch4_vol = EEPROM.read(4) - 10;
  ch5_vol = EEPROM.read(5) - 10;
  ch6_vol = EEPROM.read(6) - 10;
  // NEW: read with bounds checks
  st_src = EEPROM.read(7);
  if (st_src < 0 || st_src > 3) st_src = 0;
  surround_mode = EEPROM.read(8);
  if (surround_mode != 0 && surround_mode != 1) surround_mode = 0;
}

void btnPress() {
  returnTime = millis();
  return_d = 1;
}
void returnDelay() {
  if (millis() - returnTime > 5000 && return_d == 1 && vol_menu != 0) {
    vol_menu = 0;
    return_d = 0;
    lcd.clear();
  }
}