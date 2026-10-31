#include "Arduino.h"

const uint8_t RING_PIN = 23;  // PC817 collector, low while the bell rings
const size_t BUFFER_SIZE = 8192;
const size_t MAX_LINE_BYTES = 16;
const unsigned long HEARTBEAT_MS = 5000;

uint32_t edgeTimes[BUFFER_SIZE];
uint8_t edgeLevels[BUFFER_SIZE];
size_t head = 0;
size_t tail = 0;
uint32_t dropped = 0;
uint32_t reportedDropped = 0;
uint32_t lastPrinted = 0;
uint32_t lowSamples = 0;
int lastLevel = HIGH;
unsigned long lastHeartbeat = 0;

void record_edges() {
  int level = digitalRead(RING_PIN);
  if (level == LOW) lowSamples++;
  if (level == lastLevel) return;
  lastLevel = level;
  size_t next = (head + 1) % BUFFER_SIZE;
  if (next == tail) {
    dropped++;
    return;
  }
  edgeTimes[head] = micros();
  edgeLevels[head] = level;
  head = next;
}

void print_one_edge() {
  if (tail == head || Serial.availableForWrite() < (int)MAX_LINE_BYTES) return;
  uint32_t t = edgeTimes[tail];
  Serial.printf("%lu %u\n", (unsigned long)(t - lastPrinted), edgeLevels[tail]);
  lastPrinted = t;
  tail = (tail + 1) % BUFFER_SIZE;
  lastHeartbeat = millis();
}

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(460800);
  pinMode(RING_PIN, INPUT_PULLUP);
  lastLevel = digitalRead(RING_PIN);
  lastPrinted = micros();
  Serial.printf("# ring capture ready, level %d\n", lastLevel);
}

void loop() {
  record_edges();
  print_one_edge();
  if (dropped != reportedDropped) {
    reportedDropped = dropped;
    Serial.printf("# dropped %lu\n", (unsigned long)reportedDropped);
  }
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    Serial.printf("# idle, level %d, low samples %lu\n", digitalRead(RING_PIN), (unsigned long)lowSamples);
    lowSamples = 0;
  }
}
