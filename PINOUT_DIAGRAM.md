# Arduino UNO Pin Assignment Diagram

```
                    Ultra Digital 5.1 Home Theater v3.0
                         Arduino UNO Pinout
                              
┌─────────────────────────────────────────────────────────────┐
│                        ARDUINO UNO                          │
│                      (ATmega328P)                           │
│                                                             │
│  DIGITAL PINS                       ANALOG PINS            │
│  ┌──────────┐                      ┌──────────┐           │
│  │          │                      │          │           │
│  │  D0  RX  │──────────────────────│  A0      │────→ FT003_A (Output)
│  │  D1  TX  │──────────────────────│  A1      │────→ FT003_B (Output)
│  │  D2      │────→ LCD_D7          │  A2      │────→ FT003_SW (Output)
│  │  D3  PWM │────→ LCD_D6          │  A3      │────→ Audio IC CLK
│  │  D4      │────→ LCD_D5          │  A4      │────→ Audio IC DATA
│  │  D5  PWM │────→ LCD_D4          │  A5      │ (Reserved)
│  │  D6  PWM │────→ IR Receiver     │          │           │
│  │  D7      │────→ Encoder CLK     └──────────┘           │
│  │  D8      │────→ Encoder DT                             │
│  │  D9  PWM │────→ Encoder SW                             │
│  │  D10 PWM │────→ USB Power Control                      │
│  │  D11 PWM │────→ LCD_EN                                 │
│  │  D12     │────→ LCD_RS                                 │
│  │  D13 LED │────→ Audio IC CS                            │
│  │          │                                              │
│  └──────────┘                                              │
│                                                             │
│  POWER                                                      │
│  ┌──────────┐                                              │
│  │  5V      │────→ +5V to all ICs                         │
│  │  GND     │────→ Ground (common)                        │
│  │  VIN     │────→ 7-12V DC input                         │
│  └──────────┘                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Pin Function Summary

### Digital Pins (0-13)

| Pin | Type | Function | Connected To | Direction |
|-----|------|----------|--------------|-----------|
| D0 | I/O | Reserved | USB Serial RX | - |
| D1 | I/O | Reserved | USB Serial TX | - |
| D2 | I/O | LCD Data 7 | LCD D7 | Output |
| D3 | PWM | LCD Data 6 | LCD D6 | Output |
| D4 | I/O | LCD Data 5 | LCD D5 | Output |
| D5 | PWM | LCD Data 4 | LCD D4 | Output |
| D6 | PWM | IR Receiver | TSOP38238 OUT | Input |
| D7 | I/O | Encoder Clock | Rotary CLK | Input |
| D8 | I/O | Encoder Data | Rotary DT | Input |
| D9 | PWM | Encoder Switch | Rotary SW | Input |
| D10 | PWM | USB Power | Relay/Transistor | Output |
| D11 | PWM | LCD Enable | LCD EN | Output |
| D12 | I/O | LCD Register Sel | LCD RS | Output |
| D13 | LED | Audio IC CS | R2S15902FP CS | Output |

### Analog Pins (A0-A5)

| Pin | Function | Connected To | Direction | Notes |
|-----|----------|--------------|-----------|-------|
| A0 | FT003 Control A | FT003 A | Output | Digital output mode |
| A1 | FT003 Control B | FT003 B | Output | Digital output mode |
| A2 | FT003 Switch | FT003 SW | Output | Digital output mode |
| A3 | Audio IC Clock | R2S15902FP CLK | Output | SPI-like clock |
| A4 | Audio IC Data | R2S15902FP DATA | Output | SPI-like data |
| A5 | Reserved | - | - | Future expansion |

### Power Pins

| Pin | Voltage | Function |
|-----|---------|----------|
| 5V | 5.0V | Regulated 5V output |
| GND | 0V | Ground reference |
| VIN | 7-12V | External power input |
| 3.3V | 3.3V | Not used in this project |
| IOREF | 5V | I/O reference voltage |

## Component Connection Details

### LCD HD44780 (16×2 or 20×4) - 4-bit Parallel Mode

```
LCD Pin → Arduino Pin
─────────────────────
VSS (GND) → GND
VDD (5V)  → 5V
V0 (Contrast) → 10K pot (between 5V and GND)
RS → D12
RW → GND (write only)
EN → D11
D0 → (not connected)
D1 → (not connected)
D2 → (not connected)
D3 → (not connected)
D4 → D5
D5 → D4
D6 → D3
D7 → D2
A (LED+) → 5V (via 220Ω resistor)
K (LED-) → GND
```

**Important**: This is 4-bit mode. Only D4-D7 are used on LCD.

### IR Receiver (TSOP38238 or compatible)

```
IR Receiver → Arduino
──────────────────────
OUT → D6
VCC → 5V
GND → GND
```

**Note**: Pin order varies by model. Check datasheet!  
Common types: TSOP38238, TSOP4838, VS1838B

### Rotary Encoder (Incremental Type)

```
Encoder → Arduino
─────────────────
CLK (A) → D7
DT (B)  → D8
SW      → D9
+       → 5V
GND     → GND
```

**Note**: Internal pull-ups enabled in code (INPUT_PULLUP).  
External 10K pull-ups can be added if encoder is noisy.

### FT003 Selector IC

```
FT003 → Arduino
───────────────
A  → A0 (used as digital output)
B  → A1 (used as digital output)
SW → A2 (used as digital output)
```

**Truth Table**:
```
Mode    | A | B
────────┼───┼───
AUX     | 0 | 0
OPTIC   | 0 | 1
COAX    | 1 | 0
HDMI    | 1 | 1
```

### R2S15902FP Audio IC (SPI-like Interface)

```
R2S15902FP → Arduino
────────────────────
CS   → D13
CLK  → A3 (used as digital output)
DATA → A4 (used as digital output)
VCC  → 5V
GND  → GND
```

**Note**: This is a simplified SPI-like interface.  
Not using hardware SPI (pins 11, 12, 13) to avoid conflicts.

### USB Power Control

```
Arduino → Circuit
─────────────────
D10 → NPN Transistor Base (via 1K resistor)
     OR
D10 → Relay Coil (via transistor driver)

Transistor:
  Collector → USB 5V+
  Emitter   → GND
  Base      → D10 (via 1K)
```

**Recommended Transistor**: 2N2222, BC547, or similar NPN  
**Relay Alternative**: 5V relay with transistor driver

## Power Distribution

```
                   ┌─────────────┐
                   │  12V DC IN  │
                   │   2A MIN    │
                   └──────┬──────┘
                          │
                ┌─────────┴─────────┐
                │                   │
          ┌─────▼─────┐      ┌─────▼─────┐
          │  7805 or  │      │  Arduino  │
          │  LM2596   │      │    VIN    │
          │ 5V Reg    │      │ (7-12V)   │
          └─────┬─────┘      └─────┬─────┘
                │                  │
                │           ┌──────▼──────┐
                │           │  Arduino    │
                │           │  5V Regulator│
                │           └──────┬──────┘
                │                  │
         ┌──────┴──────┬───────────┴────┬─────────┬─────────┐
         │             │                │         │         │
    ┌────▼────┐  ┌────▼────┐     ┌────▼────┐  ┌─▼──┐  ┌──▼──┐
    │   LCD   │  │Audio IC │     │   IR    │  │FT003│ │Encoder│
    │  (5V)   │  │R2S15902 │     │ Receiver│  │(5V) │ │ (5V)│
    └─────────┘  └─────────┘     └─────────┘  └─────┘ └─────┘
```

**Note**: Add 100nF decoupling capacitors near each IC's VCC pin!

## Wiring Tips

### 1. Use Color-Coded Wires
- **Red**: +5V
- **Black**: GND
- **Yellow/Orange**: Signal wires
- **Blue**: Control signals

### 2. Keep Wires Short
- Especially for LCD data lines
- IR receiver signal line
- Encoder connections

### 3. Add Decoupling Capacitors
Place near each IC:
- 100nF ceramic (0.1µF) close to IC
- 10µF electrolytic on power rail

### 4. Shield Sensitive Lines
- IR receiver signal (if in noisy environment)
- Rotary encoder signals (twisted pair)

### 5. Ground Strategy
- Single ground point (star ground)
- Or ground plane on PCB

## PCB Layout Recommendations

```
┌─────────────────────────────────────────┐
│  LCD (Front Panel)                      │
│  ┌──────────────────┐                   │
│  │   16×2 or 20×4   │                   │
│  └────────┬─────────┘                   │
│           │ Ribbon Cable                │
│  ┌────────▼─────────────────────────┐   │
│  │  Arduino UNO                     │   │
│  │  [on main PCB or separate]       │   │
│  └──────────────────────────────────┘   │
│                                          │
│  ┌──────┐  ┌──────┐  ┌──────┐          │
│  │ FT003│  │R2S159│  │ IR   │          │
│  │      │  │02FP  │  │ Recv │          │
│  └──────┘  └──────┘  └──────┘          │
│                                          │
│  ┌──────────────┐                       │
│  │  Encoder     │  (Front Panel)        │
│  │  (Rotary)    │                       │
│  └──────────────┘                       │
└─────────────────────────────────────────┘
```

## Component Checklist

### Essential Components
- [ ] Arduino UNO (ATmega328P)
- [ ] LCD HD44780 (16×2 or 20×4)
- [ ] 10K potentiometer (LCD contrast)
- [ ] IR Receiver TSOP38238 (38kHz)
- [ ] Rotary Encoder (incremental, with switch)
- [ ] R2S15902FP Audio IC
- [ ] FT003 Selector IC
- [ ] NPN Transistor (2N2222) or relay

### Passive Components
- [ ] 10× 100nF ceramic capacitors (decoupling)
- [ ] 2× 10µF electrolytic capacitors (power)
- [ ] 1× 220Ω resistor (LCD backlight)
- [ ] 1× 1KΩ resistor (transistor base)
- [ ] 2× 10KΩ resistor (pull-ups, if needed)

### Power Supply
- [ ] 12V DC adapter (2A minimum)
- [ ] DC barrel jack
- [ ] 5V regulator (if not using Arduino's)
- [ ] Heat sink for regulator (if using 7805)

### Cables & Connectors
- [ ] USB cable (Type B)
- [ ] Jumper wires (male-to-male, male-to-female)
- [ ] Ribbon cable for LCD (optional)
- [ ] Header pins

### Enclosure & Mounting
- [ ] Project enclosure
- [ ] Mounting standoffs for Arduino
- [ ] LCD mounting bracket
- [ ] Front panel for encoder and LCD

## Testing Points

Add test points on PCB for easy debugging:
- **TP1**: +5V
- **TP2**: GND
- **TP3**: IR Receiver output (D6)
- **TP4**: Encoder CLK (D7)
- **TP5**: LCD EN (D11)
- **TP6**: Audio IC CS (D13)
- **TP7**: FT003 A (A0)
- **TP8**: USB Power (D10)

## Safety Notes

⚠️ **Important Safety Information**:

1. **Power Supply**: Always use regulated power supply
2. **Polarity**: Double-check polarity before connecting power
3. **Current**: Ensure power supply can handle total current draw
4. **Heat**: Add heat sink if using linear regulators
5. **ESD**: Handle ICs with anti-static precautions
6. **Connections**: Verify all connections before first power-on
7. **Fuses**: Consider adding fuse on power input (2A fast-blow)

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 3.0 | 2026 | Initial factory-grade release |

---

**Document**: Pin Assignment Diagram  
**Project**: Ultra Digital 5.1 Home Theater Remote Kit  
**Firmware Version**: 3.0  
**Last Updated**: 2026
