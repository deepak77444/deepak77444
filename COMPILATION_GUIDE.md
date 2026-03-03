# Compilation and Upload Guide

## Prerequisites

### Software Requirements
1. **Arduino IDE** (version 1.8.19 or later)
   - Download from: https://www.arduino.cc/en/software
   
2. **Required Libraries** (install via Library Manager):
   - **IRremote** by shirriff (latest version)
   - **LiquidCrystal** (built-in, no installation needed)
   - **EEPROM** (built-in, no installation needed)

### Hardware Requirements
- Arduino UNO board
- USB cable (Type B)
- Properly assembled hardware (see HARDWARE_CONNECTIONS.md)

## Step-by-Step Compilation

### 1. Install Arduino IDE

#### Windows
```powershell
# Download installer from arduino.cc
# Run the installer
# Follow the installation wizard
```

#### macOS
```bash
# Download DMG from arduino.cc
# Drag Arduino to Applications folder
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install arduino
```

### 2. Install IRremote Library

#### Method 1: Library Manager (Recommended)
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "IRremote"
4. Select "IRremote by shirriff"
5. Click **Install**
6. Wait for installation to complete

#### Method 2: Manual Installation
```bash
# Download from: https://github.com/Arduino-IRremote/Arduino-IRremote
cd ~/Arduino/libraries/
git clone https://github.com/Arduino-IRremote/Arduino-IRremote.git IRremote
```

### 3. Verify Libraries

Open Arduino IDE and check:
- **Sketch → Include Library** should show:
  - ✅ IRremote
  - ✅ LiquidCrystal
  - ✅ EEPROM

### 4. Open the Firmware

1. Navigate to the project folder
2. Double-click `UltraDigital_HomeTheater_v3.ino`
3. Arduino IDE should open automatically

### 5. Configure Board Settings

In Arduino IDE:
1. **Tools → Board** → Select **Arduino AVR Boards → Arduino Uno**
2. **Tools → Processor** → Select **ATmega328P**
3. **Tools → Port** → Select your COM port (varies by system):
   - Windows: `COM3`, `COM4`, etc.
   - macOS: `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`

### 6. Verify (Compile) the Code

1. Click the **✓ Verify** button (or press `Ctrl+R` / `Cmd+R`)
2. Wait for compilation to complete
3. Check the output window for:
   ```
   Sketch uses XXXX bytes (XX%) of program storage space. Maximum is 32256 bytes.
   Global variables use XXXX bytes (XX%) of dynamic memory, leaving XXXX bytes for local variables. Maximum is 2048 bytes.
   ```

### 7. Upload to Arduino

1. Connect Arduino UNO via USB cable
2. Verify correct COM port is selected (see step 5)
3. Click the **→ Upload** button (or press `Ctrl+U` / `Cmd+U`)
4. Wait for upload to complete
5. Look for "Done uploading" message

## Expected Compilation Output

### Successful Compilation
```
Sketch uses 20546 bytes (63%) of program storage space. Maximum is 32256 bytes.
Global variables use 1234 bytes (60%) of dynamic memory, leaving 814 bytes for local variables. Maximum is 2048 bytes.
```

### Memory Usage Estimates
- **Flash (Program)**: ~20-24 KB out of 32 KB
- **SRAM (Variables)**: ~1.2-1.5 KB out of 2 KB
- **EEPROM (Settings)**: ~40 bytes out of 1 KB

## Troubleshooting

### Compilation Errors

#### "IRremote.h: No such file or directory"
**Solution**: Install IRremote library (see step 2)

#### "LiquidCrystal.h: No such file or directory"
**Solution**: This should never happen (built-in). Try reinstalling Arduino IDE.

#### "'IRrecv' does not name a type"
**Solution**: Wrong IRremote version. Install "IRremote by shirriff"

#### "Low memory available, stability problems may occur"
**Solution**: Normal for this project. Code is optimized but feature-rich.

### Upload Errors

#### "avrdude: stk500_recv(): programmer is not responding"
**Solutions**:
1. Check USB cable connection
2. Try different USB port
3. Press reset button on Arduino before upload
4. Check if correct COM port is selected
5. Close Serial Monitor if open

#### "Device is not responding"
**Solutions**:
1. Disconnect external circuits during upload
2. Remove shields/modules temporarily
3. Try uploading Arduino Blink example first to verify board
4. Check for short circuits on the board

#### "Permission denied" (Linux/macOS)
**Solution**:
```bash
# Linux
sudo usermod -a -G dialout $USER
# Logout and login again

# macOS
# Check System Preferences → Security & Privacy → Privacy tab
```

### Runtime Issues

#### LCD shows garbage characters
**Solutions**:
1. Adjust contrast potentiometer
2. Check all LCD connections
3. Verify 5V power supply
4. Check for loose wires

#### IR remote not working
**Solutions**:
1. Check IR receiver orientation
2. Verify pin 6 connection
3. Test with known-working remote
4. Check IR codes match specification

#### Encoder not responding
**Solutions**:
1. Check CLK, DT, SW connections
2. Verify pull-up resistors (use INPUT_PULLUP)
3. Test encoder mechanically
4. Check for debouncing issues

## Optimization Tips

### Reduce Memory Usage (if needed)

#### Option 1: Use F() macro for strings
Already implemented in the code with `F("string")` syntax.

#### Option 2: Reduce LCD update frequency
Change in code:
```cpp
#define DISPLAY_UPDATE_INTERVAL 200  // Increase from 100 to 200ms
```

#### Option 3: Disable Serial debugging
Comment out Serial.print statements or reduce debugging:
```cpp
// Comment out or remove:
Serial.begin(9600);
Serial.println(...);
```

### Optimize for Speed

#### Enable compiler optimizations
In Arduino IDE preferences, you can't directly change optimization level, but you can modify platform.txt if needed.

## Verification After Upload

### 1. Basic Checks
- [ ] Arduino powers on (LED lights up)
- [ ] LCD displays welcome message
- [ ] Welcome text shows "ULTRA DIGITAL Ver 3.0"

### 2. IR Remote Test
- [ ] Press POWER button → Should toggle standby
- [ ] Press VOL+ → Master volume increases
- [ ] Press MUTE → Audio mutes
- [ ] Try different buttons → LCD responds

### 3. Rotary Encoder Test
- [ ] Rotate clockwise → Volume increases
- [ ] Rotate counter-clockwise → Volume decreases
- [ ] Short press → Mode cycles
- [ ] Long press (2-3s) → Standby mode

### 4. Display Test
- [ ] Line 1 shows mode and value
- [ ] Line 2 shows input and FT mode
- [ ] Display updates smoothly
- [ ] No flickering

### 5. Special Features Test
- [ ] Press TEST TONE → Enters test mode
- [ ] Long-press AUX-1 → Welcome edit mode
- [ ] Settings save on standby
- [ ] Settings restore on power-up

## Command Line Compilation (Advanced)

### Using arduino-cli

#### Install arduino-cli
```bash
# Linux/macOS
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Windows (PowerShell)
iwr -useb https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | iex
```

#### Setup and Compile
```bash
# Initialize config
arduino-cli config init

# Update core index
arduino-cli core update-index

# Install Arduino AVR core
arduino-cli core install arduino:avr

# Install libraries
arduino-cli lib install "IRremote"

# Compile
arduino-cli compile --fqbn arduino:avr:uno UltraDigital_HomeTheater_v3.ino

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno UltraDigital_HomeTheater_v3.ino
```

## Build Artifacts

After compilation, you'll find these files:
- `build/arduino.avr.uno/UltraDigital_HomeTheater_v3.ino.hex` - Compiled hex file
- `build/arduino.avr.uno/UltraDigital_HomeTheater_v3.ino.elf` - ELF file
- Build folder contains temporary compilation files

These are already in `.gitignore` and won't be committed.

## Final Checklist

Before declaring success:
- [ ] Code compiles without errors
- [ ] Code compiles without warnings (or only acceptable warnings)
- [ ] Upload completes successfully
- [ ] Arduino runs the code after upload
- [ ] Serial monitor shows "Ultra Digital 5.1 Home Theater v3.0"
- [ ] All hardware components respond correctly
- [ ] Settings save and restore properly

## Getting Help

If you encounter issues:
1. Check this guide thoroughly
2. Review HARDWARE_CONNECTIONS.md for wiring
3. Verify all component specifications
4. Test components individually
5. Check Arduino forums for similar issues

## Success!

If all tests pass, your Ultra Digital 5.1 Home Theater Remote Kit is ready for use!

Enjoy your factory-grade home theater control system! 🎉
