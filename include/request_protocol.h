#pragma once
#include <stdint.h>
#include <string.h>

namespace service {
constexpr uint32_t MAGIC = 0x57414954;
constexpr uint8_t VERSION = 2; // Re-upload BOTH boards when changing protocol.
enum class Kind : uint8_t { Request = 1, Ack = 2, State = 3, StateAck = 4 };
enum class Request : uint8_t { Waiter = 0, Water = 1, Check = 2 };
enum class Status : uint8_t { Accepted = 0, Full = 1, Coming = 2, Fulfilled = 3 };

struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint8_t version;
  Kind kind;
  uint16_t table;
  uint32_t session;
  uint32_t sequence;
  Request request;
  Status status;
  uint8_t sender[6]; // Original table MAC, echoed in all replies.
};
static_assert(sizeof(Packet) == 24, "Unexpected wire format");
inline bool sameRequest(const Packet &a, const Packet &b) {
  return memcmp(a.sender, b.sender, 6) == 0 && a.session == b.session &&
         a.sequence == b.sequence && a.table == b.table && a.request == b.request;
}
inline bool isProgress(Status state) {
  return state == Status::Coming || state == Status::Fulfilled;
}
inline bool validPacket(const Packet &p) {
  if (p.magic != MAGIC || p.version != VERSION || !p.table || uint8_t(p.request) > 2) return false;
  switch (p.kind) {
    case Kind::Request: return p.status == Status::Accepted;
    case Kind::Ack: return p.status == Status::Accepted || p.status == Status::Full;
    case Kind::State:
    case Kind::StateAck: return isProgress(p.status);
  }
  return false;
}

// One independent lifecycle per table button. Late packets cannot regress state
// or affect a newer request, including a delayed fulfillment from a previous one.
struct TableRequest {
  Packet packet{};
  bool valid = false, confirmed = false;
  Status state = Status::Accepted;
  bool active() const { return valid && state != Status::Fulfilled; }
  bool start(const Packet &p) {
    if (active()) return false;
    packet = p; valid = true; confirmed = false; state = Status::Accepted;
    return true;
  }
  bool apply(const Packet &p) {
    if (!valid || !sameRequest(packet, p)) return false;
    if (p.kind == Kind::Ack && p.status == Status::Accepted) confirmed = true;
    else if (p.kind == Kind::State && isProgress(p.status)) {
      confirmed = true;
      if (uint8_t(p.status) > uint8_t(state)) state = p.status;
    } else return false;
    return true;
  }
};
inline Status nextStage(Status state) {
  if (state == Status::Accepted) return Status::Coming;
  return Status::Fulfilled;
}
} // namespace service
