#include "Arduino.h"

# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define NUMBER_PIN 13            // pin D7 - dial pulse contact
# define DIALING_PIN 0            // pin D3 - low while the dial is turning
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

const unsigned long DEBOUNCE_MS = 5;

struct Input {
  uint8_t pin;
  int stable;
  int candidate;
  unsigned long changed_at;
};

Input dialing = {DIALING_PIN, HIGH, HIGH, 0};
Input number = {NUMBER_PIN, LOW, LOW, 0};
unsigned int pulses = 0;
unsigned int stray_pulses = 0;

bool update(Input &in) {
  int level = digitalRead(in.pin);
  if (level != in.candidate) {
    in.candidate = level;
    in.changed_at = millis();
  }
  if (in.candidate != in.stable && millis() - in.changed_at >= DEBOUNCE_MS) {
    in.stable = in.candidate;
    return true;
  }
  return false;
}

void setup() {
  pinMode(RING_PIN_A, OUTPUT);
  pinMode(RING_PIN_B, OUTPUT);
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(NUMBER_PIN, INPUT_PULLUP);
  pinMode(DIALING_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("");
  Serial.println("Dial test: LED on while the dial is off rest.");
  dialing.stable = dialing.candidate = digitalRead(DIALING_PIN);
  number.stable = number.candidate = digitalRead(NUMBER_PIN);
  Serial.print("At rest: off-normal (D3) ");
  Serial.print(dialing.stable == LOW ? "CLOSED" : "open");
  Serial.print(", pulse (D7) ");
  Serial.println(number.stable == LOW ? "closed" : "OPEN");
}

void loop() {
  if (update(dialing)) {
    if (dialing.stable == LOW) {
      Serial.println("Dial moving");
      pulses = 0;
    } else {
      Serial.print("Dial at rest: ");
      Serial.print(pulses);
      Serial.print(" pulses = digit ");
      Serial.println(pulses == 10 ? 0 : pulses);
    }
  }
  if (update(number) && number.stable == HIGH) {
    if (dialing.stable == LOW) pulses++;
    else Serial.println("Stray pulse with dial at rest: " + String(++stray_pulses));
  }
  digitalWrite(STATUS_LED_PIN, dialing.stable);
}
