#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFPlayerMini_Fast.h"
#include "PubSubClient.h"
#include "ESP8266WiFi.h"

# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define SOFTWARE_SERIAL_RX 4     // pin D2 - DFPlayer TX
# define SOFTWARE_SERIAL_TX 5     // pin D1 - DFPlayer RX
# define NUMBER_PIN 13            // pin D7 - dial pulse contact
# define PHONE_HOOK_PIN 12        // pin D6 - low when phone is lifted
# define DIALING_PIN 0            // pin D3 - low while the dial is turning
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

const char* MQTT_SERVER = "192.168.2.107";
const char* MQTT_TOPIC = "home/rotary_phone";
const char* MQTT_STATUS_TOPIC = "home/rotary_phone/status";
const char* MQTT_STATE_TOPIC = "home/rotary_phone/state";
const char* MQTT_AVAILABILITY_TOPIC = "home/rotary_phone/availability";
const char* MQTT_CLIENT_ID = "rotary_phone";

const int VOLUME = 15;  // 0 to 30
const int SOUND_VOLUME = 25;  // 0 to 30, robot sounds only
const int RING_VOLUME = 30;  // 0 to 30, ring tone while ringing
const int AUDIO_FOLDER = 1;
const int RING_TRACK = 97;
const int STARTUP_TRACK = 98;
const int HELP_ME_TRACK = 99;
const int FIRST_MESSAGE_TRACK = 0;
const int LAST_MESSAGE_TRACK = 14;

const unsigned long MAX_RING_MS = 120000;
const unsigned long PICKUP_TO_AUDIO_MS = 1000;
const unsigned long RING_ON_MS = 2000;
const unsigned long RING_OFF_MS = 4000;
const unsigned long BELL_HALF_PERIOD_MS = 20;
const unsigned long HOOK_DEBOUNCE_MS = 100;
const unsigned long AUDIO_START_GRACE_MS = 1500;
const unsigned long AUDIO_POLL_MS = 500;
const unsigned long DIAL_TIMEOUT_MS = 30000;
const unsigned long DIGIT_GAP_MS = 200;
const unsigned long NUMBER_DONE_MS = 2500;
const unsigned long HEARTBEAT_MS = 30000;
const unsigned long BELL_TEST_MS = 2000;

SoftwareSerial mySoftwareSerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX);
DFPlayerMini_Fast DFPlayer;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool hook_raw_lifted = false;
bool hook_lifted = false;
unsigned long hook_raw_since = 0;
enum class Command : uint8_t { NONE, STOP, PLAY, RING, BELL };
enum class BellMode : uint8_t { NONE, DC, AC };
enum class RingInterrupt : uint8_t { NONE, LIFTED, STOPPED };

struct PendingCommand {
  Command command;
  int track;
  BellMode bell;
};

const PendingCommand NO_COMMAND = {Command::NONE, -1, BellMode::NONE};

PendingCommand pending_command = NO_COMMAND;
bool ring_requested = false;
int requested_track = -1;
BellMode bell_test_mode = BellMode::NONE;
unsigned long last_heartbeat = 0;
unsigned int mqtt_connects = 0;
int dfplayer_volume = -1;
int current_volume = VOLUME;
int dfplayer_tracks = -1;

// ----------------------------------------------------------
void update_hook() {
  bool raw = digitalRead(PHONE_HOOK_PIN) == LOW;
  if (raw != hook_raw_lifted) {
    hook_raw_lifted = raw;
    hook_raw_since = millis();
  } else if (millis() - hook_raw_since >= HOOK_DEBOUNCE_MS && hook_lifted != raw) {
    hook_lifted = raw;
    Serial.println(hook_lifted ? "Hook: lifted" : "Hook: on hook");
    publish_state();
  }
}

void update_led() {
  bool dial_click = digitalRead(NUMBER_PIN) == HIGH;
  digitalWrite(STATUS_LED_PIN, hook_lifted != dial_click ? LOW : HIGH);
}

// One slot is enough: PubSubClient delivers at most one message per mqttClient.loop() call.
PendingCommand consume_command() {
  PendingCommand taken = pending_command;
  pending_command = NO_COMMAND;
  return taken;
}

Command act_on_command(const PendingCommand& pending) {
  switch (pending.command) {
    case Command::STOP:
      DFPlayer.stop();
      break;
    case Command::PLAY:
      play_sound(pending.track);
      break;
    case Command::RING:
      requested_track = pending.track;
      ring_requested = true;
      break;
    case Command::BELL:
      bell_test_mode = pending.bell;
      break;
    case Command::NONE:
      break;
  }
  return pending.command;
}

Command poll() {
  mqttClient.loop();
  Command command = act_on_command(consume_command());
  update_hook();
  update_led();
  return command;
}

RingInterrupt ring_wait(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (poll() == Command::STOP)
      return RingInterrupt::STOPPED;
    if (hook_lifted)
      return RingInterrupt::LIFTED;
    delay(1);
  }
  return RingInterrupt::NONE;
}

bool wait_while_lifted(unsigned long ms) {
  unsigned long start = millis();
  while (hook_lifted && millis() - start < ms) {
    poll();
    delay(5);
  }
  return hook_lifted;
}

// ----------------------------------------------------------
void bell_off() {
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);
}

bool ring_until_picked_up() {
  Serial.println("Phone ringing. Waiting...");
  report("ringing");
  RingInterrupt interrupt = RingInterrupt::NONE;
  unsigned long started = millis();
  while (interrupt == RingInterrupt::NONE && millis() - started < MAX_RING_MS) {
    unsigned long burst_start = millis();
    bool polarity = false;
    set_volume(RING_VOLUME);
    DFPlayer.playFolder(AUDIO_FOLDER, RING_TRACK);
    while (interrupt == RingInterrupt::NONE && millis() - burst_start < RING_ON_MS) {
      polarity = !polarity;
      digitalWrite(RING_PIN_A, polarity ? HIGH : LOW);
      digitalWrite(RING_PIN_B, polarity ? LOW : HIGH);
      interrupt = ring_wait(BELL_HALF_PERIOD_MS);
    }
    bell_off();
    if (interrupt == RingInterrupt::NONE)
      interrupt = ring_wait(RING_OFF_MS);
  }
  bell_off();
  DFPlayer.stop();
  if (interrupt == RingInterrupt::STOPPED) {
    Serial.println("Stopped by MQTT.");
    report("stopped");
    return false;
  }
  if (!hook_lifted) {
    Serial.println("No pick up.");
    report("no_answer");
    return false;
  }
  Serial.println("Phone picked up!");
  report("picked_up");
  return true;
}

// ----------------------------------------------------------
int track_for_number(const String& number) {
  if (number.equals("69"))
    return 30;  // Rick Roll
  if (number.equals("999"))
    return 31;  // IT Crowd
  if (number.equals("8675309"))
    return 32;  // Jenny
  if (number.equals("911"))
    return 33;  // Taken speech
  return random(FIRST_MESSAGE_TRACK, LAST_MESSAGE_TRACK + 1);
}

void handle_call() {
  if (!wait_while_lifted(PICKUP_TO_AUDIO_MS)) {
    Serial.println("Hung up.");
    report("hung_up");
    return;
  }
  if (requested_track >= 0) {
    play_track(requested_track);
    wait_for_song_end_or_hang_up();
    return;
  }
  play_track(HELP_ME_TRACK);
  String number = wait_for_dialed_number();
  if (!hook_lifted) {
    Serial.println("Hung up.");
    report("hung_up");
    DFPlayer.stop();
    return;
  }
  play_track(track_for_number(number));
  wait_for_song_end_or_hang_up();
}

void run_ring_request() {
  ring_requested = false;
  if (hook_lifted) {
    Serial.println("Handset already lifted. Ignoring ring request.");
    report("busy");
    return;
  }
  if (ring_until_picked_up())
    handle_call();
  ring_requested = false;
  Serial.println("Call finished.");
}

// ----------------------------------------------------------
int parse_track(const char* text, unsigned int length) {
  if (length == 0)
    return -1;
  long value = 0;
  for (unsigned int i = 0; i < length; i++) {
    if (!isDigit(text[i]))
      return -1;
    long digit = text[i] - '0';
    value = value > (LONG_MAX - digit) / 10 ? LONG_MAX : value * 10 + digit;
  }
  return value;
}

bool message_is(const char* text, unsigned int length, const char* expected) {
  return length == strlen(expected) && memcmp(text, expected, length) == 0;
}

PendingCommand parse_command(const char* text, unsigned int length) {
  PendingCommand parsed = NO_COMMAND;
  if (length == 4 && strncasecmp(text, "stop", 4) == 0) {
    parsed.command = Command::STOP;
  } else if (length >= 5 && memcmp(text, "play:", 5) == 0) {
    parsed.track = parse_track(text + 5, length - 5);
    if (parsed.track >= 0)
      parsed.command = Command::PLAY;
  } else if (message_is(text, length, "bell:dc")) {
    parsed.command = Command::BELL;
    parsed.bell = BellMode::DC;
  } else if (message_is(text, length, "bell:ac")) {
    parsed.command = Command::BELL;
    parsed.bell = BellMode::AC;
  } else if (message_is(text, length, "x")) {
    Serial.println("Not a valid payload for a code path. Skipping.");
  } else {
    parsed.command = Command::RING;
    parsed.track = parse_track(text, length);
  }
  return parsed;
}

void mqtt_on_message_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived. Topic: [");
  Serial.print(topic);
  Serial.print("] length: ");
  Serial.println(length);
  const char* text = (const char*)payload;
  while (length > 0 && isspace(text[0])) {
    text++;
    length--;
  }
  while (length > 0 && isspace(text[length - 1]))
    length--;
  Serial.print("Payload: ");
  Serial.write(text, length);
  Serial.println();
  pending_command = parse_command(text, length);
}

void run_bell_test() {
  BellMode mode = bell_test_mode;
  bell_test_mode = BellMode::NONE;
  const char* mode_name = mode == BellMode::AC ? "ac" : "dc";
  Serial.print("Bell test: ");
  Serial.println(mode_name);
  report(mode == BellMode::AC ? "bell_ac" : "bell_dc");
  unsigned long started = millis();
  bool polarity = false;
  while (millis() - started < BELL_TEST_MS) {
    if (mode == BellMode::AC)
      polarity = !polarity;
    digitalWrite(RING_PIN_A, polarity ? HIGH : LOW);
    digitalWrite(RING_PIN_B, polarity ? LOW : HIGH);
    unsigned long wait_start = millis();
    while (millis() - wait_start < BELL_HALF_PERIOD_MS) {
      poll();
      delay(1);
    }
  }
  bell_off();
  report("bell_off");
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
  setup_wifi();
  setup_MQTT();
  setup_audio_player();

  Serial.println("Ready for action!");
  report("online");
  heartbeat();
  play_sound(STARTUP_TRACK);
}

void loop() {
  if (!mqttClient.connected())
    reconnect_mqtt();
  poll();
  if (ring_requested)
    run_ring_request();
  if (bell_test_mode != BellMode::NONE)
    run_bell_test();
  if (millis() - last_heartbeat > HEARTBEAT_MS)
    heartbeat();
  delay(10);
}
