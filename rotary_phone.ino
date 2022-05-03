// DFPlayer
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFPlayerMini_Fast.h"

// MQTT/WIFI
#include "PubSubClient.h" // Connect and publish to the MQTT broker
#include "ESP8266WiFi.h"  // Enables the ESP8266 to connect to the local network (via WiFi)
#include "arduino_secrets.h"
//const char* ssid = "";
//const char* pass = "";

# define RINGER_PIN 0             // phone rings when we write low
# define BUILTIN_LED_OVERRIDE 2
# define SOFTWARE_SERIAL_RX 4
# define SOFTWARE_SERIAL_TX 5
# define NUMBER_PIN 12
# define PHONE_HOOK_PIN 13        // low when phone is lifted
# define DIALING_PIN 14

// DFPlayer
SoftwareSerial mySoftwareSerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX);
DFPlayerMini_Fast DFPlayer;

// MQTT
const char* MQTT_SERVER = "192.168.0.183";  // IP of the MQTT broker (raspberry pi)
const char* MQTT_TOPIC = "home/rotary_phone";
const char* MQTT_CLIENT_ID = "rotary_phone";
WiFiClient espClient;
PubSubClient mqttClient(espClient);


// ----------------------------------------------------------
void mqtt_on_message_callback(char* topic, byte* payload, unsigned int length) {
  // get MQTT message payload, cast to string
  payload[length] = '\0'; // Add a NULL to the end of the char* to make it a string.
  String mqtt_msg = String((char *)payload);  // cast byte array to string (yes, bad for RAM. I dont care.)
  Serial.println("Message arrived.  Topic: [" + String(topic) + "] payload: " + mqtt_msg);

  // todo depending on idx of MQTT message, we can perform different logic paths
  if (mqtt_msg.equals("x")) {
    Serial.println("No a valid payload for a code path. Skipping.");
    return;
  }

  // ring the phone until it gets picked up. If 30s elapse, stop ringing and return.
  digitalWrite(RINGER_PIN, LOW);
  unsigned int t0 = millis();
  while (digitalRead(PHONE_HOOK_PIN)) {
    if (millis() - t0 > 30000){
      Serial.println("No pick up :( bye");
      digitalWrite(RINGER_PIN, HIGH);
      return;
    }
    delay(2);
  }
  digitalWrite(RINGER_PIN, HIGH);

  // start an audio routine
  help_me_im_stuck();
  Serial.println("Exit");
}

// ----------------------------------------------------------
void help_me_im_stuck() {
  play_helpme_and_wait();
  String dialed_number = wait_for_dialed_number();

  if (dialed_number.equals("69")) {
    play_song_by_idx(30);  // get rick rolled
  }
  else if (dialed_number.equals("999")) {
    play_song_by_idx(31); // IT Crowd
  }
  else if (dialed_number.equals("8675309")) {
    play_song_by_idx(32);  // Jenny Jenny
  }
  else if (dialed_number.equals("911")) {
    play_song_by_idx(33); // Taken Speech
  }
  else {
    play_song_by_idx(random(0, 14));  //  annoying message
  }

  wait_for_phone_hook_or_audio_to_finish();
}

// ----------------------------------------------------------
void wait_for_phone_hook_or_audio_to_finish() {
  // turn off song if phone hook is off. Otherwise wait till it is.
  if (digitalRead(PHONE_HOOK_PIN)) {
    Serial.println("Phone off the hook.");
    stop_dfplayer();
  }

  // break once audio is done or we hang up
  int is_playing = 0;
  while (!digitalRead(PHONE_HOOK_PIN)) {
    for (int i = 0; i < 5; i++) {
      is_playing += DFPlayer.isPlaying();
    }
    if (is_playing == 0) {
      Serial.println("Audio finished.");
      break;
    }
    is_playing = 0;
    if (digitalRead(PHONE_HOOK_PIN)) {
      Serial.println("Phone off the hook.");
      stop_dfplayer();
    }
  }
}


// ----------------------------------------------------------
void setup() {
  Serial.begin(9600);
  Serial.println("");
  setup_wifi();
  setup_MQTT();
  setup_audio_player();
  setup_dialer();
  Serial.println("Ready for action!");
    play_startup_tone();

  pinMode(RINGER_PIN, OUTPUT);
  digitalWrite(RINGER_PIN, HIGH);
}

// ----------------------------------------------------------
void loop() {
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

  delay(10);
}
