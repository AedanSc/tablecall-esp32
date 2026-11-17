#pragma once
#include "request_protocol.h"
#include <stdio.h>

namespace service {
// Hardware-independent queue, used by the OLED firmware and native tests.
struct WatchQueue {
  static constexpr uint8_t CAPACITY = 20;
  struct Entry {
    Packet packet;
    uint32_t group;
    Status state;
    bool synced;
    uint32_t retryAt;
  };
  struct Group {
    uint32_t id;
    uint16_t table;
    uint32_t arrived;
    Status state;
  };
  Entry entries[CAPACITY]{};
  Group groups[CAPACITY]{};
  uint8_t count = 0, groupCount = 0;
  uint32_t nextGroup = 1;

  int find(const Packet &p) const {
    for (uint8_t i = 0; i < count; ++i) if (sameRequest(entries[i].packet, p)) return i;
    return -1;
  }
  bool add(const Packet &p, uint32_t now) {
    if (count == CAPACITY || find(p) >= 0) return false;
    uint8_t group = 0;
    while (group < groupCount &&
           (groups[group].table != p.table || groups[group].state == Status::Fulfilled)) ++group;
    // A request made after the completion press starts a NEW visit at the back.
    if (group == groupCount) {
      groups[groupCount++] = {nextGroup++, p.table, now, Status::Accepted};
    }
    const Status state = groups[group].state;
    entries[count++] = {p, groups[group].id, state, state == Status::Accepted, now};
    return true;
  }
  bool advance(uint32_t now) {
    if (!groupCount || groups[0].state == Status::Fulfilled) return false;
    Group &group = groups[0];
    group.state = nextStage(group.state);
    for (uint8_t i = 0; i < count; ++i) if (entries[i].group == group.id) {
      entries[i].state = group.state; entries[i].synced = false; entries[i].retryAt = now;
    }
    return true;
  }
  bool acknowledge(const Packet &p) {
    const int index = find(p);
    if (index < 0 || p.kind != Kind::StateAck || !isProgress(p.status) ||
        entries[index].state != p.status) return false;
    entries[index].synced = true;
    if (!groupCount || groups[0].state != Status::Fulfilled) return true;
    const uint32_t group = groups[0].id;
    for (uint8_t i = 0; i < count; ++i)
      if (entries[i].group == group && !entries[i].synced) return true;
    // Remove the completed table atomically, even when its ACKs arrive out of order.
    uint8_t remaining = 0;
    for (uint8_t i = 0; i < count; ++i)
      if (entries[i].group != group) entries[remaining++] = entries[i];
    count = remaining;
    for (uint8_t i = 1; i < groupCount; ++i) groups[i - 1] = groups[i];
    --groupCount;
    return true;
  }
  uint8_t requestMask(uint8_t group) const {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < count; ++i)
      if (entries[i].group == groups[group].id) mask |= uint8_t(1U << uint8_t(entries[i].packet.request));
    return mask;
  }
};

inline void formatAge(uint32_t elapsedMs, char *buffer, size_t size) {
  const unsigned long seconds = elapsedMs / 1000;
  if (seconds < 60) snprintf(buffer, size, "%lus", seconds);
  else if (seconds < 3600) snprintf(buffer, size, "%lum%02lus", seconds / 60, seconds % 60);
  else if (seconds < 86400) snprintf(buffer, size, "%luh%02lum", seconds / 3600, seconds / 60 % 60);
  else snprintf(buffer, size, "%lud%02luh", seconds / 86400, seconds / 3600 % 24);
}
} // namespace service
