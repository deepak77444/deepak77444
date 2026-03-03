# Ultra Digital 5.1 Home Theater - Quick Reference Card

## 🎮 Remote Control Quick Guide

### System Control
| Button | Code | Function |
|--------|------|----------|
| POWER | 0x807F827D | Standby ON/OFF |
| MUTE | 0x807F42BD | Mute audio |
| RESET | 0x807F1AE5 | Reset to defaults |
| SURROUND | 0x807FA857 | 5.1 ON/OFF |
| TEST TONE | 0x807F02FD | Test mode |

### Volume Control (Master)
| Button | Code | Step |
|--------|------|------|
| VOL + | 0x807F906F | +1 |
| VOL − | 0x807FA05F | −1 |

### Input Selection
| Button | Code | Input |
|--------|------|-------|
| AUX-1 | 0x807F52AD | Analog 1 |
| AUX-2 | 0x807FA25D | Analog 2 |
| AUX-3 | 0x807F22DD | Analog 3 |
| USB | 0x807F629D | USB MP3 |

### Channel Trim
| Button | Code | Channel |
|--------|------|---------|
| FRONT + | 0x807F40BF | FL+FR up |
| FRONT − | 0x807FC03F | FL+FR down |
| SURR + | 0x807F00FF | SL+SR up |
| SURR − | 0x807F807F | SL+SR down |
| CENTER + | 0x807F50AF | Center up |
| CENTER − | 0x807F609F | Center down |
| SUB + | 0x807FD02F | Sub up |
| SUB − | 0x807FE01F | Sub down |

### Tone Control
| Button | Code | Parameter |
|--------|------|-----------|
| BASS + | 0x807F48B7 | Bass +1 |
| BASS − | 0x807FC837 | Bass −1 |
| TREBLE + | 0x807F08F7 | Treble +1 |
| TREBLE − | 0x807F8877 | Treble −1 |
| GAIN + | 0x807F926D | Gain +1 |
| GAIN − | 0x807FB04F | Gain −1 |

## 🎛️ Rotary Encoder Functions

### Rotation
- **CW (Clockwise)**: Increase current parameter
- **CCW (Counter-Clockwise)**: Decrease current parameter

### Short Press - Mode Cycle
```
MASTER → FRONT → SURROUND → CENTER → SUB → GAIN → BASS → TREBLE
→ USB → AUX-1 → AUX-2 → AUX-3 → DVD → [back to MASTER]
```
**Auto-return**: Returns to MASTER after 5 seconds

### Long Press (2-3 seconds)
- Enter/Exit Standby
- LCD OFF
- Mute audio
- Save settings

## 📺 LCD Display Layout (16×2)

```
┌────────────────┐
│VOL: 50      MS │  ← Mode:Value  M=Mute S=Surround
│AUX-1    FT:AUX │  ← Input      FT003 mode
└────────────────┘
```

### Mode Abbreviations
- **VOL** = Master Volume
- **FRN** = Front L+R
- **SUR** = Surround L+R
- **CEN** = Center
- **SUB** = Subwoofer
- **GAN** = Gain
- **BAS** = Bass
- **TRE** = Treble
- **INP** = Input Select

### FT003 Modes
- **FT:AUX** = Auxiliary analog
- **FT:OPT** = Optical digital
- **FT:COX** = Coaxial digital
- **FT:HDM** = HDMI audio

## 🔊 Test Tone Mode

### Activation
Press **TEST TONE** button

### Channel Sequence
```
1. FL  (Front Left)
2. FR  (Front Right)
3. SR  (Surround Right)
4. SL  (Surround Left)
5. CENTER
6. SUB (Subwoofer)
```

### Controls
- **Encoder Rotation**: Adjust active channel volume
- **TEST TONE**: Next channel or exit

## ⚙️ Special Modes

### Welcome Name Edit
**Activate**: Long-press **AUX-1**

**Controls**:
- NUM 1-6 → Scroll letters
- NUM 0 → Space
- NEXT → Next character
- RESET → Save & exit

### Model Set Mode
**Activate**: Long-press **NUM-1 + NUM-0**

**Options**:
- NUM 1-7 → Select model
- NUM 0 → A/B toggle

### USB Board Select
- Long-press **4** → USB MP3
- Long-press **5** → USB Real-Play
- Long-press **6** → USB Wire

## 📊 Parameter Ranges

| Parameter | Min | Max | Default |
|-----------|-----|-----|---------|
| Master Volume | 0 | 100 | 50 |
| Channel Volume | 0 | 100 | 50 |
| Bass | −10 | +10 | 0 |
| Treble | −10 | +10 | 0 |
| Gain | −10 | +10 | 0 |

## 🔌 Quick Hardware Check

### LCD (Parallel - NO I²C)
- RS → Pin 12
- EN → Pin 11
- D4 → Pin 5
- D5 → Pin 4
- D6 → Pin 3
- D7 → Pin 2

### IR Receiver
- OUT → Pin 6
- Must be 38kHz NEC type

### Rotary Encoder
- CLK → Pin 7
- DT → Pin 8
- SW → Pin 9

### FT003 Control
- A → A0
- B → A1
- SW → A2

### Audio IC (R2S15902FP)
- CS → Pin 13
- CLK → A3
- DATA → A4

### USB Power
- Control → Pin 10

## 🆘 Emergency Reset

**Method 1**: Press **RESET** button on remote

**Method 2**: 
1. Power off
2. Hold encoder button
3. Power on while holding
4. Release after 5 seconds

## 💾 Settings Stored in EEPROM

✅ All volumes (Master + 4 channels)  
✅ All tones (Bass, Treble, Gain)  
✅ Input selection  
✅ FT003 mode  
✅ Surround ON/OFF  
✅ Model type  
✅ USB board type  
✅ Welcome message (16 chars)  

**Auto-save**: On standby entry and mode changes

## 🎯 Troubleshooting Quick Fixes

| Problem | Quick Fix |
|---------|-----------|
| LCD blank | Adjust contrast pot |
| IR not working | Check pin 6 & receiver orientation |
| Encoder jumpy | Check connections, may need debounce cap |
| No audio change | Verify R2S15902FP wiring |
| Settings lost | Check EEPROM, may need re-initialization |
| Display flicker | Check 5V power supply stability |

## 📱 USB Media Controls

For IR-controlled USB boards:

| Button | Code | Function |
|--------|------|----------|
| PLAY/PAUSE | 0xFF30CF | Toggle |
| NEXT | 0xFFA25D | Next track |
| PREV | 0xFFE21D | Previous |
| MODE | 0xFF6897 | Playback mode |
| EQ | 0xFF20DF | Equalizer |

*Note: USB commands use 0xFF prefix*

## ⚡ Power Management

**Normal Mode**: All systems active  
**Standby Mode**: 
- Display OFF
- Audio MUTED
- Settings SAVED
- USB power maintained

**Power Consumption**:
- Active: ~150-200mA @ 5V
- Standby: ~50mA @ 5V

## 🏭 Factory Specifications

- Protocol: NEC 32-bit
- Carrier: 38kHz
- Display: HD44780 parallel
- Update rate: 10Hz (100ms)
- Debounce: Built-in
- EEPROM wear leveling: Yes

---

**Quick Start**: Power on → Test IR → Rotate encoder → Short press → Success! 🎉

**Version**: 3.0 Factory Grade  
**Platform**: Arduino UNO (ATmega328P)
