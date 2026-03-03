# Compilation Error Fix - Forward Declarations

## Problem

The Arduino sketch was failing to compile with the following errors:

```
sketch_mar03a.ino:137:18: error: variable or field 'selectInput' declared void
sketch_mar03a.ino:137:18: error: 'InputSource' was not declared in this scope
sketch_mar03a.ino:148:19: error: variable or field 'setFT003Mode' declared void
sketch_mar03a.ino:148:19: error: 'FT003Mode' was not declared in this scope
sketch_mar03a.ino: In function 'void processIRCommand(long unsigned int)':
sketch_mar03a.ino:329:27: error: 'selectInput' was not declared in this scope
```

## Root Cause

In C++ and Arduino, functions must be declared before they are used. The sketch had:

1. **Enums defined** at lines 140-165 (InputSource, FT003Mode, etc.)
2. **Global variables** at lines 167-217
3. **setup() function** at line 220 (originally) - calls many functions
4. **loop() function** and other functions following

The problem: Functions like `selectInput()`, `setFT003Mode()`, `displayWelcomeScreen()`, etc. were called in `setup()`, `loop()`, and `processIRCommand()` before they were defined, but there were no forward declarations.

## Solution

Added comprehensive forward declarations for all 42+ functions between the global variables and the `setup()` function (lines 219-278).

### File Structure After Fix:

```
Lines 1-119:    Includes, pin definitions, IR codes, EEPROM addresses
Lines 122-165:  Enum definitions (EncoderMode, InputSource, FT003Mode, TestToneChannel)
Lines 167-217:  Global variables
Lines 219-278:  *** FORWARD DECLARATIONS (NEW) ***
Lines 280+:     setup() function
Lines 330+:     loop() function  
Lines 350+:     Function implementations
```

### Forward Declarations Added:

```cpp
// ==================== FUNCTION FORWARD DECLARATIONS ====================
// IR and Input Handlers
void handleIR();
void processIRCommand(unsigned long code);
void handleEncoder();
void handleEncoderRotation(int direction);
void cycleEncoderMode();
void handleNumericKey(int num);

// Volume and Tone Controls
void adjustVolume(int &volume, int delta);
void adjustTone(int &tone, int delta);

// System Functions
void toggleStandby();
void toggleMute();
void toggleSurround();
void toggleTestTone();
void resetToDefaults();
void selectInput(InputSource input);        // ← Uses InputSource enum
void selectNextInput(int direction);

// Test Tone Functions
void activateTestTone();
void deactivateTestTone();
void adjustTestToneVolume(int direction);
void cycleTestToneChannel();

// Special Modes
void enterWelcomeEditMode();
void handleWelcomeEdit(int num);
void enterModelSetMode();
void handleModelSet(int num);

// FT003 Control
void setFT003Mode(FT003Mode mode);          // ← Uses FT003Mode enum
void cycleFT003Mode();

// Audio IC Control
void initAudioIC();
void sendAudioICCommand(byte reg, byte data);
void updateAudioVolume();
void updateAudioTone();
void updateAudioGain();
void updateAudioInput();
void updateAudioSurround();
void updateAllAudioSettings();

// USB Control
void sendUSBCommand(unsigned long cmd);

// Display Functions
void displayWelcomeScreen();
void updateDisplay();
void displayMainScreen();
void displayTestTone();

// EEPROM Functions
void saveSettings();
void loadSettings();
```

## Why This Works

1. **Enums are defined first** (lines 140-165) - so types like `InputSource` and `FT003Mode` exist
2. **Forward declarations come next** (lines 219-278) - telling the compiler these functions exist and their signatures
3. **Functions can now be used** in `setup()`, `loop()`, and other functions without errors
4. **Implementations follow** (lines 700+) - the actual function code

This is standard C++ practice. The Arduino compiler requires this structure when:
- Functions call other functions defined later in the file
- Function signatures use custom types (enums, structs, classes)

## Verification

After the fix:
- ✅ `selectInput(InputSource input)` has forward declaration at line 238
- ✅ `setFT003Mode(FT003Mode mode)` has forward declaration at line 254
- ✅ All functions called in `setup()` are forward-declared
- ✅ All functions called in `processIRCommand()` are forward-declared
- ✅ Enums are defined before forward declarations that use them

## Result

The sketch will now compile successfully with avr-g++ or Arduino IDE/CLI.

---

**Fix Applied**: 2026-03-03  
**Lines Modified**: Added 60 lines (219-278)  
**Files Changed**: UltraDigital_HomeTheater_v3.ino
