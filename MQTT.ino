void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.print("Connecting to WiFi: " + String(ssid));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

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
  mqttClient.setCallback(mqtt_on_message_callback);
  reconnect_mqtt();

  // switch off built-in blue LED
  pinMode(BUILTIN_LED_OVERRIDE, OUTPUT);
  digitalWrite(BUILTIN_LED_OVERRIDE, HIGH);
  delay(500);
}



void reconnect_mqtt() {
  Serial.print("Initializing MQTT.");
  while (!mqttClient.connect(MQTT_CLIENT_ID)) {
    delay(500); Serial.print(".");
  }
  mqttClient.subscribe(MQTT_TOPIC);
  Serial.println(" Subscribed to MQTT Topic: " + String(MQTT_TOPIC));
}
