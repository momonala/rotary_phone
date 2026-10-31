void setup_wifi() {
  Serial.print("Connecting to WiFi: " + String(WIFI_SSID));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("");

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}

void setup_MQTT() {
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setBufferSize(512);
  mqttClient.setCallback(mqtt_on_message_callback);
  reconnect_mqtt();
  delay(500);
}

void reconnect_mqtt() {
  Serial.print("Initializing MQTT.");
  while (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_AVAILABILITY_TOPIC, 0, true, "offline")) {
    delay(500); Serial.print(".");
  }
  mqtt_connects++;
  mqttClient.publish(MQTT_AVAILABILITY_TOPIC, "online", true);
  mqttClient.subscribe(MQTT_TOPIC);
  Serial.println(" Subscribed to MQTT Topic: " + String(MQTT_TOPIC));
}

void report(const String& status) {
  mqttClient.publish(MQTT_STATUS_TOPIC, status.c_str());
}

void publish_state() {
  String json = "{";
  json += "\"uptime_s\":" + String(millis() / 1000);
  json += ",\"wifi_rssi\":" + String(WiFi.RSSI());
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += ",\"reset_reason\":\"" + ESP.getResetReason() + "\"";
  json += ",\"build\":\"" + String(__DATE__ " " __TIME__) + "\"";
  json += ",\"mqtt_connects\":" + String(mqtt_connects);
  json += ",\"hook_lifted\":" + String(hook_lifted ? "true" : "false");
  json += ",\"dial_turning\":" + String(digitalRead(DIALING_PIN) == LOW ? "true" : "false");
  json += ",\"call_volume\":" + String(VOLUME);
  json += ",\"sound_volume\":" + String(SOUND_VOLUME);
  json += ",\"ring_volume\":" + String(RING_VOLUME);
  json += ",\"dfplayer_volume\":" + String(dfplayer_volume);
  json += ",\"dfplayer_tracks\":" + String(dfplayer_tracks);
  json += "}";
  mqttClient.publish(MQTT_STATE_TOPIC, json.c_str(), true);
}

void heartbeat() {
  last_heartbeat = millis();
  dfplayer_volume = DFPlayer.currentVolume();
  if (dfplayer_volume >= 0 && dfplayer_volume != current_volume) {
    Serial.println("DFPlayer volume was " + String(dfplayer_volume) + ", resetting to " + String(current_volume));
    DFPlayer.volume(current_volume);
    delay(200);
    dfplayer_volume = DFPlayer.currentVolume();
  }
  dfplayer_tracks = DFPlayer.numSdTracks();
  publish_state();
}
