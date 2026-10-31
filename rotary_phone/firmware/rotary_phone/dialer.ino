String wait_for_dialed_number() {
  String number = "";
  unsigned int pulses = 0;
  int last_pulse_level = HIGH;
  unsigned long started = millis();
  unsigned long last_pulse_at = started;

  Serial.println("Ready for dialing...");
  while (hook_lifted && millis() - started < DIAL_TIMEOUT_MS) {
    poll();
    if (digitalRead(DIALING_PIN) == LOW) {
      int level = digitalRead(NUMBER_PIN);
      if (level == HIGH && last_pulse_level == LOW) {
        pulses++;
        last_pulse_at = millis();
      }
      last_pulse_level = level;
    }

    unsigned long quiet_ms = millis() - last_pulse_at;
    if (pulses > 0 && quiet_ms > DIGIT_GAP_MS) {
      number += pulses < 10 ? String(pulses) : "0";
      pulses = 0;
      Serial.println("Dialed so far: " + number);
    }
    if (number != "" && pulses == 0 && quiet_ms > NUMBER_DONE_MS) {
      Serial.println("FINAL: " + number);
      report("dialed:" + number);
      return number;
    }
    delay(15);
  }
  return number;
}
