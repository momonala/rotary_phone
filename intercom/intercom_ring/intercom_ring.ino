#include "Arduino.h"
#include "DFPlayerMini_Fast.h"

const uint8_t DFPLAYER_RX_PIN = 16;  // GPIO16 (RX2) - DFPlayer TX
const uint8_t DFPLAYER_TX_PIN = 17;  // GPIO17 (TX2) - DFPlayer RX, through 1 kOhm
const uint8_t BUTTON_PIN = 0;        // on-board BOOT button, low when pressed
const uint8_t RING_PIN = 23;         // PC817 collector, low while the bell rings
const uint8_t LED_PIN = 2;           // on-board blue LED, on while a ring signal is seen

const uint8_t VOLUME = 30;  // 0 to 30
const uint8_t AUDIO_FOLDER = 1;
const uint8_t BUILDING_TRACKS[] = {6};
const uint8_t APARTMENT_TRACKS[] = {1, 5};
const uint8_t ALL_TRACKS[] = {1, 5, 6};
const uint8_t BOOT_TRACK = 5;
const unsigned long DEBOUNCE_MS = 50;

// The building door's tune has a 740 Hz note (1351 us per cycle); the apartment door's tops out at 600 Hz (1667 us).
const uint32_t BUILDING_MAX_PERIOD_US = 1500;
const uint32_t MIN_PERIOD_US = 1000;
const uint8_t BUILDING_MIN_SHORT_CYCLES = 5;

// The optocoupler pulses with the ring tone and the melody has short pauses, so gaps under SIGNAL_GAP_MS count as one signal.
const unsigned long SIGNAL_GAP_MS = 50;
const unsigned long MIN_RING_MS = 150;
const unsigned long REARM_MS = 3000;

HardwareSerial dfSerial(2);
DFPlayerMini_Fast DFPlayer;

bool buttonWasDown = false;
unsigned long lastButtonChange = 0;

bool inSignal = false;
bool played = false;
unsigned long signalStart = 0;
unsigned long lastSignal = 0;

volatile uint32_t lastFallUs = 0;
volatile uint16_t shortCycles = 0;
volatile uint16_t glitches = 0;

// The optocoupler's slow rising edge can fire extra FALLING interrupts; skip them without moving lastFallUs.
void IRAM_ATTR on_ring_fall() {
  uint32_t now = micros();
  uint32_t period = now - lastFallUs;
  if (period < MIN_PERIOD_US) {
    glitches++;
    return;
  }
  lastFallUs = now;
  if (period < BUILDING_MAX_PERIOD_US) shortCycles++;
}

void setup_audio_player() {
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.print("Initializing DFPlayer");
  while (!DFPlayer.begin(dfSerial, false)) {
    delay(500); Serial.print(".");
  }
  Serial.println(" DFPlayer Mini online!");
  DFPlayer.volume(VOLUME);
}

void play_one_of(const uint8_t* tracks, size_t count) {
  uint8_t track = tracks[esp_random() % count];
  Serial.printf("Playing folder %u track %u\n", AUDIO_FOLDER, track);
  DFPlayer.playFolder(AUDIO_FOLDER, track);
}

bool button_pressed() {
  bool down = digitalRead(BUTTON_PIN) == LOW;
  if (down == buttonWasDown || millis() - lastButtonChange < DEBOUNCE_MS) return false;
  buttonWasDown = down;
  lastButtonChange = millis();
  return down;
}

bool ring_started() {
  unsigned long now = millis();
  if (digitalRead(RING_PIN) == LOW) {
    if (!inSignal) {
      signalStart = now;
      if (!played) {
        shortCycles = 0;
        glitches = 0;
      }
    }
    inSignal = true;
    lastSignal = now;
  } else if (inSignal && now - lastSignal > SIGNAL_GAP_MS) {
    inSignal = false;
  }
  digitalWrite(LED_PIN, inSignal ? HIGH : LOW);

  if (!inSignal && played && now - lastSignal > REARM_MS) {
    played = false;
    Serial.println("Ring over, re-armed.");
  }
  if (inSignal && !played && now - signalStart >= MIN_RING_MS) {
    played = true;
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RING_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(RING_PIN), on_ring_fall, FALLING);
  setup_audio_player();
  Serial.printf("Boot: playing folder %u track %u\n", AUDIO_FOLDER, BOOT_TRACK);
  DFPlayer.playFolder(AUDIO_FOLDER, BOOT_TRACK);
}

void loop() {
  if (ring_started()) {
    uint16_t seen = shortCycles;
    if (seen >= BUILDING_MIN_SHORT_CYCLES) {
      Serial.printf("Building door rang (%u cycles above 667 Hz, %u glitches).\n", seen, glitches);
      play_one_of(BUILDING_TRACKS, sizeof(BUILDING_TRACKS));
    } else {
      Serial.printf("Apartment door rang (%u cycles above 667 Hz, %u glitches).\n", seen, glitches);
      play_one_of(APARTMENT_TRACKS, sizeof(APARTMENT_TRACKS));
    }
  }
  if (button_pressed()) play_one_of(ALL_TRACKS, sizeof(ALL_TRACKS));
}
