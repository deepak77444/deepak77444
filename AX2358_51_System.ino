// AX2358 5.1 Surround System
// Updated for latest IRremote, fast rotary encoder, and smooth IR hold handling

#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <IRremote.hpp>

#define AX2358_address 0b1001010

#define sw01 9      // Encoder SW (push)
#define sw02 11     // Encoder DT
#define sw03 10     // Encoder CLK

#define sw_power 13 // Power control output to amp
#define IR_RECEIVE_PIN 8

// NEC command byte values extracted from original 32-bit codes
#define NEC_CMD_POWER     0x82
#define NEC_CMD_MUTE      0x42
#define NEC_CMD_IN0       0x62
#define NEC_CMD_IN1       0x52
#define NEC_CMD_IN2       0xA2
#define NEC_CMD_IN3       0x22
#define NEC_CMD_IN4       0x20
#define NEC_CMD_VOL_UP    0x90
#define NEC_CMD_VOL_DN    0xA0
#define NEC_CMD_FL_UP     0x40
#define NEC_CMD_FL_DN     0xC0
#define NEC_CMD_FR_UP     0x00
#define NEC_CMD_FR_DN     0x80
#define NEC_CMD_SL_UP     0x48
#define NEC_CMD_SL_DN     0xC8
#define NEC_CMD_SR_UP     0x08
#define NEC_CMD_SR_DN     0x88
#define NEC_CMD_CN_UP     0x50
#define NEC_CMD_CN_DN     0x60
#define NEC_CMD_SUB_UP    0xD0
#define NEC_CMD_SUB_DN    0xE0
#define NEC_CMD_SP_MODE   0x0A
#define NEC_CMD_SURR_TG   0xA8
#define NEC_CMD_RESET     0x1A

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// Custom LCD characters
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
const char custom_num_top[10][digit_width] = { 0, 1, 2, 1, 2, 32, 6, 6, 2, 6, 6, 2, 3, 4, 7,   7, 6, 6, 0, 6, 6, 1, 1, 2,   0, 6, 2, 0, 6, 2};
const char custom_num_bot[10][digit_width] = { 3, 4, 5, 4, 7, 4,  7, 4, 4, 4, 4, 5, 32, 32, 7, 4, 4, 5, 3, 4, 5, 32, 32, 7, 3, 4, 5, 4, 4, 5};

byte arrow_right[8] = {B00000, B10000, B11000, B11100, B11110, B11100, B11000, B10000};

// State
unsigned long time;
int in, mute, return_d, surr, mix, a, b, x, power, menu, menu_active, ch_mute, speaker_mode, btn_press, long_press, vol_menu, vol_menu_jup, reset;
int fl, fr, sl, sr, cn, sub, ir_menu, ir_on, mas_vol, fl_vol, fr_vol, sl_vol, sr_vol, cn_vol, sub_vol;

// Debounce / long-press
long btn_timer = 0;
long long_press_time = 600;

// Non-blocking button clear
void btn_cl() {
  time = millis();
  return_d = 1;
}
void ir_cl() {
  time = millis();
  return_d = 1;
}

// Fast encoder helpers
int encoderDelta() {
  static int lastClk = HIGH;
  int clk = digitalRead(sw03);
  int delta = 0;
  if (clk != lastClk && clk == LOW) {
    int dt = digitalRead(sw02);
    delta = (dt == HIGH) ? +1 : -1;
  }
  lastClk = clk;
  return delta;
}
int encoderDeltaWithAccel() {
  static unsigned long lastTickMs = 0;
  int d = encoderDelta();
  if (d == 0) return 0;
  unsigned long now = millis();
  unsigned long dt = now - lastTickMs;
  lastTickMs = now;
  if (dt < 50) return d * 3;
  if (dt < 120) return d * 2;
  return d;
}

// IR hold acceleration
uint8_t lastIrCommand = 0;
uint8_t irRepeatCount = 0;
unsigned long lastIrEventMs = 0;
int irStepForRepeat(bool isRepeat) {
  if (!isRepeat) { irRepeatCount = 0; return 1; }
  if (millis() - lastIrEventMs > 150) { irRepeatCount = 0; return 1; }
  if (irRepeatCount < 6) irRepeatCount++;
  return 1 + irRepeatCount / 2;
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  pinMode(sw01, INPUT_PULLUP);
  pinMode(sw02, INPUT_PULLUP);
  pinMode(sw03, INPUT_PULLUP);
  pinMode(sw_power, OUTPUT);

  digitalWrite(sw_power, LOW);
  lcd.begin(16, 2);

  power = 0;
  eeprom_read();
  start_up();
  power_up();
}

void loop() {
  lcd_update();
  eeprom_update();
  ir_control();
  return_delay();

  if (menu_active == 0) {
    custom_num_shape();
  } else {
    custom_shape();
  }

  // Encoder push handling (menu)
  if (digitalRead(sw01) == LOW) {
    if (btn_press == 0) {
      btn_press = 1;
      btn_timer = millis();
    }
    if ((millis() - btn_timer > long_press_time) && (long_press == 0) && (menu_active == 0)) {
      long_press = 1;
      menu_active = 1;
      menu = 1;
      btn_cl();
      lcd.clear();
    } else if ((millis() - btn_timer > long_press_time) && (long_press == 0) && (menu_active == 1)) {
      long_press = 1;
      menu_active = 0;
      vol_menu = 0;
      reset = 0;
      btn_cl();
      lcd.clear();
    }
  } else {
    if (btn_press == 1) {
      if (long_press == 1) {
        long_press = 0;
      } else {
        if (menu_active == 1) {
          menu++;
          if (menu > 4) menu = 1;
          btn_cl();
          lcd.clear();
        } else if (menu_active == 0 && speaker_mode == 0) {
          vol_menu++;
          if (vol_menu > 6) vol_menu = 0;
          btn_cl();
        } else if (menu_active == 0 && speaker_mode == 1) {
          vol_menu++;
          if (vol_menu_jup == 0) {
            if (vol_menu > 2) { vol_menu = 6; vol_menu_jup = 1; }
          }
          if (vol_menu_jup == 1) {
            if (vol_menu > 6) { vol_menu = 0; vol_menu_jup = 0; }
          }
          btn_cl();
        }
      }
      btn_press = 0;
    }
  }

  // Encoder rotation: main screen volume trims with acceleration
  if (menu_active == 0) {
    int steps = encoderDeltaWithAccel();
    if (steps != 0) {
      if (vol_menu == 0) { mas_vol += steps; }
      if (vol_menu == 1) { fl_vol += steps; }
      if (vol_menu == 2) { fr_vol += steps; }
      if (vol_menu == 3) { sl_vol += steps; }
      if (vol_menu == 4) { sr_vol += steps; }
      if (vol_menu == 5) { cn_vol += steps; }
      if (vol_menu == 6) { sub_vol += steps; }
      set_mas_vol();
      set_fl(); set_fr(); set_sub();
      if (speaker_mode == 0) { set_sl(); set_sr(); set_cn(); }
      btn_cl();
    }
  }

  // Encoder rotation: menu adjustments
  if (menu_active == 1) {
    int steps = encoderDeltaWithAccel();
    if (steps != 0) {
      int dir = (steps > 0) ? 1 : -1;
      switch (menu) {
        case 1: surr += dir; set_surr(); break;
        case 2: speaker_mode += dir; if (speaker_mode == 1) vol_menu_jup = 0; set_speaker_mode(); break;
        case 3: mix += dir; set_mix(); break;
        case 4: reset++; set_reset(); break;
      }
      btn_cl();
    }
  }
}

// EEPROM -----------------------------------------------------
void eeprom_update() {
  static int prev_in = -1, prev_mas_vol = -100;
  static int prev_fl_vol = -100, prev_fr_vol = -100, prev_sl_vol = -100, prev_sr_vol = -100, prev_cn_vol = -100, prev_sub_vol = -100;
  static int prev_surr = -1, prev_speaker_mode = -1, prev_mix = -1;
  static bool pending = false;
  static unsigned long lastChangeMs = 0;

  bool changed =
    (in != prev_in) ||
    (mas_vol != prev_mas_vol) ||
    (fl_vol != prev_fl_vol) || (fr_vol != prev_fr_vol) ||
    (sl_vol != prev_sl_vol) || (sr_vol != prev_sr_vol) ||
    (cn_vol != prev_cn_vol) || (sub_vol != prev_sub_vol) ||
    (surr != prev_surr) || (speaker_mode != prev_speaker_mode) ||
    (mix != prev_mix);

  if (changed) {
    pending = true;
    lastChangeMs = millis();
  }

  if (pending && millis() - lastChangeMs >= 2000) {
    EEPROM.update(0, in);
    EEPROM.update(1, mas_vol);
    EEPROM.update(2, fl_vol + 10);
    EEPROM.update(3, fr_vol + 10);
    EEPROM.update(4, sl_vol + 10);
    EEPROM.update(5, sr_vol + 10);
    EEPROM.update(6, cn_vol + 10);
    EEPROM.update(7, sub_vol + 10);
    EEPROM.update(8, surr);
    EEPROM.update(9, speaker_mode);
    EEPROM.update(10, mix);

    prev_in = in;
    prev_mas_vol = mas_vol;
    prev_fl_vol = fl_vol; prev_fr_vol = fr_vol;
    prev_sl_vol = sl_vol; prev_sr_vol = sr_vol;
    prev_cn_vol = cn_vol; prev_sub_vol = sub_vol;
    prev_surr = surr; prev_speaker_mode = speaker_mode; prev_mix = mix;
    pending = false;
  }
}

void eeprom_read() {
  in = EEPROM.read(0);
  mas_vol = EEPROM.read(1);
  fl_vol = EEPROM.read(2) - 10;
  fr_vol = EEPROM.read(3) - 10;
  sl_vol = EEPROM.read(4) - 10;
  sr_vol = EEPROM.read(5) - 10;
  cn_vol = EEPROM.read(6) - 10;
  sub_vol = EEPROM.read(7) - 10;
  surr = EEPROM.read(8);
  speaker_mode = EEPROM.read(9);
  mix = EEPROM.read(10);
}

void return_delay() {
  if (millis() - time > 5000 && return_d == 1 && mute == 0 && menu_active != 0) {
    menu_active = 0;
    vol_menu = 0;
    reset = 0;
    return_d = 0;
    lcd.clear();
  } else if (millis() - time > 5000 && return_d == 1 && mute == 0 && menu_active == 0) {
    vol_menu = 0;
    return_d = 0;
  }
}

// Power up -----------------------------------------------------
void power_up() {
  if (power == 1) {
    lcd.clear();
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("   LOADING...   ");
    delay(1000);
    lcd.clear();
    if (mas_vol > 19) { mute = 0; }
    set_mute();
    vol_menu = 0;
    menu_active = 0;
    delay(300);
    ir_on = 1;
    vol_menu_jup = 0;
    digitalWrite(sw_power, HIGH);
  } else {
    digitalWrite(sw_power, LOW);
    mute = 1;
    set_mute();
    delay(100);
    menu_active = 100;
    ir_on = 0;
  }
}

void start_up() {
  mute = 1;
  set_mute();
  delay(500);
  lcd.setCursor(0, 0);
  lcd.print("    Ui Tech     ");
  delay(500);
  lcd.setCursor(0, 1);
  lcd.print("   5.1 SYSTEM   ");
  delay(1000);
  lcd.clear();
  delay(300);
  lcd.setCursor(0, 1);
  lcd.print("   LOADING...   ");
  delay(1500);
  lcd.clear();
  delay(300);
  AX2358();
  set_in();
  set_surr();
  set_mix();
  set_fl();
  set_fr();
  set_sl();
  set_sr();
  set_cn();
  set_sub();
}

// IR control --------------------------------------------------------------------------------
void ir_control() {
  if (!IrReceiver.decode()) return;

  auto &ir = IrReceiver.decodedIRData;
  bool isRepeat = (ir.flags & IRDATA_FLAGS_IS_REPEAT);
  uint8_t cmd = ir.command;

  int step = irStepForRepeat(isRepeat);
  lastIrEventMs = millis();
  lastIrCommand = cmd;

  if (cmd == NEC_CMD_POWER) {
    power = (power == 0) ? 1 : 0;
    power_up();
    IrReceiver.resume();
    return;
  }

  if (ir_on == 1) {
    if (cmd == NEC_CMD_IN0) { in = 0; set_in(); ir_cl(); IrReceiver.resume(); return; }
    if (cmd == NEC_CMD_IN1) { in = 1; set_in(); ir_cl(); IrReceiver.resume(); return; }
    if (cmd == NEC_CMD_IN2) { in = 2; set_in(); ir_cl(); IrReceiver.resume(); return; }
    if (cmd == NEC_CMD_IN3) { in = 3; set_in(); ir_cl(); IrReceiver.resume(); return; }
    if (cmd == NEC_CMD_IN4) { in = 4; set_in(); ir_cl(); IrReceiver.resume(); return; }

    if (cmd == NEC_CMD_MUTE && mas_vol != 19) {
      mute = (mute == 0) ? 1 : 0;
      menu_active = (mute == 1) ? 99 : 0;
      set_mute();
      lcd.clear();
      IrReceiver.resume();
      return;
    }
  }

  if (ir_on == 1 && menu_active == 0) {
    switch (cmd) {
      case NEC_CMD_VOL_UP:
        if (speaker_mode == 0 || speaker_mode == 1) {
          mas_vol += step;
          vol_menu = 0;
          set_mas_vol();
          set_fl(); set_fr(); set_sub();
          if (speaker_mode == 0) { set_sl(); set_sr(); set_cn(); }
        }
        break;
      case NEC_CMD_VOL_DN:
        if (speaker_mode == 0 || speaker_mode == 1) {
          mas_vol -= step;
          vol_menu = 0;
          set_mas_vol();
          set_fl(); set_fr(); set_sub();
          if (speaker_mode == 0) { set_sl(); set_sr(); set_cn(); }
        }
        break;
      case NEC_CMD_FL_UP:  if (speaker_mode == 0 || speaker_mode == 1) { fl_vol  += step; vol_menu = 1; set_fl(); }  break;
      case NEC_CMD_FL_DN:  if (speaker_mode == 0 || speaker_mode == 1) { fl_vol  -= step; vol_menu = 1; set_fl(); }  break;
      case NEC_CMD_FR_UP:  if (speaker_mode == 0 || speaker_mode == 1) { fr_vol  += step; vol_menu = 2; set_fr(); }  break;
      case NEC_CMD_FR_DN:  if (speaker_mode == 0 || speaker_mode == 1) { fr_vol  -= step; vol_menu = 2; set_fr(); }  break;
      case NEC_CMD_SL_UP:  if (speaker_mode == 0)                  { sl_vol  += step; vol_menu = 3; set_sl(); }  break;
      case NEC_CMD_SL_DN:  if (speaker_mode == 0)                  { sl_vol  -= step; vol_menu = 3; set_sl(); }  break;
      case NEC_CMD_SR_UP:  if (speaker_mode == 0)                  { sr_vol  += step; vol_menu = 4; set_sr(); }  break;
      case NEC_CMD_SR_DN:  if (speaker_mode == 0)                  { sr_vol  -= step; vol_menu = 4; set_sr(); }  break;
      case NEC_CMD_CN_UP:  if (speaker_mode == 0)                  { cn_vol  += step; vol_menu = 5; set_cn(); }  break;
      case NEC_CMD_CN_DN:  if (speaker_mode == 0)                  { cn_vol  -= step; vol_menu = 5; set_cn(); }  break;
      case NEC_CMD_SUB_UP: if (speaker_mode == 0 || speaker_mode == 1) { sub_vol += step; vol_menu = 6; set_sub(); } break;
      case NEC_CMD_SUB_DN: if (speaker_mode == 0 || speaker_mode == 1) { sub_vol -= step; vol_menu = 6; set_sub(); } break;
      case NEC_CMD_SP_MODE:
        speaker_mode++;
        if (speaker_mode == 1) { vol_menu_jup = 0; }
        vol_menu = 0;
        set_speaker_mode();
        break;
      case NEC_CMD_SURR_TG:
        surr++;
        vol_menu = 0;
        set_surr();
        break;
      case NEC_CMD_RESET:
        reset++;
        vol_menu = 0;
        set_reset();
        break;
    }
    ir_cl();
  }

  IrReceiver.resume();
}

// Custom shape --------------------------------------------------------------------------------
void custom_num_shape() {
  for (int i = 0; i < 8; i++) lcd.createChar(i, custom_num[i]);
}
void custom_shape() { lcd.createChar(1, arrow_right); }

// LCD ---------------------------------------------------------
void lcd_update() {
  int c = 0;
  switch (menu_active) {
    case 0:
      lcd.setCursor(0, 0);
      if (in == 0) lcd.print("IN1");
      if (in == 1) lcd.print("IN2");
      if (in == 2) lcd.print("IN3");
      if (in == 3) lcd.print("AUX");
      if (in == 4) lcd.print("DVD");

      lcd.setCursor(4, 0);
      if (speaker_mode == 0) lcd.print("5.1");
      if (speaker_mode == 1) lcd.print("2.1");

      switch (vol_menu) {
        case 0: lcd.setCursor(0, 1); lcd.print("MAS-VOL"); c = mas_vol - 19; break;
        case 1: lcd.setCursor(0, 1); lcd.print("FL-VOL "); c = fl_vol; break;
        case 2: lcd.setCursor(0, 1); lcd.print("FR-VOL "); c = fr_vol; break;
        case 3: lcd.setCursor(0, 1); lcd.print("SL-VOL "); c = sl_vol; break;
        case 4: lcd.setCursor(0, 1); lcd.print("SR-VOL "); c = sr_vol; break;
        case 5: lcd.setCursor(0, 1); lcd.print("CN-VOL "); c = cn_vol; break;
        case 6: lcd.setCursor(0, 1); lcd.print("SUB-VOL"); c = sub_vol; break;
      }
      break;

    case 1:
      switch (menu) {
        case 1:
          lcd.setCursor(0, 0); lcd.print("Surround");
          lcd.setCursor(1, 1); lcd.print("ON");
          lcd.setCursor(6, 1); lcd.print("OFF");
          if (surr == 0) { lcd.setCursor(0, 1); lcd.write(1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (surr == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write(1); }
          break;
        case 2:
          lcd.setCursor(0, 0); lcd.print("Speaker Mode");
          lcd.setCursor(1, 1); lcd.print("5.1");
          lcd.setCursor(6, 1); lcd.print("2.1");
          if (speaker_mode == 0) { lcd.setCursor(0, 1); lcd.write(1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (speaker_mode == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write(1); }
          break;
        case 3:
          lcd.setCursor(0, 0); lcd.print("-6dB");
          lcd.setCursor(1, 1); lcd.print("ON");
          lcd.setCursor(6, 1); lcd.print("OFF");
          if (mix == 0) { lcd.setCursor(0, 1); lcd.write(1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (mix == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write(1); }
          break;
        case 4:
          lcd.setCursor(0, 0); lcd.print("All Reset");
          lcd.setCursor(1, 1); lcd.print("Custom");
          lcd.setCursor(9, 1); lcd.print("Reset");
          if (reset == 0) { lcd.setCursor(0, 1); lcd.write(1); lcd.setCursor(8, 1); lcd.print(" "); }
          if (reset == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(8, 1); lcd.write(1); }
          break;
      }
      break;

    case 99:
      lcd.setCursor(0, 0); lcd.print("                ");
      lcd.setCursor(0, 1); lcd.print("      MUTE      ");
      break;

    case 100:
      lcd.setCursor(0, 0); lcd.print("                ");
      lcd.setCursor(0, 1); lcd.print("    STANDBY     ");
      break;
  }

  if (menu_active == 0) {
    int y;
    if (c < 0) { lcd.setCursor(8, 1); lcd.print("-"); y = 10 - (c + 10); }
    else if (c == -10) { lcd.setCursor(8, 1); lcd.print("-"); y = 10; }
    else { lcd.setCursor(8, 1); lcd.print(" "); y = c; }

    a = y / 10; b = y - a * 10;

    lcd.setCursor(9, 0); for (int i = 0; i < digit_width; i++) lcd.print(custom_num_top[a][i]);
    lcd.setCursor(9, 1); for (int i = 0; i < digit_width; i++) lcd.print(custom_num_bot[a][i]);
    lcd.setCursor(13, 0); for (int i = 0; i < digit_width; i++) lcd.print(custom_num_top[b][i]);
    lcd.setCursor(13, 1); for (int i = 0; i < digit_width; i++) lcd.print(custom_num_bot[b][i]);
  }
}

// All reset --------------------------------------------------------------------------------
void set_reset() {
  if (reset == 1) {
    in = 0;
    mas_vol = 44;
    fl_vol = 0; fr_vol = 0; sl_vol = 0; sr_vol = 0; cn_vol = 0; sub_vol = 0;
    speaker_mode = 0; surr = 0; mix = 0;
    vol_menu = 0; menu_active = 0; reset = 0;
    lcd.clear();
  }
  set_in(); set_fl(); set_fr(); set_sl(); set_sr(); set_cn(); set_sub();
  set_speaker_mode(); set_surr(); set_mix();
}

// Speaker mode --------------------------------------------------------------------------------
void set_speaker_mode() {
  if (speaker_mode > 1) speaker_mode = 0;
  if (speaker_mode < 0) speaker_mode = 1;
  switch (speaker_mode) {
    case 0: ch_mute = 0; break; // 5.1
    case 1: ch_mute = 1; break; // 2.1
  }
  set_sl(); set_sr(); set_cn();
}

// AX2358 settings -----------------------------------------------------
void set_in() {
  if (in > 4) in = 0;
  switch (in) {
    case 0: a = 0b11001011; break; // 1 input
    case 1: a = 0b11001010; break; // 2 input
    case 2: a = 0b11001001; break; // 3 input
    case 3: a = 0b11001000; break; // 4 input
    case 4: a = 0b11001111; break; // 6 CH input
  }
  AX2358_send(a);
}
void set_surr() {
  if (surr > 1) surr = 0;
  if (surr < 0) surr = 1;
  switch (surr) {
    case 0: a = 0b11000000; break; // Surround ON
    case 1: a = 0b11000001; break; // Surround OFF
  }
  AX2358_send(a);
}
void set_mix() {
  if (mix > 1) mix = 0;
  switch (mix) {
    case 0: a = 0b11000010; break; // (-6dB) on
    case 1: a = 0b11000011; break; // (-6dB) off
  }
  AX2358_send(a);
}

// AX2358 Volume settings ----------------------------------------------
void set_mas_vol() {
  if (mas_vol > 69) mas_vol = 69;
  if (mas_vol < 19) mas_vol = 19;
  if (mas_vol == 19) mute = 1; else mute = 0;
  set_mute();
}
void set_mute() {
  if (mute > 1) mute = 0;
  switch (mute) {
    case 0: ch_mute = 0; break;
    case 1: ch_mute = 1; break;
  }
  set_fl(); set_fr(); set_sub();
  if (speaker_mode == 0) { set_sl(); set_sr(); set_cn(); }
}
void set_fl() {
  if (fl_vol > 10) fl_vol = 10;
  if (fl_vol < -10) fl_vol = -10;
  fl = mas_vol + fl_vol;
  int c = 79 - fl; a = c / 10; b = c - a * 10;
  AX2358_vol(0b10000000 + a, 0b10010000 + b);
  switch (ch_mute) { case 0: x = 0b11110000; break; case 1: x = 0b11110001; break; }
  AX2358_send(x);
}
void set_fr() {
  if (fr_vol > 10) fr_vol = 10;
  if (fr_vol < -10) fr_vol = -10;
  fr = mas_vol + fr_vol;
  int c = 79 - fr; a = c / 10; b = c - a * 10;
  AX2358_vol(0b01000000 + a, 0b01010000 + b);
  switch (ch_mute) { case 0: x = 0b11110010; break; case 1: x = 0b11110011; break; }
  AX2358_send(x);
}
void set_cn() {
  if (cn_vol > 10) cn_vol = 10;
  if (cn_vol < -10) cn_vol = -10;
  cn = mas_vol + cn_vol;
  int c = 79 - cn; a = c / 10; b = c - a * 10;
  AX2358_vol(0b00000000 + a, 0b00010000 + b);
  switch (ch_mute) { case 0: x = 0b11110100; break; case 1: x = 0b11110101; break; }
  AX2358_send(x);
}
void set_sub() {
  if (sub_vol > 10) sub_vol = 10;
  if (sub_vol < -10) sub_vol = -10;
  sub = mas_vol + sub_vol;
  int c = 79 - sub; a = c / 10; b = c - a * 10;
  AX2358_vol(0b00100000 + a, 0b00110000 + b);
  switch (ch_mute) { case 0: x = 0b11110110; break; case 1: x = 0b11110111; break; }
  AX2358_send(x);
}
void set_sl() {
  if (sl_vol > 10) sl_vol = 10;
  if (sl_vol < -10) sl_vol = -10;
  sl = mas_vol + sl_vol;
  int c = 79 - sl; a = c / 10; b = c - a * 10;
  AX2358_vol(0b01100000 + a, 0b01110000 + b);
  switch (ch_mute) { case 0: x = 0b11111000; break; case 1: x = 0b11111001; break; }
  AX2358_send(x);
}
void set_sr() {
  if (sr_vol > 10) sr_vol = 10;
  if (sr_vol < -10) sr_vol = -10;
  sr = mas_vol + sr_vol;
  int c = 79 - sr; a = c / 10; b = c - a * 10;
  AX2358_vol(0b10100000 + a, 0b10110000 + b);
  switch (ch_mute) { case 0: x = 0b11111010; break; case 1: x = 0b11111011; break; }
  AX2358_send(x);
}

// AX2358 send -----------------------------------------------------
void AX2358_send(char c) {
  Wire.beginTransmission(AX2358_address);
  Wire.write(c);
  Wire.endTransmission();
}
void AX2358() {
  Wire.beginTransmission(AX2358_address);
  Wire.write(0b11000100);
  Wire.endTransmission();
}
void AX2358_vol(char c, char d) {
  Wire.beginTransmission(AX2358_address);
  Wire.write(c);
  Wire.write(d);
  Wire.endTransmission();
}