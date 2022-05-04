// h-bridge outputs
# define en1 13
# define en2 12
# define enA 14

# define LISTEN_PIN 5
# define LED_BUILTIN_OVERRIDE 2

int pin_val = 1;

void setup() {
  Serial.begin(9600);
  pinMode(enA, OUTPUT);
  pinMode(en1, OUTPUT);
  pinMode(en2, OUTPUT);
  pinMode(LISTEN_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN_OVERRIDE, OUTPUT);
  digitalWrite(LED_BUILTIN_OVERRIDE, LOW);
}

void ring_constantly() {
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
}



void loop() {
  if (!digitalRead(LISTEN_PIN))
    ring_constantly();
  digitalWrite(LED_BUILTIN_OVERRIDE, LOW);
}
