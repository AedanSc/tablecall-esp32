#include "watch_queue.h"
#include <assert.h>
using namespace service;
Packet makeRequest(uint16_t table, uint32_t sequence, Request type) {
  Packet p{}; p.magic = MAGIC; p.version = VERSION; p.kind = Kind::Request;
  p.table = table; p.session = 1; p.sequence = sequence; p.request = type;
  p.status = Status::Accepted; p.sender[0] = uint8_t(table); return p;
}
Packet ack(Packet p, Status state) { p.kind = Kind::StateAck; p.status = state; return p; }
int main() {
  WatchQueue q;
  const Packet water = makeRequest(1, 1, Request::Water);
  const Packet other = makeRequest(3, 1, Request::Waiter);
  const Packet check = makeRequest(1, 2, Request::Check);
  assert(!q.advance(0));
  assert(q.add(water, 1000)); assert(q.add(other, 2000)); assert(q.add(check, 3000));
  assert(q.groupCount == 2 && q.count == 3);
  assert(q.groups[0].table == 1 && q.groups[1].table == 3);
  assert(q.groups[0].arrived == 1000 && q.requestMask(0) == 6);
  assert(!q.add(water, 4000) && q.count == 3); // duplicate doesn't reset timer
  assert(q.advance(5000));
  assert(q.entries[0].state == Status::Coming && q.entries[2].state == Status::Coming);
  assert(q.entries[1].state == Status::Accepted); // only front table affected
  assert(q.acknowledge(ack(water, Status::Coming)) && q.count == 3);
  // Late additions before completion join the active visit and inherit Coming.
  const Packet service = makeRequest(1, 3, Request::Waiter);
  assert(q.add(service, 6000) && q.entries[3].state == Status::Coming);
  assert(q.requestMask(0) == 7);
  assert(q.advance(7000) && q.groups[0].state == Status::Fulfilled);
  assert(!q.advance(7001)); // can't accidentally advance another table while syncing
  assert(!q.acknowledge(ack(check, Status::Coming))); // stale ACK can't complete
  assert(q.acknowledge(ack(check, Status::Fulfilled)) && q.groupCount == 2);
  assert(q.acknowledge(ack(water, Status::Fulfilled)) && q.groupCount == 2);
  // Request made AFTER the completion press starts a fresh visit at the back.
  const Packet again = makeRequest(1, 4, Request::Water);
  assert(q.add(again, 8000) && q.groupCount == 3);
  assert(q.groups[2].table == 1 && q.groups[2].state == Status::Accepted);
  assert(q.acknowledge(ack(service, Status::Fulfilled)));
  assert(q.count == 2 && q.groupCount == 2 && q.groups[0].table == 3);
  assert(q.groups[1].table == 1 && q.entries[q.find(again)].state == Status::Accepted);
  assert(!q.acknowledge(ack(water, Status::Fulfilled))); // removed old visit
  assert(q.advance(9000) && q.advance(10000));
  assert(q.acknowledge(ack(other, Status::Fulfilled)) && q.groups[0].table == 1);
  assert(q.advance(11000) && q.advance(12000));
  assert(q.acknowledge(ack(again, Status::Fulfilled)) && !q.count && !q.groupCount);
  // Capacity must not overwrite queued requests or create phantom groups.
  for (uint8_t i = 0; i < WatchQueue::CAPACITY; ++i)
    assert(q.add(makeRequest(i + 1, 1, Request::Water), i * 1000));
  assert(!q.add(makeRequest(99, 1, Request::Check), 30000));
  assert(q.groupCount == 20 && q.count == 20 && q.groups[0].table == 1);
  char age[8];
  formatAge(48000, age, sizeof(age)); assert(strcmp(age, "48s") == 0);
  formatAge(135000, age, sizeof(age)); assert(strcmp(age, "2m15s") == 0);
  formatAge(3900000, age, sizeof(age)); assert(strcmp(age, "1h05m") == 0);
  formatAge(183600000, age, sizeof(age)); assert(strcmp(age, "2d03h") == 0);
  formatAge(uint32_t(500 - uint32_t(0xFFFFFE0C)), age, sizeof(age));
  assert(strcmp(age, "1s") == 0); // millisecond counter rollover
  formatAge(UINT32_MAX, age, sizeof(age));
  char line[32]; snprintf(line, sizeof(line), ">T65535 Coming %s", age);
  assert(strlen(line) <= 21); // 21 characters at 6 px fit the 128-pixel screen
  snprintf(line, sizeof(line), "Tables:%u +%u more", 20U, 17U); assert(strlen(line) <= 21);
  assert(strlen("Waiter+Water+Check") <= 21 && strlen("Press: fulfilled") <= 21);
  puts("Grouped watch queue and timer tests passed.");
}
