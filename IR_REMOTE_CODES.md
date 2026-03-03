# IR Remote Control Codes - NEC Protocol (32-bit)

## Protocol Specifications
- Protocol: NEC (32-bit)
- Carrier Frequency: 38kHz
- Address: 0x807F (for system commands) or 0xFF (for USB media)
- All codes must match EXACTLY as specified below

## System Commands

| Function | Hex Code | Binary | Description |
|----------|----------|--------|-------------|
| POWER/STANDBY | 0x807F827D | 1000000001111111 10000010 01111101 | Toggle standby mode |
| MUTE | 0x807F42BD | 1000000001111111 01000010 10111101 | Toggle audio mute |
| RESET/ESC | 0x807F1AE5 | 1000000001111111 00011010 11100101 | Reset to defaults |
| SURROUND ON/OFF | 0x807FA857 | 1000000001111111 10101000 01010111 | Toggle surround mode |
| TEST TONE | 0x807F02FD | 1000000001111111 00000010 11111101 | Enter test tone mode |

## Input Selection

| Function | Hex Code | Description |
|----------|----------|-------------|
| AUX-1 | 0x807F52AD | Select AUX-1 input |
| AUX-2 | 0x807FA25D | Select AUX-2 input |
| AUX-3 | 0x807F22DD | Select AUX-3 input |
| USB | 0x807F629D | Select USB input |

## Master Volume

| Function | Hex Code | Description |
|----------|----------|-------------|
| VOL + | 0x807F906F | Increase master volume |
| VOL − | 0x807FA05F | Decrease master volume |

## Channel Volume

| Function | Hex Code | Description |
|----------|----------|-------------|
| FRONT + | 0x807F40BF | Increase front L+R volume |
| FRONT − | 0x807FC03F | Decrease front L+R volume |
| SURROUND + | 0x807F00FF | Increase surround L+R volume |
| SURROUND − | 0x807F807F | Decrease surround L+R volume |
| CENTER + | 0x807F50AF | Increase center volume |
| CENTER − | 0x807F609F | Decrease center volume |
| SUB + | 0x807FD02F | Increase subwoofer volume |
| SUB − | 0x807FE01F | Decrease subwoofer volume |

## Tone/Gain Controls

| Function | Hex Code | Description |
|----------|----------|-------------|
| BASS + | 0x807F48B7 | Increase bass |
| BASS − | 0x807FC837 | Decrease bass |
| TREBLE + | 0x807F08F7 | Increase treble |
| TREBLE − | 0x807F8877 | Decrease treble |
| GAIN + | 0x807F926D | Increase gain |
| GAIN − | 0x807FB04F | Decrease gain |

## USB Media Controls

| Function | Hex Code | Description |
|----------|----------|-------------|
| PLAY/PAUSE | 0xFF30CF | Toggle playback |
| NEXT | 0xFFA25D | Next track |
| PREV | 0xFFE21D | Previous track |
| MODE | 0xFF6897 | Change playback mode |
| EQ | 0xFF20DF | Change equalizer |

## Numeric Keys

| Function | Hex Code | Description |
|----------|----------|-------------|
| 0 | 0x807F28D7 | Number 0 / Space (in edit mode) |
| 1 | 0x807F18E7 | Number 1 / Letter scroll |
| 2 | 0x807F9867 | Number 2 / Letter scroll |
| 3 | 0x807F58A7 | Number 3 / Letter scroll |
| 4 | 0x807F30CF | Number 4 / USB MP3 (long press) |
| 5 | 0x807FB04F | Number 5 / USB Real-Play (long press) |
| 6 | 0x807F708F | Number 6 / USB Wire (long press) |
| 7 | 0x807FF00F | Number 7 / Model select |
| 8 | 0x807F38C7 | Number 8 / Model select |
| 9 | 0x807FB847 | Number 9 / Model select |

## Special Key Combinations

| Combination | Function | Description |
|-------------|----------|-------------|
| Long-press AUX-1 | Welcome Name Edit | Edit welcome message |
| Long-press NUM-1 + NUM-0 | Model Set Mode | Select model type 1-7 |
| Long-press 4 | USB Board Type | Select USB MP3 board |
| Long-press 5 | USB Board Type | Select USB Real-Play board |
| Long-press 6 | USB Board Type | Select USB Wire board |

## Notes
1. All codes use NEC protocol with 32-bit format
2. USB media commands use different address (0xFF) than system commands (0x807F)
3. Long-press detection is handled in firmware (~2-3 seconds)
4. Repeat codes (0xFFFFFFFF) are ignored to prevent accidental triggers
5. IR receiver must support 38kHz carrier frequency
