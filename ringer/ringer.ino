// h-bridge outputs
# define en1 13
# define en2 12
# define enA 14

# define LISTEN_PIN 5
# define BUILTIN_LED_OVERRIDE 2



void setup() {
  //  Serial.begin(9600);
  pinMode(enA, OUTPUT);
  pinMode(en1, OUTPUT);
  pinMode(en2, OUTPUT);
  pinMode(LISTEN_PIN, INPUT_PULLUP);
}


void phone_ring() {
  unsigned long start_time;
  for (int i = 0; i < 5; i++) {
    start_time = millis();
    while (millis() - start_time < 2000) {
      base_ring();
      if (digitalRead(LISTEN_PIN))
        break;
    }
    //     ring off
    delay(4000);
  }
}

void base_ring() {
  digitalWrite(enA, 255);
  digitalWrite(BUILTIN_LED_OVERRIDE, HIGH);

  // one direction
  digitalWrite(en1, HIGH);
  digitalWrite(en2, LOW);
  delay(20);

  //other direction
  digitalWrite(en1, LOW);
  digitalWrite(en2, HIGH);
  delay(20);
  digitalWrite(BUILTIN_LED_OVERRIDE, LOW);
}

void loop() {
  if (!digitalRead(LISTEN_PIN))
    phone_ring();
}
