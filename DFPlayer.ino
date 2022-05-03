
void setup_audio_player() {
  mySoftwareSerial.begin(9600);
  Serial.print("Initializing DFPlayer");
  bool debug = false;
  while (!DFPlayer.begin(mySoftwareSerial, debug)) {
    delay(500); Serial.print(".");
  }
  Serial.println(" DFPlayer Mini online!");
  DFPlayer.volume(1);  // Set volume value. From 0 to 30
  DFPlayer.setTimeout(2000);
}

void stop_dfplayer(){
  delay(500);
  DFPlayer.stop();
  delay(500);
}

void play_startup_tone() {
  stop_dfplayer();
  DFPlayer.playFolder(1, 98);
  delay(100);
}

void play_helpme_and_wait() {
  stop_dfplayer();
  DFPlayer.playFolder(1, 99);
  delay(100);
}

void play_song_by_idx(int idx) {
  Serial.print("Playing song idx: ");
  Serial.println(idx);
  stop_dfplayer();
  DFPlayer.playFolder(1, idx);
  delay(100);
}
