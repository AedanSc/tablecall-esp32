#include "request_system.h"
#include "watch_display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>

using namespace service;
constexpr uint8_t OLED_SDA = 13, OLED_SCL = 14, BUZZER = 25;
constexpr uint8_t HISTORY_SIZE = 64;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Button waiterButton(27);
struct Record { Packet packet; Status state; };
WatchQueue queue;
Record history[HISTORY_SIZE];
uint8_t historyCount = 0, historyNext = 0;
bool dirty = true, buzzing = false;
uint32_t beepStarted = 0, lastDraw = 0;

void draw() {
  drawWatch(display, queue, millis());
}

void transmit(const Packet &request, Kind kind, Status state) {
  Packet reply = request; reply.kind = kind; reply.status = state;
  const esp_err_t result = send(reply);
  if (result != ESP_OK) Serial.printf("Radio reply: %s\n", esp_err_to_name(result));
}

void remember(const Packet &packet, Status state) {
  for (uint8_t i = 0; i < historyCount; ++i) {
    if (sameRequest(packet, history[i].packet)) { history[i].state = state; return; }
  }
  history[historyNext] = {packet, state};
  historyNext = (historyNext + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) ++historyCount;
}

void handle(const Received &event) {
  const Packet &p = event.packet;
  if (memcmp(p.sender, event.source, 6) != 0) return;
  if (p.kind == Kind::StateAck) {
    if (queue.acknowledge(p)) dirty = true;
    return;
  }
  if (p.kind != Kind::Request) return;
  // Pending IDs take precedence over the bounded completed-message history.
  const int index = queue.find(p);
  if (index >= 0) {
    transmit(p, Kind::Ack, Status::Accepted);
    if (isProgress(queue.entries[index].state)) transmit(p, Kind::State, queue.entries[index].state);
    return;
  }
  for (uint8_t i = 0; i < historyCount; ++i) {
    if (!sameRequest(p, history[i].packet)) continue;
    transmit(p, Kind::Ack, Status::Accepted);
    if (isProgress(history[i].state)) transmit(p, Kind::State, history[i].state);
    return;
  }
  if (!queue.add(p, millis())) {
    transmit(p, Kind::Ack, Status::Full); Serial.println("Queue full; request NOT accepted."); return;
  }
  remember(p, queue.entries[queue.count - 1].state);
  transmit(p, Kind::Ack, Status::Accepted);
  Serial.printf("NEW: table %u %s | requests %u, table groups %u\n", p.table, name(p.request), queue.count, queue.groupCount);
  digitalWrite(BUZZER, HIGH); buzzing = true; beepStarted = millis(); dirty = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW); waiterButton.begin();
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(50);
  uint8_t address = 0;
  for (uint8_t candidate : {uint8_t(0x3C), uint8_t(0x3D)}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) { address = candidate; break; }
  }
  if (!address) halt("OLED not found at 0x3C/0x3D. Check 3V3, GND, SDA=13, SCL=14; then reset.");
  if (!display.begin(SSD1306_SWITCHCAPVCC, address, true, false)) halt("OLED initialization failed; reset board.");
  Serial.printf("SSD1306 128x64 OLED ready at I2C 0x%02X\n", address);
  draw();
  if (!beginRadio()) {
    display.clearDisplay(); display.setCursor(0, 0); display.print("Radio failed"); display.display();
    halt("ESP-NOW startup failed; reset board.");
  }
  Serial.println("WAITER ready. GPIO27 controls FRONT TABLE: first press COMING, second FULFILLED.");
}

void loop() {
  Received event;
  for (uint8_t i = 0; i < 24 && xQueueReceive(inbox, &event, 0) == pdTRUE; ++i) handle(event);
  const uint32_t now = millis();
  if (buzzing && uint32_t(now - beepStarted) >= 150) { digitalWrite(BUZZER, LOW); buzzing = false; }
  if (waiterButton.pressed(now) && queue.advance(now)) {
    const auto &group = queue.groups[0];
    for (uint8_t i = 0; i < queue.count; ++i)
      if (queue.entries[i].group == group.id) remember(queue.entries[i].packet, queue.entries[i].state);
    dirty = true;
    Serial.printf("Table %u GROUP: %s\n", group.table,
                  group.state == Status::Coming ? "COMING" : "FULFILLED");
  }
  // Keep retrying state changes until the TABLE confirms receipt. Never remove
  // a fulfilled request before its LED-off update is acknowledged.
  for (uint8_t i = 0; i < queue.count; ++i) {
    auto &entry = queue.entries[i];
    if (!entry.synced && int32_t(now - entry.retryAt) >= 0) {
      transmit(entry.packet, Kind::State, entry.state);
      entry.retryAt = now + 450 + esp_random() % 151;
    }
  }
  if (dirty || uint32_t(now - lastDraw) >= 1000) { draw(); dirty = false; lastDraw = now; }
  delay(1);
}
