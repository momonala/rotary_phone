
void setup_dialer() {
  pinMode(DIALING_PIN, INPUT_PULLUP);
  pinMode(NUMBER_PIN, INPUT_PULLUP);
  pinMode(PHONE_HOOK_PIN, INPUT_PULLUP);
  Serial.println("Dialer setup.");
}


String wait_for_dialed_number() {
  String final_number = "";
  unsigned long last_updated = millis();
  unsigned int pulse_reading = 0;
  unsigned int last_pulse_reading = 0;
  unsigned int num_pulses = 0;


  Serial.println("Ready for dialing...");
  while (true) {
    // check if we're dialing, if so, sum up all the pulses in the dialing period
    if (digitalRead(DIALING_PIN) == 0) {
      last_pulse_reading = pulse_reading;
      pulse_reading = digitalRead(NUMBER_PIN);
      if ((pulse_reading == 1) && (last_pulse_reading == 0)) {
        num_pulses ++;
        last_updated = millis();
      }
    }

    // if its been more than 2.5s since the last number added, thats the final number
    if ((millis() - last_updated) > 2500) {
      if (final_number != "") {
        Serial.print("FINAL: ");
        Serial.println(final_number);
        return final_number;
        final_number = "";
      }
      last_updated = millis();
    }

    // if its been more than 200ms since the last mumber added, thats the latest number
    if ((millis() - last_updated ) > 200) {
      if (num_pulses > 0) {
        final_number += num_pulses < 10 ? String(num_pulses) : "0"; // append the number, 0 if 10
        Serial.print("--- pulses: ");
        Serial.print(num_pulses);
        Serial.print(" final so far: ");
        Serial.println(final_number);

        num_pulses = 0;
        last_updated = millis();
      }
    }
    delay(15);
  }
}
