# Testing & Verification Plan

## Pre-Upload Checklist

### Arduino IDE Setup
- [ ] Arduino IDE installed (v1.8.19+)
- [ ] IRremote library installed
- [ ] Board set to "Arduino UNO"
- [ ] Correct COM port selected
- [ ] Code compiles without errors

### Hardware Assembly
- [ ] All components mounted properly
- [ ] LCD connected (parallel, not I²C)
- [ ] IR receiver oriented correctly
- [ ] Rotary encoder connections verified
- [ ] Power supply stable (12V 2A min)
- [ ] No short circuits detected

## Basic Functionality Tests

### Test 1: Power-On Sequence
**Objective**: Verify system boots correctly

**Steps**:
1. Connect USB cable to Arduino
2. Power on the system
3. Observe LCD display

**Expected Results**:
- [ ] LCD backlight turns on
- [ ] Welcome message appears: "ULTRA DIGITAL"
- [ ] Second line shows: "Ver 3.0"
- [ ] Welcome screen displays for 2 seconds
- [ ] Main screen appears

**Pass Criteria**: All items checked

---

### Test 2: LCD Display Verification
**Objective**: Verify LCD shows correct information

**Steps**:
1. System in normal mode
2. Observe main display

**Expected Results**:
- [ ] Line 1 shows: "VOL: 50" (or current volume)
- [ ] Line 2 shows: "AUX-1    FT:AUX"
- [ ] Display is stable (no flickering)
- [ ] Characters are clear and readable
- [ ] Contrast is appropriate

**Pass Criteria**: All items checked

---

### Test 3: IR Remote - Power Control
**Objective**: Test POWER button functionality

**Steps**:
1. Point remote at IR receiver
2. Press POWER button (code: 0x807F827D)
3. Wait 1 second
4. Press POWER again

**Expected Results**:
- [ ] First press: Display turns off (standby mode)
- [ ] Second press: Display turns on
- [ ] Settings are preserved

**Pass Criteria**: All items checked

---

### Test 4: IR Remote - Volume Control
**Objective**: Test master volume adjustment

**Steps**:
1. Note current volume on display
2. Press VOL+ button 5 times (code: 0x807F906F)
3. Press VOL− button 3 times (code: 0x807FA05F)

**Expected Results**:
- [ ] Display updates after each press
- [ ] Volume increases by 5 (to 55 if starting at 50)
- [ ] Volume decreases by 3 (to 52)
- [ ] Volume value shown correctly on LCD

**Pass Criteria**: All items checked

---

### Test 5: IR Remote - Mute Function
**Objective**: Test mute functionality

**Steps**:
1. Press MUTE button (code: 0x807F42BD)
2. Check display for 'M' indicator
3. Press MUTE again

**Expected Results**:
- [ ] First press: 'M' appears on display
- [ ] Audio is muted (if speakers connected)
- [ ] Second press: 'M' disappears
- [ ] Audio restored

**Pass Criteria**: All items checked

---

### Test 6: Rotary Encoder - Rotation
**Objective**: Test encoder volume control

**Steps**:
1. Note current volume
2. Rotate encoder clockwise 5 clicks
3. Rotate encoder counter-clockwise 3 clicks

**Expected Results**:
- [ ] CW rotation increases volume by 5
- [ ] Display updates smoothly
- [ ] CCW rotation decreases volume by 3
- [ ] No false triggers or jitter

**Pass Criteria**: All items checked

---

### Test 7: Rotary Encoder - Short Press
**Objective**: Test mode cycling

**Steps**:
1. Short-press encoder button
2. Observe display change
3. Continue pressing to cycle through all modes
4. Count total modes

**Expected Results**:
- [ ] Mode changes on each press
- [ ] Modes cycle: VOL→FRN→SUR→CEN→SUB→GAN→BAS→TRE→INP→...
- [ ] Display shows correct mode abbreviation
- [ ] Total 13 modes in cycle

**Pass Criteria**: All items checked

---

### Test 8: Rotary Encoder - Long Press
**Objective**: Test standby activation

**Steps**:
1. Press and hold encoder button
2. Hold for 3 seconds
3. Release

**Expected Results**:
- [ ] Display turns off after 2-3 seconds
- [ ] System enters standby
- [ ] Long press again restores display

**Pass Criteria**: All items checked

---

### Test 9: Encoder Auto-Return
**Objective**: Test auto-return to MASTER mode

**Steps**:
1. Short-press encoder to change to FRONT mode
2. Wait 5 seconds without touching anything
3. Observe display

**Expected Results**:
- [ ] Display shows "FRN" initially
- [ ] After 5 seconds, display changes to "VOL"
- [ ] Auto-return successful

**Pass Criteria**: All items checked

---

### Test 10: Input Selection
**Objective**: Test input switching

**Steps**:
1. Press AUX-1 button (0x807F52AD)
2. Press AUX-2 button (0x807FA25D)
3. Press AUX-3 button (0x807F22DD)
4. Press USB button (0x807F629D)

**Expected Results**:
- [ ] Display updates to show each input
- [ ] "AUX-1", "AUX-2", "AUX-3", "USB" shown correctly
- [ ] USB power pin goes HIGH when USB selected
- [ ] USB power pin goes LOW for other inputs

**Pass Criteria**: All items checked

---

### Test 11: Channel Volume Control
**Objective**: Test individual channel trims

**Steps**:
1. Press FRONT+ button (0x807F40BF) 3 times
2. Press SURR+ button (0x807F00FF) 2 times
3. Press CENTER+ button (0x807F50AF) 1 time
4. Press SUB+ button (0x807FD02F) 4 times

**Expected Results**:
- [ ] Each channel adjusts independently
- [ ] Values stored correctly
- [ ] No interference between channels

**Pass Criteria**: All items checked

---

### Test 12: Tone Controls
**Objective**: Test bass, treble, and gain

**Steps**:
1. Press BASS+ button (0x807F48B7) 3 times
2. Press BASS− button (0x807FC837) 1 time
3. Press TREBLE+ button (0x807F08F7) 2 times
4. Press GAIN+ button (0x807F926D) 1 time

**Expected Results**:
- [ ] Each tone control adjusts independently
- [ ] Values in range -10 to +10
- [ ] Display reflects changes (when in tone mode)

**Pass Criteria**: All items checked

---

### Test 13: Test Tone Mode
**Objective**: Test test tone sequence

**Steps**:
1. Press TEST TONE button (0x807F02FD)
2. Observe display
3. Press TEST TONE again to cycle channels
4. Exit with TEST TONE

**Expected Results**:
- [ ] Display shows "TEST TONE MODE"
- [ ] Shows channel: FL→FR→SR→SL→CENTER→SUB
- [ ] Encoder adjusts active channel only
- [ ] Exit returns to normal mode

**Pass Criteria**: All items checked

---

### Test 14: Surround Mode Toggle
**Objective**: Test 5.1 surround enable/disable

**Steps**:
1. Press SURROUND button (0x807FA857)
2. Check for 'S' indicator
3. Press SURROUND again

**Expected Results**:
- [ ] First press: 'S' indicator appears/disappears
- [ ] Second press: Toggles back
- [ ] State saved to EEPROM

**Pass Criteria**: All items checked

---

### Test 15: EEPROM Storage
**Objective**: Verify settings persistence

**Steps**:
1. Change volume to 75
2. Change bass to +5
3. Select AUX-2 input
4. Enter standby (long press encoder or POWER)
5. Power off Arduino completely
6. Power on Arduino

**Expected Results**:
- [ ] Volume restored to 75
- [ ] Bass restored to +5
- [ ] Input restored to AUX-2
- [ ] All settings preserved

**Pass Criteria**: All items checked

---

### Test 16: Reset Function
**Objective**: Test reset to defaults

**Steps**:
1. Change several settings
2. Press RESET button (0x807F1AE5)
3. Observe changes

**Expected Results**:
- [ ] Display shows "RESET TO DEFAULT"
- [ ] Volume returns to 50
- [ ] Bass/Treble/Gain return to 0
- [ ] Input returns to AUX-1

**Pass Criteria**: All items checked

---

### Test 17: FT003 Input Selector
**Objective**: Test FT003 control pins

**Steps**:
1. Measure FT003_A and FT003_B pins
2. Cycle through FT modes (if implemented in UI)
3. Verify pin states

**Expected Results**:
- [ ] FT:AUX: A=LOW, B=LOW
- [ ] FT:OPTIC: A=LOW, B=HIGH
- [ ] FT:COAX: A=HIGH, B=LOW
- [ ] FT:HDMI: A=HIGH, B=HIGH

**Pass Criteria**: All items checked

---

### Test 18: USB Media Controls
**Objective**: Test USB board commands

**Steps**:
1. Press PLAY/PAUSE (0xFF30CF)
2. Press NEXT (0xFFA25D)
3. Press PREV (0xFFE21D)

**Expected Results**:
- [ ] Serial monitor shows USB commands
- [ ] Commands formatted correctly
- [ ] Different prefix (0xFF) recognized

**Pass Criteria**: All items checked

---

### Test 19: Numeric Keys
**Objective**: Test numeric keypad

**Steps**:
1. Press NUM 0 through NUM 9
2. Check serial monitor output

**Expected Results**:
- [ ] All 10 numeric keys recognized
- [ ] Correct hex codes received
- [ ] Serial shows key number

**Pass Criteria**: All items checked

---

### Test 20: Display Update Rate
**Objective**: Verify display refresh

**Steps**:
1. Rapidly rotate encoder
2. Observe display updates
3. Check for flickering or lag

**Expected Results**:
- [ ] Display updates smoothly
- [ ] No flickering
- [ ] Max 100ms update interval
- [ ] Stable during rapid changes

**Pass Criteria**: All items checked

---

## Advanced Tests

### Test 21: Welcome Name Edit Mode
**Objective**: Test welcome message editing

**Steps**:
1. Long-press AUX-1 button
2. Use NUM keys to scroll letters
3. Use NEXT to move cursor
4. Press RESET to save

**Expected Results**:
- [ ] Edit mode activates
- [ ] NUM 1-6 scroll letters
- [ ] NUM 0 inserts space
- [ ] NEXT moves cursor
- [ ] RESET saves and exits
- [ ] New welcome shown on next boot

**Pass Criteria**: All items checked (feature implemented)

---

### Test 22: Model Set Mode
**Objective**: Test model selection

**Steps**:
1. Long-press NUM-1 + NUM-0
2. Press NUM 1-7 to select model
3. Press NUM 0 for A/B mode

**Expected Results**:
- [ ] Mode activates with simultaneous press
- [ ] NUM 1-7 select models
- [ ] NUM 0 toggles A/B
- [ ] Setting saved to EEPROM

**Pass Criteria**: All items checked (feature implemented)

---

### Test 23: USB Board Selection
**Objective**: Test USB board type setting

**Steps**:
1. Long-press NUM 4 (USB MP3)
2. Long-press NUM 5 (USB Real-Play)
3. Long-press NUM 6 (USB Wire)

**Expected Results**:
- [ ] Long press detection works
- [ ] Board type saved
- [ ] Setting persists after reboot

**Pass Criteria**: All items checked (feature implemented)

---

### Test 24: Memory Usage
**Objective**: Verify resource usage

**Steps**:
1. Compile code in Arduino IDE
2. Note memory usage in output

**Expected Results**:
- [ ] Program storage < 30KB (93% max)
- [ ] Global variables < 1.8KB (88% max)
- [ ] No "low memory" warnings during operation
- [ ] System stable for extended periods

**Pass Criteria**: All items checked

---

### Test 25: Long-Duration Stability
**Objective**: Test extended operation

**Steps**:
1. Power on system
2. Let run for 4+ hours
3. Test all functions periodically

**Expected Results**:
- [ ] No crashes or hangs
- [ ] Display remains stable
- [ ] All functions work after hours of operation
- [ ] No memory leaks evident

**Pass Criteria**: All items checked

---

## Test Results Summary

| Test # | Test Name | Pass/Fail | Notes |
|--------|-----------|-----------|-------|
| 1 | Power-On Sequence | ⬜ | |
| 2 | LCD Display | ⬜ | |
| 3 | IR Power | ⬜ | |
| 4 | IR Volume | ⬜ | |
| 5 | IR Mute | ⬜ | |
| 6 | Encoder Rotation | ⬜ | |
| 7 | Encoder Short Press | ⬜ | |
| 8 | Encoder Long Press | ⬜ | |
| 9 | Auto-Return | ⬜ | |
| 10 | Input Selection | ⬜ | |
| 11 | Channel Volume | ⬜ | |
| 12 | Tone Controls | ⬜ | |
| 13 | Test Tone Mode | ⬜ | |
| 14 | Surround Toggle | ⬜ | |
| 15 | EEPROM Storage | ⬜ | |
| 16 | Reset Function | ⬜ | |
| 17 | FT003 Control | ⬜ | |
| 18 | USB Media | ⬜ | |
| 19 | Numeric Keys | ⬜ | |
| 20 | Display Rate | ⬜ | |
| 21 | Welcome Edit | ⬜ | |
| 22 | Model Set | ⬜ | |
| 23 | USB Board Select | ⬜ | |
| 24 | Memory Usage | ⬜ | |
| 25 | Long Duration | ⬜ | |

**Total Tests**: 25  
**Passed**: ___  
**Failed**: ___  
**Completion**: ___%

---

## Debugging Tools

### Serial Monitor Commands
Open Serial Monitor (115200 baud) to see:
- Boot message: "Ultra Digital 5.1 Home Theater v3.0"
- IR codes received
- Mode changes
- Debug messages

### LED Indicators
- Arduino onboard LED (pin 13) blinks during SPI communication

### Multimeter Checks
- VCC: 5.0V ±0.2V
- LCD V0 (contrast): 0.5-1.5V
- FT003 pins: 0V or 5V (digital levels)
- USB power pin: 0V or 5V

---

## Common Issues & Solutions

| Issue | Likely Cause | Solution |
|-------|--------------|----------|
| LCD blank | Contrast | Adjust potentiometer |
| LCD garbage | Wrong connection | Check 4-bit mode wiring |
| No IR | Wrong orientation | Check IR receiver pinout |
| Encoder bouncy | Mechanical | Add 0.1µF caps on pins |
| Volume jumps | Power noise | Add bulk capacitor |
| Settings lost | EEPROM issue | Re-upload firmware |
| Display flickers | Update too fast | Increase update interval |

---

## Sign-Off

**Tester Name**: _________________  
**Date**: _________________  
**Firmware Version**: 3.0  
**Hardware Revision**: _________________

**Overall Result**: ⬜ PASS  ⬜ FAIL

**Comments**:
_________________________________________________________________
_________________________________________________________________
_________________________________________________________________

---

**Next Steps**:
- If all tests pass → Ready for production
- If tests fail → Debug and re-test
- Document any anomalies
