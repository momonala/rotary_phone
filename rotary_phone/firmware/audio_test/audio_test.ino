#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFPlayerMini_Fast.h"

# define SOFTWARE_SERIAL_RX 4     // pin D2 - DFPlayer TX
# define SOFTWARE_SERIAL_TX 5     // pin D1 - DFPlayer RX
# define RING_PIN_A 16            // pin D0 - DRV8871 IN2
# define RING_PIN_B 14            // pin D5 - DRV8871 IN1
# define STATUS_LED_PIN 2         // pin D4 - built-in LED, on when low

const int VOLUME = 30;  // 0 to 30
const int AUDIO_FOLDER = 1;
const int TRACKS[] = {98, 30, 31, 32, 33};
const int NUM_TRACKS = sizeof(TRACKS) / sizeof(TRACKS[0]);
const unsigned long PLAY_MS = 8000;

SoftwareSerial mySoftwareSerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX);
DFPlayerMini_Fast DFPlayer;

int track_idx = 0;

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  pinMode(RING_PIN_A, OUTPUT);
  pinMode(RING_PIN_B, OUTPUT);
  digitalWrite(RING_PIN_A, LOW);
  digitalWrite(RING_PIN_B, LOW);

  Serial.begin(9600);
  Serial.println("");
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
  Serial.print("Tracks on SD card: ");
  Serial.println(DFPlayer.numSdTracks());
  Serial.print("Folders on SD card: ");
  Serial.println(DFPlayer.numFolders());
}

void loop() {
  int track = TRACKS[track_idx];
  track_idx = (track_idx + 1) % NUM_TRACKS;

  Serial.print("Playing folder 01, track ");
  Serial.println(track);
  DFPlayer.playFolder(AUDIO_FOLDER, track);
  delay(1500);
  Serial.print("  isPlaying: ");
  Serial.println(DFPlayer.isPlaying() ? "yes" : "no");
  delay(PLAY_MS - 1500);
  DFPlayer.stop();
  delay(500);
}
