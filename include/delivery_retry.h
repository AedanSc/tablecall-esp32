#pragma once
#include <stdint.h>

namespace service {
// Never abandon an open request. Fast initial attempts give a responsive demo;
// slow periodic replay also restores the watch's volatile queue after a reset.
struct DeliveryRetry {
  static constexpr uint8_t FAST_ATTEMPTS = 5;
  static constexpr uint32_t SLOW_MS = 5000;
  uint8_t fastAttempts = 0;
  uint32_t retryAt = 0;
  void restart(uint32_t now) { fastAttempts = 0; retryAt = now; }
  bool due(uint32_t now) const { return int32_t(now - retryAt) >= 0; }
  void sent(uint32_t now, bool confirmed, uint32_t jitter) {
    if (fastAttempts < FAST_ATTEMPTS) ++fastAttempts;
    retryAt = now + ((confirmed || fastAttempts >= FAST_ATTEMPTS) ? SLOW_MS : 450) + jitter % 151;
  }
  void acknowledged(uint32_t now) { retryAt = now + SLOW_MS; }
};
} // namespace service
