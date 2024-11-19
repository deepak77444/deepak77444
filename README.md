- 👋 Hi, I’m @deepak77444
- 👀 I’m interested in ...
- 🌱 I’m currently learning ...
- 💞️ I’m looking to collaborate on ...
- 📫 How to reach me ...
- 😄 Pronouns: ...
- ⚡ Fun fact: ...

<!---
deepak77444/deepak77444 is a ✨ special ✨ repository because its `README.md` (this file) appears on your GitHub profile.
You can click the Preview link to take a look at your changes.
--->
// Auduino: A Lo-Fi Granular Synthesizer
// by Peter Knight, Tinker.it
// More help: http://code.google.com/p/tinkerit/wiki/Auduino

#include <avr/io.h>
#include <avr/interrupt.h>

// Phase accumulators and increments for grains
uint16_t syncPhaseAcc;
uint16_t syncPhaseInc;
uint16_t grainPhaseAcc;
uint16_t grainPhaseInc;
uint16_t grainAmp;
uint8_t grainDecay;
uint16_t grain2PhaseAcc;
uint16_t grain2PhaseInc;
uint16_t grain2Amp;
uint8_t grain2Decay;

// Analog Input Channels
#define SYNC_CONTROL         (A4) // Grain 1 pitch
#define GRAIN_FREQ_CONTROL   (A0) // Grain repetition frequency
#define GRAIN_DECAY_CONTROL  (A2) // Grain 1 decay
#define GRAIN2_FREQ_CONTROL  (A3) // Grain 2 pitch
#define GRAIN2_DECAY_CONTROL (A1) // Grain 2 decay

// Pin Configuration for Arduino Nano
#define PWM_PIN       3
#define PWM_VALUE     OCR2B
#define PWM_INTERRUPT TIMER2_OVF_vect

// Logarithmic mapping table
uint16_t antilogTable[] = {
  64830, 64132, 63441, 62757, 62081, 61413, 60751, 60097,
  59449, 58809, 58176, 57549, 56929, 56316, 55709, 55109,
  54515, 53928, 53347, 52773, 52204, 51642, 51085, 50535,
  49991, 49452, 48920, 48393, 47871, 47356, 46846, 46341,
  45842, 45348, 44859, 44376, 43898, 43425, 42958, 42495,
  42037, 41584, 41136, 40693, 40255, 39821, 39392, 38968,
  38548, 38133, 37722, 37316, 36914, 36516, 36123, 35734,
  35349, 34968, 34591, 34219, 33850, 33486, 33125, 32768
};

// Function to map phase increments using logarithmic scaling
uint16_t mapPhaseInc(uint16_t input) {
  return (antilogTable[input & 0x3f]) >> (input >> 6);
}

// Pentatonic mapping table
uint16_t pentatonicTable[54] = {
  0, 19, 22, 26, 29, 32, 38, 43,
  51, 58, 65, 77, 86, 103, 115, 129,
  154, 173, 206, 231, 259, 308, 346,
  411, 461, 518, 616, 691, 822, 923,
  1036, 1232, 1383, 1644, 1845, 2071,
  2463, 2765, 3288, 3691, 4143, 4927,
  5530, 6577, 7382, 8286, 9854, 11060,
  13153, 14764, 16572, 19708, 22121, 26306
};

// Function to map input to pentatonic scale
uint16_t mapPentatonic(uint16_t input) {
  uint8_t value = (1023 - input) / (1024 / 53);
  return (pentatonicTable[value]);
}

// Function to configure audio output
void audioOn() {
  // Set up PWM for Fast PWM mode, non-inverted output
  TCCR2A = _BV(COM2B1) | _BV(WGM20) | _BV(WGM21);
  TCCR2B = _BV(CS20); // No prescaler
  TIMSK2 = _BV(TOIE2); // Enable Timer2 overflow interrupt
}

// Setup function
void setup() {
  pinMode(PWM_PIN, OUTPUT);
  audioOn();
}

// Main loop
void loop() {
  syncPhaseInc = mapPentatonic(analogRead(SYNC_CONTROL));
  grainPhaseInc  = mapPhaseInc(analogRead (GRAIN_FREQ_CONTROL)) / 2;
  grainDecay     = analogRead(GRAIN_DECAY_CONTROL) / 8;
  grain2PhaseInc = mapPhaseInc(analogRead(GRAIN2_FREQ_CONTROL)) / 2;
  grain2Decay    = analogRead(GRAIN2_DECAY_CONTROL) / 4;
}

// PWM Interrupt Service Routine
SIGNAL(PWM_INTERRUPT) {
  uint8_t value;
  uint16_t output;

  syncPhaseAcc += syncPhaseInc;
  if (syncPhaseAcc < syncPhaseInc) {
    // Start a new grain
    grainPhaseAcc = 0;
    grainAmp = 0x7fff >> grainDecay; // Set grain amplitude based on decay
  }

  // Generate grain output
  grainPhaseAcc += grainPhaseInc;
  if (grainPhaseAcc >= 0xFFFF) {
    grainPhaseAcc -= 0xFFFF;
  }
  output = (grainAmp * (grainPhaseAcc >> 8)) >> 8;

  // Set PWM value
  PWM_VALUE = output;

  // Handle second grain similarly
  grain2PhaseAcc += grain2PhaseInc;
  if (grain2PhaseAcc >= 0xFFFF) {
    grain2PhaseAcc -= 0xFFFF;
  }
  output += (grain2Amp * (grain2PhaseAcc >> 8)) >> 8;

  // Final output to PWM
  PWM_VALUE = output;
}
