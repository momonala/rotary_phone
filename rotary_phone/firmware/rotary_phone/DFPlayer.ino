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
}

void set_volume(int volume) {
  if (volume == current_volume)
    return;
  DFPlayer.volume(volume);
  current_volume = volume;
  delay(100);
}

void play_sound(int track) {
  set_volume(SOUND_VOLUME);
  Serial.print("Playing sound: ");
  Serial.println(track);
  DFPlayer.playFolder(AUDIO_FOLDER, track);
  report("playing:" + String(track));
}

void play_track(int track) {
  set_volume(VOLUME);
  Serial.print("Playing track: ");
  Serial.println(track);
  DFPlayer.playFolder(AUDIO_FOLDER, track);
  report("playing:" + String(track));
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
      report("hung_up");
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
  report("finished");
}
