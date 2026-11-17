// Hardware-free regression tests for the actual protocol/lifecycle used by firmware.
#include "request_protocol.h"
#include <assert.h>
#include <stdio.h>
using namespace service;
Packet request(uint32_t sequence, Request type = Request::Water) {
  Packet p{}; p.magic = MAGIC; p.version = VERSION; p.kind = Kind::Request;
  p.table = 1; p.session = 42; p.sequence = sequence; p.request = type;
  p.status = Status::Accepted; p.sender[0] = 5; return p;
}
int main() {
  TableRequest slot;
  Packet p = request(1);
  assert(validPacket(p)); assert(slot.start(p)); assert(slot.active());
  assert(!slot.confirmed && slot.state == Status::Accepted); // solid immediately
  assert(!slot.start(request(2))); // repeated presses cannot replace active ID
  Packet full = p; full.kind = Kind::Ack; full.status = Status::Full;
  assert(!slot.apply(full) && !slot.confirmed); // queue full is not confirmation
  Packet coming = p; coming.kind = Kind::State; coming.status = Status::Coming;
  assert(slot.apply(coming) && slot.confirmed && slot.state == Status::Coming);
  Packet ack = p; ack.kind = Kind::Ack;
  assert(slot.apply(ack) && slot.state == Status::Coming); // late request ACK
  Packet done = coming; done.status = Status::Fulfilled;
  assert(slot.apply(done) && !slot.active());
  assert(slot.apply(coming) && slot.state == Status::Fulfilled); // reordered update
  assert(slot.apply(done) && !slot.active()); // duplicate completion
  assert(slot.start(request(2)) && slot.active());
  assert(!slot.apply(done) && slot.state == Status::Accepted); // old completion
  Packet other = request(2); other.kind = Kind::State; other.status = Status::Fulfilled;
  other.table = 2; assert(!slot.apply(other));
  other.table = 1; other.session = 43; assert(!slot.apply(other));
  other.session = 42; other.request = Request::Check; assert(!slot.apply(other));
  other.request = Request::Water; other.sender[0] = 6; assert(!slot.apply(other));
  // A lost coming update does not prevent direct terminal delivery.
  other.sender[0] = 5; assert(slot.apply(other) && !slot.active());
  TableRequest independent; assert(independent.start(request(3, Request::Check)));
  assert(independent.active() && !slot.active());
  assert(nextStage(Status::Accepted) == Status::Coming);
  assert(nextStage(Status::Coming) == Status::Fulfilled);
  p.version = 1; assert(!validPacket(p));
  p = request(1); p.kind = Kind::State; assert(!validPacket(p));
  p.status = Status::Coming; assert(validPacket(p));
  p.kind = Kind::StateAck; assert(validPacket(p));
  p.request = static_cast<Request>(3); assert(!validPacket(p));
  puts("Lifecycle regression tests passed.");
}
