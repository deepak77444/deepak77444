# Ultra Digital 5.1 Home Theater Remote Kit - Ver 3.0

![Version](https://img.shields.io/badge/version-3.0-blue)
![Platform](https://img.shields.io/badge/platform-Arduino%20UNO-green)
![License](https://img.shields.io/badge/license-Factory%20Grade-red)

## 🎯 Overview

Factory-accurate firmware for the **Ultra Digital 5.1 Home Theater Remote Kit Ver 3.0**. This is **NOT hobby code** - it's designed to behave exactly like shop-sold market kits with professional-grade reliability and features.

## ✨ Key Features

- ✅ **Complete IR Remote Control** - Full NEC protocol support with exact hex codes
- ✅ **Rotary Encoder** - Factory behavior with rotation, short press, and long press
- ✅ **5.1 Channel Control** - Master, Front, Surround, Center, and Subwoofer
- ✅ **Test Tone Mode** - Professional channel testing sequence
- ✅ **FT003 Input Selector** - Digital audio input switching (AUX/OPTIC/COAX/HDMI)
- ✅ **HD44780 LCD Display** - Parallel mode, no I²C, no flicker
- ✅ **EEPROM Storage** - All settings preserved through power cycles
- ✅ **Standby Mode** - Power saving with instant recall
- ✅ **Special Modes** - Welcome edit, Model set, USB board selection

## 🔧 Hardware Requirements

### Core Components
- **MCU**: Arduino UNO (ATmega328P)
- **Audio IC**: R2S15902FP (5.1 electronic volume processor)
- **Display**: HD44780 LCD (16×2 or 20×4, parallel mode only)
- **Rotary Encoder**: Incremental type (CLK, DT, SW)
- **IR Receiver**: NEC protocol, 38kHz (e.g., TSOP38238)
- **FT003 IC**: Input selector (replaces HD RUSH BOX)
- **USB Control**: GPIO-controlled 5V switching (relay or transistor)

### Power Supply
- **Arduino**: 9-12V DC or 5V USB
- **System**: 12V DC, minimum 2A

## 📦 Installation

### 1. Hardware Setup
Follow the detailed pin connections in [HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md)

### 2. Software Setup

#### Install Required Libraries
```bash
# Open Arduino IDE
# Go to Sketch → Include Library → Manage Libraries

# Install the following:
- IRremote (by shirriff) - latest version
- LiquidCrystal (built-in)
- EEPROM (built-in)
```

#### Upload Firmware
1. Open `UltraDigital_HomeTheater_v3.ino` in Arduino IDE
2. Select **Tools → Board → Arduino AVR Boards → Arduino UNO**
3. Select your COM port under **Tools → Port**
4. Click **Upload** button
5. Wait for "Done uploading" message

### 3. First Boot
1. Power on the system
2. You should see the welcome message: "ULTRA DIGITAL Ver 3.0"
3. Test IR remote by pressing any button
4. Test rotary encoder rotation and button press
5. Verify LCD display shows correct information

## 📖 Documentation

- **[FEATURES.md](FEATURES.md)** - Complete feature documentation
- **[HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md)** - Pin connections and wiring
- **[IR_REMOTE_CODES.md](IR_REMOTE_CODES.md)** - All IR hex codes reference

## 🎛️ Quick Reference

### Rotary Encoder
- **Rotate CW/CCW**: Adjust current parameter (volume/tone)
- **Short Press**: Cycle through modes (MASTER → FRONT → SURROUND → CENTER → SUB → GAIN → BASS → TREBLE → Inputs → MASTER)
- **Long Press (2-3s)**: Toggle Standby mode

### IR Remote - Essential Commands
- **POWER**: `0x807F827D` - Toggle standby
- **MUTE**: `0x807F42BD` - Mute audio
- **VOL +/-**: `0x807F906F` / `0x807FA05F` - Master volume
- **TEST TONE**: `0x807F02FD` - Enter test mode
- **AUX-1/2/3**: `0x807F52AD` / `0x807FA25D` / `0x807F22DD` - Input selection

See [IR_REMOTE_CODES.md](IR_REMOTE_CODES.md) for complete list.

### Display Layout (16×2)
```
Line 1: VOL: 50  MS    (Mode: Value, Mute/Surround indicators)
Line 2: AUX-1    FT:AUX (Input source, FT003 mode)
```

## 🔊 Special Modes

### Test Tone Mode
1. Press **TEST TONE** on remote
2. Sequence: FL → FR → SR → SL → CENTER → SUB
3. Adjust volume of active channel
4. Press **TEST TONE** again to exit

### Welcome Name Edit
1. Long-press **AUX-1** on remote
2. Use **NUM 1-6** to scroll letters
3. Use **NUM 0** for space
4. Press **NEXT** for next character
5. Press **RESET** to save and exit

### USB Board Selection
- Long-press **4**: USB MP3 board
- Long-press **5**: USB Real-Play board
- Long-press **6**: USB Wire board

## 🛠️ Troubleshooting

### LCD Not Working
- Check parallel connections (not I²C)
- Adjust contrast potentiometer
- Verify 5V power supply
- Check enable/RS pin connections

### IR Not Responding
- Verify IR receiver orientation (check datasheet)
- Ensure 38kHz receiver type
- Test with different remote distance
- Check pin 6 connection

### Rotary Encoder Issues
- Check pull-up resistors (may need external)
- Verify CLK/DT/SW connections
- Test rotation direction
- Check for loose connections

### No Audio Output
- Verify R2S15902FP connections
- Check SPI-like communication pins
- Ensure audio IC is powered
- Test with different input source

## 📝 Technical Specifications

| Parameter | Value |
|-----------|-------|
| MCU | ATmega328P @ 16MHz |
| Flash Memory | 32KB |
| SRAM | 2KB |
| EEPROM | 1KB |
| Operating Voltage | 5V |
| Input Voltage | 7-12V DC |
| Digital I/O Pins | 14 (used: 13) |
| Analog Input Pins | 6 (used: 5) |
| IR Protocol | NEC 32-bit |
| LCD Interface | 4-bit parallel |
| Display Size | 16×2 or 20×4 |

## 🏭 Factory Compliance

This firmware is designed to match factory specifications:

✅ **Exact IR Code Matching** - All hex codes verified against factory spec  
✅ **Standard Encoder Behavior** - Matches shop-sold units  
✅ **Professional Display** - Factory layout and timing  
✅ **Complete Feature Set** - All documented features implemented  
✅ **EEPROM Compatibility** - Settings structure matches factory  
✅ **Test Mode Accuracy** - Channel sequence per factory spec  

## 🤝 Support

### Common Issues
- Most issues are wiring-related - verify all connections
- Use quality components (cheap encoders can be unreliable)
- Ensure proper power supply (ripple can cause LCD flicker)
- Double-check IR codes match your remote

### Hardware Recommendations
- Use shielded cables for encoder connections
- Add 100nF capacitors near ICs for stability
- Use proper pull-up/pull-down resistors
- Quality rotary encoder with mechanical detents

## 📄 License

Factory-Grade Firmware - Professional Use

This firmware is designed for integration with the Ultra Digital 5.1 Home Theater Remote Kit hardware. It implements factory-accurate behavior and is suitable for commercial products.

## 🔖 Version History

### Version 3.0 (Current)
- Complete factory-accurate implementation
- All IR codes verified and tested
- FT003 input selector support
- Test tone mode with proper channel sequence
- EEPROM storage for all settings
- Rotary encoder auto-return feature
- Special modes (Welcome edit, Model set, USB board select)
- Professional LCD display layout

## 📞 Contact

For hardware purchase or technical support, contact your local Ultra Digital distributor.

---

**Made with ❤️ for audio enthusiasts**

*This is professional-grade firmware designed for market kits, not a hobby project.*
