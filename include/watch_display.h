#pragma once
#include "watch_queue.h"

namespace service {
// Translate legacy wire-channel IDs to the physical button labels without
// changing any sender wiring, LED channel, packet identity, or ACK behavior.
inline const char *watchRequestLabel(Request request) {
  switch (request) {
    case Request::Waiter: return "Check";
    case Request::Water: return "Waiter";
    case Request::Check: return "Water";
  }
  return "?";
}

// Canvas is Adafruit_SSD1306 on hardware; the preview uses the same renderer.
template <typename Canvas>
void drawWatch(Canvas &screen, const WatchQueue &queue, uint32_t now) {
  screen.clearDisplay();
  screen.setTextWrap(false);
  screen.setTextSize(1);
  screen.setTextColor(1);

  // Service-bell mark, title, and real queue counts (no simulated battery meter).
  screen.drawLine(2, 5, 8, 5, 1);
  screen.drawLine(3, 2, 7, 2, 1);
  screen.drawLine(2, 3, 2, 4, 1);
  screen.drawLine(8, 3, 8, 4, 1);
  screen.drawPixel(5, 0, 1);
  screen.drawLine(1, 7, 9, 7, 1);
  screen.setCursor(13, 0); screen.print("TABLE CALL");
  char countLabel[12];
  if (!queue.count) snprintf(countLabel, sizeof(countLabel), "READY");
  else if (queue.groupCount > 3)
    snprintf(countLabel, sizeof(countLabel), "%uT +%u", queue.groupCount, queue.groupCount - 3);
  else snprintf(countLabel, sizeof(countLabel), "%u TABLE%s", queue.groupCount, queue.groupCount == 1 ? "" : "S");
  screen.setCursor(128 - int(strlen(countLabel)) * 6, 0); screen.print(countLabel);

  if (!queue.count) {
    // Small static smile, no animation or extra refresh traffic.
    screen.drawCircle(64, 26, 12, 1);
    screen.fillRect(59, 22, 2, 2, 1);
    screen.fillRect(67, 22, 2, 2, 1);
    screen.drawLine(59, 29, 61, 31, 1);
    screen.drawLine(61, 31, 67, 31, 1);
    screen.drawLine(67, 31, 69, 29, 1);
    screen.setCursor(25, 43); screen.print("ALL CAUGHT UP!");
    screen.setCursor(10, 55); screen.print("Ready for requests");
    screen.display();
    return;
  }

  for (uint8_t row = 0; row < queue.groupCount && row < 3; ++row) {
    const auto &group = queue.groups[row];
    const int y = 8 + row * 16;
    const uint16_t ink = row == 0 ? 0 : 1;
    if (row == 0) screen.fillRect(0, y, 128, 16, 1);
    else if (row > 1) screen.drawLine(2, y, 125, y, 1);
    screen.setTextColor(ink);
    screen.setCursor(3, y + 1); screen.printf("T%u", group.table);

    const char *state = group.state == Status::Accepted ? "NEW" :
                        group.state == Status::Coming ? "ON WAY" : "SYNC";
    // Inverse badge inside the highlighted front-table card.
    if (row == 0) {
      screen.fillRect(44, y, 39, 9, 0);
      screen.setTextColor(1);
    }
    screen.setCursor(45 + (36 - int(strlen(state)) * 6) / 2, y + 1);
    screen.print(state);
    screen.setTextColor(ink);
    char age[8]; formatAge(uint32_t(now - group.arrived), age, sizeof(age));
    screen.setCursor(126 - int(strlen(age)) * 6, y + 1); screen.print(age);

    screen.setCursor(3, y + 9);
    const uint8_t mask = queue.requestMask(row);
    const Request order[] = {Request::Water, Request::Check, Request::Waiter};
    bool separator = false;
    for (Request request : order) if (mask & (1U << uint8_t(request))) {
      if (separator) screen.print("+");
      screen.print(watchRequestLabel(request)); separator = true;
    }
  }

  screen.setTextColor(1);
  screen.drawLine(0, 56, 127, 56, 1);
  screen.drawCircle(5, 60, 2, 1);
  screen.setCursor(14, 57);
  if (queue.groups[0].state == Status::Accepted) screen.print("PRESS: I'M COMING");
  else if (queue.groups[0].state == Status::Coming) screen.print("PRESS: COMPLETE");
  else screen.print("SYNCING TABLE...");
  screen.display();
}
} // namespace service
