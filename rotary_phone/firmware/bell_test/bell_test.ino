#include "Arduino.h"

# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

const unsigned long RING_ON_MS = 2000;
const unsigned long RING_OFF_MS = 4000;
const unsigned long BELL_HALF_PERIOD_MS = 20;

void bell_off() {
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  pinMode(RING_PIN_A, OUTPUT);
  pinMode(RING_PIN_B, OUTPUT);
  bell_off();

  Serial.begin(9600);
  Serial.println("");
  Serial.println("Bell test: 2s ring, 4s pause, forever.");
}

void loop() {
  Serial.println("Ring");
  unsigned long burst_start = millis();
  bool polarity = false;
  while (millis() - burst_start < RING_ON_MS) {
    polarity = !polarity;
    digitalWrite(RING_PIN_A, polarity ? HIGH : LOW);
    digitalWrite(RING_PIN_B, polarity ? LOW : HIGH);
    delay(BELL_HALF_PERIOD_MS);
  }
  bell_off();
  Serial.println("Pause");
  delay(RING_OFF_MS);
}
