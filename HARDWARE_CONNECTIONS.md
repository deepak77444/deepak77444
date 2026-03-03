# Ultra Digital 5.1 Home Theater Remote Kit - Hardware Connections

## Arduino UNO Pin Assignments

### LCD Display (HD44780 - Parallel Mode)
- RS → Digital Pin 12
- EN → Digital Pin 11
- D4 → Digital Pin 5
- D5 → Digital Pin 4
- D6 → Digital Pin 3
- D7 → Digital Pin 2
- VSS → GND
- VDD → 5V
- V0 → 10K potentiometer for contrast
- RW → GND (write mode only)
- A → 5V (with 220Ω resistor)
- K → GND

### IR Receiver (NEC Protocol)
- OUT → Digital Pin 6
- VCC → 5V
- GND → GND

### Rotary Encoder
- CLK → Digital Pin 7
- DT → Digital Pin 8
- SW → Digital Pin 9
- + → 5V
- GND → GND

### FT003 Selector IC
- A → Analog Pin A0 (Digital Output)
- B → Analog Pin A1 (Digital Output)
- SW → Analog Pin A2 (Digital Output)

### USB Board Control
- Power Control → Digital Pin 10
- (Controls USB 5V via relay/transistor)

### R2S15902FP Audio IC (SPI Interface)
- CS → Digital Pin 13
- CLK → Analog Pin A3
- DATA → Analog Pin A4

## Power Supply
- Arduino: 9-12V DC via barrel jack OR 5V via USB
- System Power: 12V DC, 2A minimum
- Use proper voltage regulators for 5V components

## Audio Connections
All audio routing is handled by the R2S15902FP IC. Arduino only controls volume, tone, and input selection via SPI-like interface.

## Important Notes
1. **NO I²C on LCD** - Must use parallel 4-bit mode (LiquidCrystal library)
2. **IR Receiver** - Compatible with TSOP38238 or similar 38kHz NEC receiver
3. **Rotary Encoder** - Use standard incremental encoder (with pull-up resistors if needed)
4. **FT003** - External audio decoder selector (A, B pins control 4 inputs)
5. **USB Control** - Use transistor (2N2222) or small relay for 5V switching

## Testing Procedure
1. Connect LCD first and verify display works
2. Add IR receiver and test IR code reception
3. Add rotary encoder and test rotation/button press
4. Connect audio IC and verify volume control
5. Add FT003 and verify input switching
6. Complete with USB control

## Schematic Reference
```
Arduino UNO
┌─────────────────┐
│                 │
│  D2-D5 ────────┼──→ LCD Data
│  D11-D12 ──────┼──→ LCD Control
│                 │
│  D6 ───────────┼──→ IR Receiver
│                 │
│  D7-D9 ────────┼──→ Rotary Encoder
│                 │
│  D10 ──────────┼──→ USB Power (via transistor)
│                 │
│  D13 ──────────┼──→ Audio IC CS
│  A3-A4 ────────┼──→ Audio IC CLK/DATA
│                 │
│  A0-A2 ────────┼──→ FT003 Control
│                 │
└─────────────────┘
```
