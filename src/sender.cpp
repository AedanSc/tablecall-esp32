#include "request_system.h"
#include "delivery_retry.h"
#include <esp_system.h>

using namespace service;
#ifndef TABLE_ID
#define TABLE_ID 1
#endif
static_assert(TABLE_ID > 0 && TABLE_ID <= 65535, "TABLE_ID must be 1..65535");
// Preserve the original button-to-LED channels. The watch translates their labels.
Button buttons[] = {Button(25), Button(26), Button(27)};
// Each LED needs its own 330 ohm series resistor, with cathode to GND.
constexpr uint8_t LED_PINS[] = {19, 21, 18}; // Original LED channels; do not reorder.
TableRequest requests[3];
DeliveryRetry delivery[3];
uint32_t sessionId, nextSequence = 1;
uint8_t ownMac[6];

void pressRequest(uint8_t index) {
  TableRequest &slot = requests[index];
  if (slot.active()) {
    // Re-press forces immediate replay even if a restarted watch lost its queue.
    // Preserve the SAME ID, LED state, and pending request rather than duplicating.
    delivery[index].restart(millis());
    Serial.printf("Rechecking existing %s request\n", name(slot.packet.request));
    return;
  }
  Packet p{};
  p.magic = MAGIC; p.version = VERSION; p.kind = Kind::Request;
  p.table = TABLE_ID; p.session = sessionId; p.sequence = nextSequence++;
  p.request = static_cast<Request>(index); p.status = Status::Accepted;
  memcpy(p.sender, ownMac, 6);
  slot.start(p);
  delivery[index].restart(millis());
  Serial.printf("NEW: table %u %s\n", unsigned(TABLE_ID), name(p.request));
}

void handle(const Received &event) {
  const Packet &p = event.packet;
  if (p.table != TABLE_ID || p.session != sessionId || memcmp(p.sender, ownMac, 6) != 0) return;
  TableRequest &slot = requests[uint8_t(p.request)];
  if (p.kind == Kind::State) {
    // ACK old state retransmissions too, even if the customer has already made
    // a new request. Only a matching request ID can change the current LED.
    if (slot.apply(p)) delivery[uint8_t(p.request)].acknowledged(millis());
    Packet reply = p; reply.kind = Kind::StateAck;
    const esp_err_t result = send(reply);
    if (result != ESP_OK) Serial.printf("State ACK: %s\n", esp_err_to_name(result));
    Serial.printf("%s state: %s\n", name(p.request), p.status == Status::Coming ? "COMING" : "FULFILLED");
  } else if (p.kind == Kind::Ack) {
    if (slot.apply(p)) {
      delivery[uint8_t(p.request)].acknowledged(millis());
      Serial.printf("%s: receiver confirmed\n", name(p.request));
    }
    else if (sameRequest(slot.packet, p) && p.status == Status::Full)
      Serial.println("Receiver queue full; retrying shortly.");
  }
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 3; ++i) {
    buttons[i].begin(); pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW);
  }
  if (!beginRadio()) halt("ESP-NOW startup failed; reset board.");
  WiFi.macAddress(ownMac); sessionId = esp_random();
  Serial.printf("TABLE %u ready. Button channels 25/26/27, LED channels 19/21/18\n", unsigned(TABLE_ID));
}

void loop() {
  Received event;
  for (uint8_t i = 0; i < 24 && xQueueReceive(inbox, &event, 0) == pdTRUE; ++i) handle(event);
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 3; ++i) {
    if (buttons[i].pressed(now)) pressRequest(i);
    TableRequest &slot = requests[i];
    if (slot.active() && delivery[i].due(now)) {
      const esp_err_t result = send(slot.packet);
      delivery[i].sent(now, slot.confirmed, esp_random());
      Serial.printf("%s %s: %s\n", name(slot.packet.request),
                    slot.confirmed ? "rechecking watch queue" : "sending/retrying", esp_err_to_name(result));
      if (!slot.confirmed && delivery[i].fastAttempts == DeliveryRetry::FAST_ATTEMPTS)
        Serial.println("Not confirmed yet; will keep retrying about every 5 seconds.");
    }
    const bool lit = slot.active() &&
      (slot.state != Status::Coming || (now / 350) % 2 == 0);
    digitalWrite(LED_PINS[i], lit ? HIGH : LOW);
  }
  delay(1);
}
