#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFPlayerMini_Fast.h"

# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define SOFTWARE_SERIAL_RX 4     // pin D2 - DFPlayer TX
# define SOFTWARE_SERIAL_TX 5     // pin D1 - DFPlayer RX
# define NUMBER_PIN 13            // pin D7 - dial pulse contact
# define PHONE_HOOK_PIN 12        // pin D6 - low when phone is lifted
# define DIALING_PIN 0            // pin D3 - low while the dial is turning
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

const int VOLUME = 30;  // 0 to 30
const int AUDIO_FOLDER = 1;
const int STARTUP_TRACK = 98;
const int HELP_ME_TRACK = 99;
const int FIRST_MESSAGE_TRACK = 0;
const int LAST_MESSAGE_TRACK = 14;

const unsigned long START_DELAY_MS = 5000;
const unsigned long NEXT_RING_DELAY_MS = 10000;
const unsigned int MAX_RING_BURSTS = 5;
const unsigned long RING_ON_MS = 2000;
const unsigned long RING_OFF_MS = 4000;
const unsigned long BELL_HALF_PERIOD_MS = 20;
const unsigned long HOOK_DEBOUNCE_MS = 100;
const unsigned long AUDIO_START_GRACE_MS = 1500;
const unsigned long AUDIO_POLL_MS = 500;
const unsigned long DIAL_TIMEOUT_MS = 30000;
const unsigned long DIGIT_GAP_MS = 200;
const unsigned long NUMBER_DONE_MS = 2500;

SoftwareSerial mySoftwareSerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX);
DFPlayerMini_Fast DFPlayer;

bool hook_raw_lifted = false;
bool hook_lifted = false;
unsigned long hook_raw_since = 0;

// ----------------------------------------------------------
void update_hook() {
  bool raw = digitalRead(PHONE_HOOK_PIN) == LOW;
  if (raw != hook_raw_lifted) {
    hook_raw_lifted = raw;
    hook_raw_since = millis();
  } else if (millis() - hook_raw_since >= HOOK_DEBOUNCE_MS && hook_lifted != raw) {
    hook_lifted = raw;
    Serial.println(hook_lifted ? "Hook: lifted" : "Hook: on hook");
  }
}

void update_led() {
  bool dial_click = digitalRead(NUMBER_PIN) == HIGH;
  digitalWrite(STATUS_LED_PIN, hook_lifted != dial_click ? LOW : HIGH);
}

void poll() {
  update_hook();
  update_led();
}

void wait_ms(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    poll();
    delay(5);
  }
}

bool wait_for_pickup(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    poll();
    if (hook_lifted)
      return true;
    delay(1);
  }
  return false;
}

void wait_for_hang_up() {
  while (hook_lifted) {
    poll();
    delay(5);
  }
}

// ----------------------------------------------------------
void bell_off() {
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);
}

bool ring_until_picked_up() {
  Serial.println("Phone ringing. Waiting...");
  for (unsigned int burst = 0; burst < MAX_RING_BURSTS; burst++) {
    unsigned long burst_start = millis();
    bool polarity = false;
    while (millis() - burst_start < RING_ON_MS) {
      polarity = !polarity;
      digitalWrite(RING_PIN_A, polarity ? HIGH : LOW);
      digitalWrite(RING_PIN_B, polarity ? LOW : HIGH);
      if (wait_for_pickup(BELL_HALF_PERIOD_MS)) {
        bell_off();
        Serial.println("Phone picked up!");
        return true;
      }
    }
    bell_off();
    if (wait_for_pickup(RING_OFF_MS)) {
      Serial.println("Phone picked up!");
      return true;
    }
  }
  Serial.println("No pick up.");
  return false;
}

// ----------------------------------------------------------
void setup_audio_player() {
  mySoftwareSerial.begin(9600);
  Serial.print("Initializing DFPlayer");
  while (!DFPlayer.begin(mySoftwareSerial, false)) {
    delay(500); Serial.print(".");
  }
  Serial.println(" DFPlayer Mini online!");
  DFPlayer.setTimeout(500);
  delay(500);
  DFPlayer.volume(VOLUME);
  delay(200);
  Serial.print("Volume read back: ");
  Serial.println(DFPlayer.currentVolume());
}

void play_track(int track) {
  Serial.print("Playing track: ");
  Serial.println(track);
  DFPlayer.playFolder(AUDIO_FOLDER, track);
}

// The DFPlayer reports "not playing" for a moment right after a play command, and
// can drop a status reply over software serial, so require two silent polls in a row.
void wait_for_song_end_or_hang_up() {
  unsigned long started = millis();
  unsigned long last_poll = 0;
  int silent_polls = 0;
  while (silent_polls < 2) {
    poll();
    if (!hook_lifted) {
      Serial.println("Hung up.");
      DFPlayer.stop();
      return;
    }
    if (millis() - started > AUDIO_START_GRACE_MS && millis() - last_poll > AUDIO_POLL_MS) {
      last_poll = millis();
      silent_polls = DFPlayer.isPlaying() ? 0 : silent_polls + 1;
    }
    delay(5);
  }
  Serial.println("Audio finished.");
}

// ----------------------------------------------------------
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
      return number;
    }
    delay(15);
  }
  return number;
}

int track_for_number(const String& number) {
  if (number.equals("69"))
    return 30;
  if (number.equals("999"))
    return 31;
  if (number.equals("8675309"))
    return 32;
  if (number.equals("911"))
    return 33;
  return random(FIRST_MESSAGE_TRACK, LAST_MESSAGE_TRACK + 1);
}

void handle_call() {
  play_track(HELP_ME_TRACK);
  String number = wait_for_dialed_number();
  if (!hook_lifted) {
    Serial.println("Hung up.");
    DFPlayer.stop();
    return;
  }
  if (number == "") {
    Serial.println("Nothing dialed.");
    return;
  }
  play_track(track_for_number(number));
  wait_for_song_end_or_hang_up();
}

// ----------------------------------------------------------
void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);
  pinMode(RING_PIN_A, OUTPUT);
  pinMode(RING_PIN_B, OUTPUT);
  bell_off();
  pinMode(PHONE_HOOK_PIN, INPUT_PULLUP);
  pinMode(DIALING_PIN, INPUT_PULLUP);
  pinMode(NUMBER_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("");
  randomSeed(ESP.random());
  setup_audio_player();

  play_track(STARTUP_TRACK);
  Serial.println("Ringing in 5s...");
  wait_ms(START_DELAY_MS);
}

void loop() {
  if (hook_lifted) {
    Serial.println("Hang up to start the next ring.");
    wait_for_hang_up();
  }
  if (ring_until_picked_up())
    handle_call();
  bell_off();
  wait_for_hang_up();
  Serial.println("Call finished. Ringing again in 10s...");
  wait_ms(NEXT_RING_DELAY_MS);
}
