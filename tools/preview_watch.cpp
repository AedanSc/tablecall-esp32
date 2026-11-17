// Renders the real firmware UI using the real Adafruit classic font, without hardware.
#include "watch_display.h"
#include "glcdfont.c"
#include <stdarg.h>
#include <assert.h>
#include <fstream>
#include <string>
using namespace service;

struct PreviewCanvas {
  uint8_t pixels[64][128]{};
  int x = 0, y = 0, clipped = 0, scale = 1;
  uint16_t ink = 1;
  void clearDisplay() { memset(pixels, 0, sizeof(pixels)); clipped = 0; }
  void setTextWrap(bool) {}
  void setTextSize(int value) { scale = value; }
  void setTextColor(uint16_t color) { ink = color; }
  void setCursor(int xx, int yy) { x = xx; y = yy; }
  void display() {}
  void drawPixel(int xx, int yy, uint16_t color) {
    if (xx < 0 || xx >= 128 || yy < 0 || yy >= 64) { ++clipped; return; }
    pixels[yy][xx] = color ? 255 : 0;
  }
  void fillRect(int xx, int yy, int w, int h, uint16_t color) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) drawPixel(xx+i, yy+j, color);
  }
  void drawLine(int ax, int ay, int bx, int by, uint16_t color) {
    const int dx = abs(bx-ax), sx = ax < bx ? 1 : -1;
    const int dy = -abs(by-ay), sy = ay < by ? 1 : -1;
    int error = dx + dy;
    while (true) {
      drawPixel(ax, ay, color); if (ax == bx && ay == by) break;
      const int doubled = 2 * error;
      if (doubled >= dy) { error += dy; ax += sx; }
      if (doubled <= dx) { error += dx; ay += sy; }
    }
  }
  void drawCircle(int xx, int yy, int radius, uint16_t color) {
    int ax = 0, ay = radius, decision = 1-radius;
    while (ax <= ay) {
      const int points[8][2] = {{ax,ay},{-ax,ay},{ax,-ay},{-ax,-ay},{ay,ax},{-ay,ax},{ay,-ax},{-ay,-ax}};
      for (const auto &p : points) drawPixel(xx+p[0], yy+p[1], color);
      ++ax;
      if (decision < 0) decision += 2*ax+1;
      else { --ay; decision += 2*(ax-ay)+1; }
    }
  }
  void print(const char *value) {
    while (*value) {
      const uint8_t character = static_cast<uint8_t>(*value++);
      for (int column = 0; column < 5; ++column) {
        const uint8_t bits = font[character*5+column];
        for (int row = 0; row < 8; ++row)
          if (bits & (1U << row)) fillRect(x+column*scale, y+row*scale, scale, scale, ink);
      }
      x += 6*scale;
    }
  }
  void printf(const char *format, ...) {
    char buffer[128]; va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); print(buffer);
  }
  void save(const std::string &path) {
    assert(clipped == 0);
    // 24-bit BMP; the PowerShell contact sheet converts these to a PNG.
    unsigned char header[54] = {'B','M'};
    const uint32_t size = 54+128*64*3, offset=54, dib=40, width=128, height=64;
    memcpy(header+2,&size,4); memcpy(header+10,&offset,4); memcpy(header+14,&dib,4);
    memcpy(header+18,&width,4); memcpy(header+22,&height,4); header[26]=1; header[28]=24;
    std::ofstream output(path, std::ios::binary); output.write(reinterpret_cast<char *>(header),54);
    for (int yy = 63; yy >= 0; --yy) for (int xx = 0; xx < 128; ++xx) {
      const char value = static_cast<char>(pixels[yy][xx]);
      const char bgr[3] = {value,value,value}; output.write(bgr,3);
    }
    assert(output.good());
  }
};
Packet sample(uint16_t table, Request type) {
  Packet p{}; p.table=table; p.request=type; p.sequence=uint8_t(type)+1;
  p.sender[0]=uint8_t(table); return p;
}
int main() {
  assert(strcmp(watchRequestLabel(Request::Waiter), "Check") == 0);
  assert(strcmp(watchRequestLabel(Request::Water), "Waiter") == 0);
  assert(strcmp(watchRequestLabel(Request::Check), "Water") == 0);
  PreviewCanvas canvas; WatchQueue queue;
  drawWatch(canvas, queue, 135000); canvas.save(".pio/watch-idle.bmp");
  queue.add(sample(1,Request::Check),0); queue.add(sample(1,Request::Waiter),10000);
  queue.add(sample(3,Request::Water),87000); queue.add(sample(5,Request::Check),123000);
  queue.add(sample(7,Request::Waiter),130000);
  drawWatch(canvas, queue, 135000); canvas.save(".pio/watch-new.bmp");
  queue.advance(135000); drawWatch(canvas, queue, 135000); canvas.save(".pio/watch-coming.bmp");
  queue.advance(135000); drawWatch(canvas, queue, 135000); canvas.save(".pio/watch-sync.bmp");
  // Edge-case UI validation: full queue, maximum table ID, longest timer, all labels.
  WatchQueue edge;
  edge.add(sample(65535,Request::Waiter),0); edge.add(sample(65535,Request::Water),0);
  edge.add(sample(65535,Request::Check),0);
  for (uint16_t table=1; edge.count < WatchQueue::CAPACITY; ++table) edge.add(sample(table,Request::Water),0);
  edge.advance(0); drawWatch(canvas, edge, UINT32_MAX); assert(canvas.clipped == 0);
  // All table IDs and request combinations must remain on-screen.
  for (uint16_t table : {uint16_t(9), uint16_t(10), uint16_t(99), uint16_t(100)}) {
    for (uint8_t mask = 1; mask < 8; ++mask) {
      WatchQueue fit;
      for (uint8_t type = 0; type < 3; ++type)
        if (mask & (1U << type)) fit.add(sample(table, static_cast<Request>(type)), 0);
      drawWatch(canvas, fit, UINT32_MAX); assert(canvas.clipped == 0);
    }
  }
  puts("Rendered four UI states; all screen-boundary checks passed.");
}
