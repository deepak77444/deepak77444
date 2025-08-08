// AX2358 5.1 Surround System
// DaacWaves <https://daacwaves.blogspot.com>

#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <IRremote.h>

#define AX2358_address 0b1001010

#define btn_delay 20

#define sw01 9          // Encoder push (SW)
#define sw02 11         // Encoder DT
#define sw03 10         // Encoder CLK

#define sw_power 13     // Power control out

#define usb_5v_pin 12   // USB 5V control output (HIGH when input=USB)

// IR HEX code
#define ir_power      0x807F827D    // IR power ON/OFF
#define ir_mute       0x807F42BD    // IR mute
#define ir_in_0       0x807F629D    // IR input USB
#define ir_in_1       0x807F52AD    // IR input BLU
#define ir_in_2       0x807FA25D    // IR input FM
#define ir_in_3       0x807F22DD    // IR input AUX
#define ir_in_4       0x807F20DF    // IR input DVD
#define ir_vol_i      0x807F906F    // IR vol++
#define ir_vol_d      0x807FA05F    // IR vol--
#define ir_fl_i       0x807F40BF    // IR fl++
#define ir_fl_d       0x807FC03F    // IR fl--
#define ir_fr_i       0x807F00FF    // IR fr++
#define ir_fr_d       0x807F807F    // IR fr--
#define ir_sl_i       0x807F48B7    // IR sl++
#define ir_sl_d       0x807FC837    // IR sl--
#define ir_sr_i       0x807F08F7    // IR sr++
#define ir_sr_d       0x807F8877    // IR sr--
#define ir_cn_i       0x807F50AF    // IR cn++
#define ir_cn_d       0x807F609F    // IR cn--
#define ir_sub_i      0x807FD02F    // IR sub++
#define ir_sub_d      0x807FE01F    // IR sub--
#define ir_sp_mode    0x807F0AF5    // IR speaker mode change
#define ir_surr_mode  0x807FA857    // IR surround ON/OFF
#define ir_mix_mode   0x00000000    // IR -6dB ON/OFF (placeholder)
#define ir_reset      0x807F1AE5    // IR reset

// USB panel Bluetooth/FM player IR codes
#define irp_vol_plus    0x807F926D
#define irp_prev_chm    0x807F728D
#define irp_play_pause  0x807FB24D
#define irp_next_chp    0x807F32CD
#define irp_vol_minus   0x807FB04F
#define irp_eq          0x807FF00F
#define irp_mode        0x807F9867

IRrecv irrecv(8);
decode_results results;

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

const uint8_t digit_width = 3;
const uint8_t custom_num_top[10][digit_width] = {
  {0, 1, 2}, {1, 2, 32}, {6, 6, 2}, {6, 6, 2}, {3, 4, 7},
  {7, 6, 6}, {0, 6, 6}, {1, 1, 2}, {0, 6, 2}, {0, 6, 2}
};
const uint8_t custom_num_bot[10][digit_width] = {
  {3, 4, 5}, {4, 7, 4}, {7, 4, 4}, {4, 4, 5}, {32, 32, 7},
  {4, 4, 5}, {32, 32, 7}, {3, 4, 5}, {4, 4, 5}, {4, 4, 5}
};

byte arrow_right[8] = {B00000, B10000, B11000, B11100, B11110, B11100, B11000, B10000};

LiquidCrystal lcd(7, 6, 5, 4, 3, 2); // RS,E,D4,D5,D6,D7

unsigned long time;
int in, mute, return_d, surr, mix, a, b, x, power, menu, menu_active, ch_mute, speaker_mode, btn_press, long_press, vol_menu, vol_menu_jup, reset;
int fl, fr, sl, sr, cn, sub, ir_menu, ir_on, mas_vol, fl_vol, fr_vol, sl_vol, sr_vol, cn_vol, sub_vol;

long btn_timer = 0;
long long_press_time = 600;

int last_custom_state = -1;

// Read rotary encoder step: -1, 0, +1
int readEncoderStep() {
  static int lastClk = HIGH;
  static unsigned long lastEdgeUs = 0;
  const unsigned long debounceUs = 2500; // increased debounce for stability

  int clk = digitalRead(sw03); // CLK
  int dt  = digitalRead(sw02); // DT

  if (clk != lastClk) {
    lastClk = clk;
    unsigned long now = micros();
    if ((now - lastEdgeUs) > debounceUs && clk == LOW) {
      lastEdgeUs = now;
      return (dt != clk) ? +1 : -1; // Reverse if needed
    }
  }
  return 0;
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  irrecv.enableIRIn();

  pinMode(sw01, INPUT_PULLUP);  // Encoder push
  pinMode(sw02, INPUT_PULLUP);  // Encoder DT
  pinMode(sw03, INPUT_PULLUP);  // Encoder CLK
  pinMode(sw_power, OUTPUT); // Out
  pinMode(usb_5v_pin, OUTPUT); // USB 5V control

  digitalWrite(sw_power, LOW);

  lcd.begin(16, 2);

  // Load digit custom characters so first frame is correct
  custom_num_shape();
  last_custom_state = 0;

  // Ensure USB 5V is off at boot
  digitalWrite(usb_5v_pin, LOW);

  power = 1; // Default ON (physical power button removed)
  eeprom_read();
  start_up();
  power_up();
}

void loop() {
  // Ensure the correct custom set is loaded BEFORE drawing
  if (menu_active == 0) {
    if (last_custom_state != 0) { custom_num_shape(); last_custom_state = 0; }
  } else {
    if (last_custom_state != 1) { custom_shape(); last_custom_state = 1; }
  }

  lcd_update();
  eeprom_update();
  ir_control();
  return_delay();

  // Only IR and encoder now (Input/Mute/Power physical buttons removed)
  if (power == 1) {
    // select menu via encoder push
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
        custom_shape();
        last_custom_state = 1;
      } else if ((millis() - btn_timer > long_press_time) && (long_press == 0) && (menu_active == 1)) {
        long_press = 1;
        menu_active = 0;
        vol_menu = 0;
        reset = 0;
        btn_cl();
        lcd.clear();
        custom_num_shape();
        last_custom_state = 0;
      }
    } else {
      if (btn_press == 1) {
        if (long_press == 1) {
          long_press = 0;
        } else {
          if (menu_active == 1) {
            menu++;
            if (menu > 4) {
              menu = 1;
            }
            btn_cl();
            lcd.clear();
            custom_shape();
            last_custom_state = 1;
          } else if (menu_active == 0 && speaker_mode == 0) {
            vol_menu++;
            if (vol_menu > 6) {
              vol_menu = 0;
            }
            btn_cl();
          } else if (menu_active == 0 && speaker_mode == 1) {
            vol_menu++;
            if (vol_menu_jup == 0) {
              if (vol_menu > 2) {
                vol_menu = 6;
                vol_menu_jup = 1;
              }
            }
            if (vol_menu_jup == 1) {
              if (vol_menu > 6) {
                vol_menu = 0;
                vol_menu_jup = 0;
              }
            }
            btn_cl();
          }
        }
        btn_press = 0;
      }
    }
  }

  // menu active 0: fast encoder adjusts volumes
  if (menu_active == 0) {
    int step = readEncoderStep();
    if (step != 0) {
      if (vol_menu == 0) mas_vol += step;
      if (vol_menu == 1) fl_vol  += step;
      if (vol_menu == 2) fr_vol  += step;
      if (vol_menu == 3) sl_vol  += step;
      if (vol_menu == 4) sr_vol  += step;
      if (vol_menu == 5) cn_vol  += step;
      if (vol_menu == 6) sub_vol += step;

      set_mas_vol();
      set_fl();
      set_fr();
      set_sub();
      if (speaker_mode == 0) {
        set_sl();
        set_sr();
        set_cn();
      }
      btn_cl();
    }
  }

  // menu active 1: encoder toggles options
  if (menu_active == 1) {
    int step = readEncoderStep();
    if (step != 0) {
      if (menu == 1) { surr += (step > 0 ? 1 : -1); set_surr(); btn_cl(); }
      if (menu == 2) { speaker_mode += (step > 0 ? 1 : -1); set_speaker_mode(); btn_cl(); }
      if (menu == 3) { mix += (step > 0 ? 1 : -1); set_mix(); btn_cl(); }
      if (menu == 4) { reset += (step > 0 ? 1 : -1); set_reset(); btn_cl(); }
    }
  }
}

// eeprom -----------------------------------------------------//

void eeprom_update() {
  static unsigned long lastMs = 0;
  static int prev_in=-1, prev_mas=-1, prev_fl=999, prev_fr=999, prev_sl=999, prev_sr=999, prev_cn=999, prev_sub=999, prev_surr=-1, prev_spm=-1, prev_mix=-1;
  if (millis() - lastMs < 2000) return;  // throttle
  lastMs = millis();

  if (prev_in  != in)       { EEPROM.update(0, in);                 prev_in = in; }
  if (prev_mas != mas_vol)  { EEPROM.update(1, mas_vol);            prev_mas = mas_vol; }
  if (prev_fl  != fl_vol)   { EEPROM.update(2, fl_vol + 10);        prev_fl = fl_vol; }
  if (prev_fr  != fr_vol)   { EEPROM.update(3, fr_vol + 10);        prev_fr = fr_vol; }
  if (prev_sl  != sl_vol)   { EEPROM.update(4, sl_vol + 10);        prev_sl = sl_vol; }
  if (prev_sr  != sr_vol)   { EEPROM.update(5, sr_vol + 10);        prev_sr = sr_vol; }
  if (prev_cn  != cn_vol)   { EEPROM.update(6, cn_vol + 10);        prev_cn = cn_vol; }
  if (prev_sub != sub_vol)  { EEPROM.update(7, sub_vol + 10);       prev_sub = sub_vol; }
  if (prev_surr!= surr)     { EEPROM.update(8, surr);               prev_surr = surr; }
  if (prev_spm != speaker_mode){ EEPROM.update(9, speaker_mode);    prev_spm = speaker_mode; }
  if (prev_mix != mix)      { EEPROM.update(10, mix);               prev_mix = mix; }
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

void btn_cl() {
  delay(btn_delay);
  time = millis();
  return_d = 1;
}
void ir_cl() {
  time = millis();
  return_d = 1;
}
void return_delay() {
  if (millis() - time > 5000 && return_d == 1 && mute == 0 && menu_active != 0) {
    menu_active = 0;
    vol_menu = 0;
    reset = 0;
    return_d = 0;
    lcd.clear();
    custom_num_shape();
    last_custom_state = 0;
  } else if (millis() - time > 5000 && return_d == 1 && mute == 0 && menu_active == 0) {
    vol_menu = 0;
    return_d = 0;
  }
}

// power up -----------------------------------------------------//

void power_up() {
  if (power == 1) {
    lcd.clear();
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("   LOADING...   ");
    delay(1000);
    lcd.clear();
    if (mas_vol > 19) {
      mute = 0;
    }
    set_mute();
    vol_menu = 0;
    menu_active = 0;
    delay(300);
    ir_on = 1;
    vol_menu_jup = 0;
    digitalWrite(sw_power, HIGH);

    // Update USB 5V state based on current input
    if (in == 0) digitalWrite(usb_5v_pin, HIGH); else digitalWrite(usb_5v_pin, LOW);

    // Ensure digits are loaded for main screen
    custom_num_shape();
    last_custom_state = 0;
  } else {
    digitalWrite(sw_power, LOW);
    digitalWrite(usb_5v_pin, LOW); // Always off when main power off
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

// IR control --------------------------------------------------------------------------------//

void ir_control() {
  if (irrecv.decode(&results)) {
    static uint32_t lastIrCode = 0;
    bool isRepeat = false;
    uint32_t codeVal = results.value;
    if (codeVal == 0xFFFFFFFF) { // NEC repeat
      codeVal = lastIrCode;
      isRepeat = true;
    } else {
      lastIrCode = codeVal;
    }

    switch (codeVal) {
      // power -------------------------------------------------//
      case ir_power:
        power = (power == 0) ? 1 : 0;
        power_up();
        break;
    }
    if (ir_on == 1) {
      switch (codeVal) {
        // mute -------------------------------------------------//
        case ir_mute:
          if (mas_vol != 19) {
            mute = (mute == 0) ? 1 : 0;
            menu_active = (mute == 1) ? 99 : 0;
            set_mute();
            lcd.clear();
            if (menu_active == 0) { custom_num_shape(); last_custom_state = 0; }
          }
          break;

        // select input -------------------------------------------------//
        case ir_in_0: in = 0; set_in(); ir_cl(); break;
        case ir_in_1: in = 1; set_in(); ir_cl(); break;
        case ir_in_2: in = 2; set_in(); ir_cl(); break;
        case ir_in_3: in = 3; set_in(); ir_cl(); break;
        case ir_in_4: in = 4; set_in(); ir_cl(); break;
      }
    }

    // USB panel Bluetooth/FM player IR mapping (always active)
    switch (codeVal) {
      case irp_vol_plus:   mas_vol++; set_mas_vol(); set_fl(); set_fr(); set_sub(); if (speaker_mode==0){ set_sl(); set_sr(); set_cn(); } break;
      case irp_vol_minus:  mas_vol--; set_mas_vol(); set_fl(); set_fr(); set_sub(); if (speaker_mode==0){ set_sl(); set_sr(); set_cn(); } break;
      case irp_prev_chm:   in--; set_in(); break;  // cycle input backward
      case irp_next_chp:   in++; set_in(); break;  // cycle input forward
      case irp_mode:       speaker_mode++; set_speaker_mode(); break; // toggle 5.1/2.1
      case irp_eq:         surr++; set_surr(); break; // repurpose EQ as surround toggle
      case irp_play_pause: /* no-op in amp; keep for future */ break;
    }

    if (ir_on == 1 && menu_active == 0) {
      switch (codeVal) {
        // VOL -------------------------------------------------//
        case ir_vol_i: {
          if (speaker_mode == 0 || speaker_mode == 1) {
            uint8_t steps = isRepeat ? 3 : 1;
            while (steps--) {
              mas_vol++;
              set_mas_vol();
              set_fl();
              set_fr();
              set_sub();
              if (speaker_mode == 0) {
                set_sl();
                set_sr();
                set_cn();
              }
            }
            vol_menu = 0;
          }
        } break;

        case ir_vol_d: {
          if (speaker_mode == 0 || speaker_mode == 1) {
            uint8_t steps = isRepeat ? 3 : 1;
            while (steps--) {
              mas_vol--;
              set_mas_vol();
              set_fl();
              set_fr();
              set_sub();
              if (speaker_mode == 0) {
                set_sl();
                set_sr();
                set_cn();
              }
            }
            vol_menu = 0;
          }
        } break;

        // FL -------------------------------------------------//
        case ir_fl_i:
          if (speaker_mode == 0 || speaker_mode == 1) { fl_vol++; vol_menu = 1; set_fl(); }
          break;
        case ir_fl_d:
          if (speaker_mode == 0 || speaker_mode == 1) { fl_vol--; vol_menu = 1; set_fl(); }
          break;

        // FR -------------------------------------------------//
        case ir_fr_i:
          if (speaker_mode == 0 || speaker_mode == 1) { fr_vol++; vol_menu = 2; set_fr(); }
          break;
        case ir_fr_d:
          if (speaker_mode == 0 || speaker_mode == 1) { fr_vol--; vol_menu = 2; set_fr(); }
          break;

        // SL -------------------------------------------------//
        case ir_sl_i:
          if (speaker_mode == 0) { sl_vol++; vol_menu = 3; set_sl(); }
          break;
        case ir_sl_d:
          if (speaker_mode == 0) { sl_vol--; vol_menu = 3; set_sl(); }
          break;

        // SR -------------------------------------------------//
        case ir_sr_i:
          if (speaker_mode == 0) { sr_vol++; vol_menu = 4; set_sr(); }
          break;
        case ir_sr_d:
          if (speaker_mode == 0) { sr_vol--; vol_menu = 4; set_sr(); }
          break;

        // CN -------------------------------------------------//
        case ir_cn_i:
          if (speaker_mode == 0) { cn_vol++; vol_menu = 5; set_cn(); }
          break;
        case ir_cn_d:
          if (speaker_mode == 0) { cn_vol--; vol_menu = 5; set_cn(); }
          break;

        // SUB -------------------------------------------------//
        case ir_sub_i:
          if (speaker_mode == 0 || speaker_mode == 1) { sub_vol++; vol_menu = 6; set_sub(); }
          break;
        case ir_sub_d:
          if (speaker_mode == 0 || speaker_mode == 1) { sub_vol--; vol_menu = 6; set_sub(); }
          break;

        // speaker mode -------------------------------------------------//
        case ir_sp_mode:
          speaker_mode++;
          vol_menu = 0;
          if (speaker_mode == 1) {
            vol_menu_jup = 0;
          }
          set_speaker_mode();
          break;

        // surround -------------------------------------------------//
        case ir_surr_mode:
          surr++;
          vol_menu = 0;
          set_surr();
          break;

        // -------------------------------------------------//
        case ir_mix_mode:
          mix++;
          vol_menu = 0;
          set_mix();
          break;

        // -------------------------------------------------//
        case ir_reset:
          reset++;
          vol_menu = 0;
          set_reset();
          break;
      }
      ir_cl();
    }
    irrecv.resume();
  }
}

// custom shape --------------------------------------------------------------------------------//

void custom_num_shape() {
  for (int i = 0; i < 8; i++)
    lcd.createChar(i, custom_num[i]);
}

void custom_shape() {
  lcd.createChar(1, arrow_right);
}

// lcd ---------------------------------------------------------//

void lcd_update() {
  int c;
  switch (menu_active) {
    case 0:
      // input -------------------------------------------------//
      lcd.setCursor(0, 0);
      if (in == 0) lcd.print("USB");
      if (in == 1) lcd.print("IN2");
      if (in == 2) lcd.print("IN3");
      if (in == 3) lcd.print("AUX");
      if (in == 4) lcd.print("DVD");

      // speaker mode ------------------------------------------//
      lcd.setCursor(4, 0);
      if (speaker_mode == 0) lcd.print("5.1");
      if (speaker_mode == 1) lcd.print("2.1");

      // vol ----------------------------------------------//
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
          if (surr == 0) { lcd.setCursor(0, 1); lcd.write((uint8_t)1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (surr == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write((uint8_t)1); }
          break;

        case 2:
          lcd.setCursor(0, 0); lcd.print("Speaker Mode");
          lcd.setCursor(1, 1); lcd.print("5.1");
          lcd.setCursor(6, 1); lcd.print("2.1");
          if (speaker_mode == 0) { lcd.setCursor(0, 1); lcd.write((uint8_t)1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (speaker_mode == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write((uint8_t)1); }
          break;

        case 3:
          lcd.setCursor(0, 0); lcd.print("-6dB");
          lcd.setCursor(1, 1); lcd.print("ON");
          lcd.setCursor(6, 1); lcd.print("OFF");
          if (mix == 0) { lcd.setCursor(0, 1); lcd.write((uint8_t)1); lcd.setCursor(5, 1); lcd.print(" "); }
          if (mix == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(5, 1); lcd.write((uint8_t)1); }
          break;

        case 4:
          lcd.setCursor(0, 0); lcd.print("All Reset");
          lcd.setCursor(1, 1); lcd.print("Custom");
          lcd.setCursor(9, 1); lcd.print("Reset");
          if (reset == 0) { lcd.setCursor(0, 1); lcd.write((uint8_t)1); lcd.setCursor(8, 1); lcd.print(" "); }
          if (reset == 1) { lcd.setCursor(0, 1); lcd.print(" "); lcd.setCursor(8, 1); lcd.write((uint8_t)1); }
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
    if (c < 0) {
      lcd.setCursor(8, 1); lcd.print("-");
      y = -c; // absolute value
    } else {
      lcd.setCursor(8, 1); lcd.print(" ");
      y = c;
    }
    a = y / 10;
    b = y - a * 10;

    lcd.setCursor(9, 0);
    for (int i = 0; i < digit_width; i++) lcd.write(custom_num_top[a][i]);

    lcd.setCursor(9, 1);
    for (int i = 0; i < digit_width; i++) lcd.write(custom_num_bot[a][i]);

    lcd.setCursor(13, 0);
    for (int i = 0; i < digit_width; i++) lcd.write(custom_num_top[b][i]);

    lcd.setCursor(13, 1);
    for (int i = 0; i < digit_width; i++) lcd.write(custom_num_bot[b][i]);
  }
}

// all reset --------------------------------------------------------------------------------//

void set_reset() {
  if (reset == 1) {
    in = 0;
    mas_vol = 44;
    fl_vol = 0;
    fr_vol = 0;
    sl_vol = 0;
    sr_vol = 0;
    cn_vol = 0;
    sub_vol = 0;
    speaker_mode = 0;
    surr = 0;
    mix = 0;
    vol_menu = 0;
    menu_active = 0;
    reset = 0;
    lcd.clear();
    custom_num_shape();
    last_custom_state = 0;
  }
  set_in();
  set_fl();
  set_fr();
  set_sl();
  set_sr();
  set_cn();
  set_sub();
  set_speaker_mode();
  set_surr();
  set_mix();
}

// speaker mode --------------------------------------------------------------------------------//

void set_speaker_mode() {
  if (speaker_mode > 1) speaker_mode = 0;
  if (speaker_mode < 0) speaker_mode = 1;
  switch (speaker_mode) {
    case 0: ch_mute = 0; break; // 5.1 mode
    case 1: ch_mute = 1; break; // 2.1 mode
  }
  set_sl();
  set_sr();
  set_cn();
}

// AX2358 settings -----------------------------------------------------//

void set_in() {
  if (in > 4) in = 0;
  if (in < 0) in = 4;
  switch (in) {
    case 0: a = 0b11001011; break; // USB (Input 1)
    case 1: a = 0b11001010; break; // 2 input
    case 2: a = 0b11001001; break; // 3 input
    case 3: a = 0b11001000; break; // 4 input
    case 4: a = 0b11001111; break; // 6 CH input
  }
  // Drive USB 5V pin: ON only when USB input selected and power is ON
  if (power == 1 && in == 0) {
    digitalWrite(usb_5v_pin, HIGH);
  } else {
    digitalWrite(usb_5v_pin, LOW);
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
  if (mix < 0) mix = 1;
  switch (mix) {
    case 0: a = 0b11000010; break; // (-6dB) on
    case 1: a = 0b11000011; break; // (-6dB) off
  }
  AX2358_send(a);
}

// AX2358 Volume settings ----------------------------------------------//

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
  set_fl();
  set_fr();
  set_sub();
  if (speaker_mode == 0) {
    set_sl();
    set_sr();
    set_cn();
  }
}
void set_fl() {
  if (fl_vol > 10) fl_vol = 10;
  if (fl_vol < -10) fl_vol = -10;
  fl = mas_vol + fl_vol;
  int c = 79 - fl;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b10000000 + a, 0b10010000 + b);  // CH1

  switch (ch_mute) {
    case 0: x = 0b11110000; break; // Mute disabled
    case 1: x = 0b11110001; break; // Mute
  }
  AX2358_send(x);
}
void set_fr() {
  if (fr_vol > 10) fr_vol = 10;
  if (fr_vol < -10) fr_vol = -10;
  fr = mas_vol + fr_vol;
  int c = 79 - fr;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b01000000 + a, 0b01010000 + b);  // CH2

  switch (ch_mute) {
    case 0: x = 0b11110010; break; // Mute disabled
    case 1: x = 0b11110011; break; // Mute
  }
  AX2358_send(x);
}
void set_cn() {
  if (cn_vol > 10) cn_vol = 10;
  if (cn_vol < -10) cn_vol = -10;
  cn = mas_vol + cn_vol;
  int c = 79 - cn;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b00000000 + a, 0b00010000 + b);  // CH3

  switch (ch_mute) {
    case 0: x = 0b11110100; break; // Mute disabled
    case 1: x = 0b11110101; break; // Mute
  }
  AX2358_send(x);
}
void set_sub() {
  if (sub_vol > 10) sub_vol = 10;
  if (sub_vol < -10) sub_vol = -10;
  sub = mas_vol + sub_vol;
  int c = 79 - sub;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b00100000 + a, 0b00110000 + b);  // CH4

  switch (ch_mute) {
    case 0: x = 0b11110110; break; // Mute disabled
    case 1: x = 0b11110111; break; // Mute
  }
  AX2358_send(x);
}
void set_sl() {
  if (sl_vol > 10) sl_vol = 10;
  if (sl_vol < -10) sl_vol = -10;
  sl = mas_vol + sl_vol;
  int c = 79 - sl;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b01100000 + a, 0b01110000 + b);  // CH5

  switch (ch_mute) {
    case 0: x = 0b11111000; break; // Mute disabled
    case 1: x = 0b11111001; break; // Mute
  }
  AX2358_send(x);
}
void set_sr() {
  if (sr_vol > 10) sr_vol = 10;
  if (sr_vol < -10) sr_vol = -10;
  sr = mas_vol + sr_vol;
  int c = 79 - sr;
  a = c / 10;
  b = c - a * 10;
  AX2358_vol(0b10100000 + a, 0b10110000 + b);  // CH6

  switch (ch_mute) {
    case 0: x = 0b11111010; break; // Mute disabled
    case 1: x = 0b11111011; break; // Mute
  }
  AX2358_send(x);
}

// AX2358 send -----------------------------------------------------//

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

// end code