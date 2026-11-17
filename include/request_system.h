#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "request_protocol.h"

namespace service {
constexpr uint8_t CHANNEL = 1;  // Both boards must use the same channel.
constexpr uint8_t BROADCAST[6] = {255, 255, 255, 255, 255, 255};
struct Received { Packet packet; uint8_t source[6]; };
static QueueHandle_t inbox = nullptr;

inline const char *name(Request request) {
  switch (request) {
    case Request::Waiter: return "WAITER";
    case Request::Water: return "WATER";
    case Request::Check: return "CHECK";
  }
  return "?";
}

class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}
  void begin() {
    pinMode(pin_, INPUT_PULLUP);
    raw_ = stable_ = digitalRead(pin_);
    changed_ = millis();
  }
  bool pressed(uint32_t now) {
    const bool reading = digitalRead(pin_);
    if (reading != raw_) { raw_ = reading; changed_ = now; }
    if (raw_ != stable_ && uint32_t(now - changed_) >= 30) {
      stable_ = raw_;
      return stable_ == LOW;
    }
    return false;
  }
 private:
  uint8_t pin_;
  bool raw_ = HIGH, stable_ = HIGH;
  uint32_t changed_ = 0;
};

// Wi-Fi task only copies data; application logic and display run in loop().
inline void receive(const uint8_t *source, const uint8_t *data, int length) {
  if (length != sizeof(Packet) || !inbox) return;
  Received event{};
  memcpy(&event.packet, data, sizeof(Packet));
  const Packet &p = event.packet;
  if (!validPacket(p)) return;
  memcpy(event.source, source, 6);
  xQueueSend(inbox, &event, 0); // Lost inbox packets are handled by retries.
}
#if ESP_IDF_VERSION_MAJOR >= 5
inline void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
  receive(info->src_addr, data, length);
}
#else
inline void onReceive(const uint8_t *source, const uint8_t *data, int length) {
  receive(source, data, length);
}
#endif

inline bool beginRadio() {
  inbox = xQueueCreate(24, sizeof(Received));
  if (!inbox || !WiFi.mode(WIFI_STA)) return false;
  if (esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
  if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) return false;
  if (esp_now_init() != ESP_OK) return false;
  if (esp_now_register_recv_cb(onReceive) != ESP_OK) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) return false;
  Serial.printf("MAC: %s | ESP-NOW channel: %u\n", WiFi.macAddress().c_str(), CHANNEL);
  return true;
}
inline esp_err_t send(const Packet &packet) {
  return esp_now_send(BROADCAST, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}
inline void halt(const char *message) {
  Serial.println(message);
  while (true) delay(1000);
}
} // namespace service
