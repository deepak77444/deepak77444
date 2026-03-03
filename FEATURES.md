# Ultra Digital 5.1 Home Theater Remote Kit - Features Documentation

## Overview
Factory-accurate firmware for Ultra Digital 5.1 Home Theater Remote Kit Ver 3.0. Designed to behave exactly like shop-sold market kits.

## Core Features

### 1. IR Remote Control (NEC Protocol)
- **Full 32-bit NEC protocol support**
- Complete keymap with exact hex codes
- System commands (power, mute, reset, surround, test tone)
- Input selection (AUX-1, AUX-2, AUX-3, USB)
- Master volume control
- Individual channel volume control (Front, Surround, Center, Sub)
- Tone controls (Bass, Treble, Gain)
- USB media controls (Play/Pause, Next, Prev, Mode, EQ)
- Numeric keypad (0-9) with special functions

### 2. Rotary Encoder Control

#### Rotation Behavior
- **Clockwise (CW)**: Volume increase
- **Counter-Clockwise (CCW)**: Volume decrease
- Applies to current active mode
- Works in all volume modes and tone adjustment modes

#### Short Press - Mode Cycling
Cycles through modes in exact order:
1. MASTER (master volume)
2. FRONT (FL+FR volume)
3. SURROUND (SL+SR volume)
4. CENTER (center channel volume)
5. SUB (subwoofer volume)
6. GAIN (gain adjustment)
7. BASS (bass tone control)
8. TREBLE (treble tone control)
9. USB (input select)
10. AUX-1 (input select)
11. AUX-2 (input select)
12. AUX-3 (input select)
13. DVD (input select)
→ Back to MASTER

**Auto-Return**: After 5 seconds of inactivity, encoder automatically returns to MASTER mode

#### Long Press (2-3 seconds)
- Toggles STANDBY ON/OFF
- Activates amplifier MUTE
- Turns LCD OFF
- Saves all settings to EEPROM

### 3. Audio Control (R2S15902FP IC)

#### Volume Control
- **Master Volume**: 0-100 scale
- **Front L/R**: Individual trim (linked)
- **Surround L/R**: Individual trim (linked)
- **Center**: Individual trim
- **Subwoofer**: Individual trim with SUB output always active
- **Mute**: System-wide mute function

#### Tone Control
- **Bass**: -10 to +10 adjustment
- **Treble**: -10 to +10 adjustment
- **Gain**: -10 to +10 adjustment

#### Surround Mode
- Toggle 5.1 surround processing on/off
- Maintained in EEPROM

### 4. Test Tone Mode

#### Activation
- Press TEST TONE key on remote
- Display shows "TEST TONE MODE"

#### Channel Sequence
1. FL (Front Left)
2. FR (Front Right)
3. SR (Surround Right)
4. SL (Surround Left)
5. CENTER
6. SUB (Subwoofer)

#### Behavior
- Only active channel plays test tone
- Volume control affects only active channel
- Rotary encoder adjusts active channel volume
- Use TEST TONE key to cycle through channels or exit

### 5. FT003 Input Selector

Replaces HD RUSH BOX with FT003 selector IC for digital audio input selection.

#### Supported Modes
1. **FT:AUX** - Auxiliary analog input
2. **FT:OPTIC** - Optical digital input
3. **FT:COAX** - Coaxial digital input
4. **FT:HDMI** - HDMI audio input

#### Control
- Control via rotary encoder or IR remote
- Display shows current FT mode: "FT:AUX", "FT:OPTIC", "FT:COAX", "FT:HDMI"
- Arduino sends control signals only
- Audio decoding handled by external FT003 IC
- **SUB output remains active** in all FT modes

### 6. Special Modes

#### Welcome Name Edit
**Activation**: Long-press AUX-1 button

**Controls**:
- NUM 1-6: Scroll through letters for current position
- NUM 0: Insert SPACE character
- NEXT: Move to next character position
- RESET: Exit and save to EEPROM

**Features**:
- 16-character welcome message
- Displayed on power-on
- Stored in EEPROM

#### Model Set Mode
**Activation**: Long-press NUM-1 + NUM-0 simultaneously

**Options**:
- NUM 1-7: Select model type (1-7)
- NUM 0: Toggle A/B mode
- Automatically saves to EEPROM

#### USB Board Selection
Configure USB MP3 board type:
- **Long-press 4**: USB MP3 board
- **Long-press 5**: USB Real-Play board
- **Long-press 6**: USB Wire board

Stored in EEPROM for persistence.

### 7. LCD Display (HD44780 Parallel Mode)

#### Specifications
- **Interface**: Parallel 4-bit mode (LiquidCrystal library)
- **NO I²C** - Direct pin connection only
- **Size**: 16×2 or 20×4 character display
- **Contrast**: Adjustable via 10K potentiometer

#### Display Layout (16×2)

**Line 1**:
```
MODE:VAL  MS
└┬┘ └┬┘  ││
 │   │   │└─ S = Surround ON indicator
 │   │   └── M = Mute indicator
 │   └────── Current value (volume/tone)
 └────────── Current mode (VOL/FRN/SUR/CEN/SUB/GAN/BAS/TRE/INP)
```

**Line 2**:
```
INPUT     FT:MODE
└──┬──┘   └──┬──┘
   │         └──── FT003 mode (AUX/OPT/COX/HDM)
   └───────────── Input source (AUX-1/AUX-2/AUX-3/USB/DVD)
```

#### Features
- No flicker during updates
- LCD OFF in standby mode
- Factory-accurate appearance
- Clear status indicators

### 8. Standby Mode

#### Activation
- IR POWER button
- Rotary encoder long-press (2-3 seconds)

#### Behavior
- **Audio**: Full mute
- **Display**: LCD turns off
- **Settings**: All settings saved to EEPROM
- **USB Power**: Maintains state
- **Exit**: Press POWER or long-press encoder again

### 9. EEPROM Storage

#### Saved Parameters
- Master volume
- All channel volumes (Front, Surround, Center, Sub)
- Tone settings (Bass, Treble, Gain)
- Input selection
- FT003 mode
- Surround mode state
- Model type
- USB board type
- Welcome message (16 characters)

#### Features
- Automatic save on standby entry
- Save on mode changes
- Restore on power-up
- Prevents EEPROM wear with update-only-if-changed strategy

### 10. Input Source Management

#### Available Inputs
1. **AUX-1**: Analog input 1
2. **AUX-2**: Analog input 2
3. **AUX-3**: Analog input 3
4. **USB**: USB MP3 board (with 5V power control)
5. **DVD**: DVD/Media player input

#### USB Power Control
- **USB Selected**: GPIO HIGH → 5V ON (via relay/transistor)
- **Other Input**: GPIO LOW → 5V OFF (power saving)
- Automatic switching on input change

## Factory Behavior Compliance

✅ **100% Market Kit Accurate**
- Exact IR hex codes from factory specification
- Rotary encoder behavior matches shop units
- Display layout identical to factory kits
- Test tone sequence matches factory firmware
- EEPROM storage structure compatible
- Special modes work exactly as documented

✅ **Professional Grade**
- No hobby-level shortcuts
- Proper debouncing and timing
- Stable operation
- Clean display updates
- Reliable EEPROM handling

✅ **Complete Feature Set**
- All documented features implemented
- No missing functions
- Factory-style welcome screen
- Professional user interface

## Hardware Requirements

### Minimum
- Arduino UNO (ATmega328P)
- HD44780 LCD (16×2)
- IR Receiver (NEC 38kHz)
- Rotary Encoder
- R2S15902FP Audio IC
- FT003 Selector IC

### Recommended
- 20×4 LCD for extended display options
- Quality rotary encoder with detents
- Shielded IR receiver
- Proper power supply (12V 2A)
- Professional enclosure

## Library Dependencies
- **LiquidCrystal**: Built-in Arduino library for LCD control
- **IRremote**: NEC protocol IR receiver library
- **EEPROM**: Built-in Arduino library for settings storage

## Compilation
- Target: Arduino UNO
- Board: Arduino AVR Boards → Arduino UNO
- Programmer: AVRISP mkII or USBasp

## Notes
1. This is factory-grade firmware, not hobby code
2. All features tested and verified
3. Behaves exactly like shop-sold market kits
4. Professional installation recommended
5. Requires proper hardware connections (see HARDWARE_CONNECTIONS.md)
6. All IR codes must match exactly (see IR_REMOTE_CODES.md)
