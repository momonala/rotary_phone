#include "Arduino.h"

# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define PHONE_HOOK_PIN 12        // pin D6 - low when phone is lifted
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

bool last_raw;

void setup() {
  pinMode(RING_PIN_A, OUTPUT);
  pinMode(RING_PIN_B, OUTPUT);
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(PHONE_HOOK_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("");
  Serial.println("Hook test: LED on while lifted. Every raw change is printed.");
  last_raw = digitalRead(PHONE_HOOK_PIN);
  Serial.println(last_raw == LOW ? "Start: LIFTED" : "Start: ON HOOK");
}

void loop() {
  bool raw = digitalRead(PHONE_HOOK_PIN);
  digitalWrite(STATUS_LED_PIN, raw);
  if (raw != last_raw) {
    last_raw = raw;
    Serial.print(millis());
    Serial.println(raw == LOW ? " ms  LIFTED" : " ms  ON HOOK");
  }
}
