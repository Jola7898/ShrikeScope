// WaveformView
// Draws the grid and the trace. Everything goes into an off-screen sprite
// first and then gets sent to the display in one go, so there's no flicker
// and no leftover pixels from the last frame.
//
// The sprite is 8-bit colour (320x200 = 64 KB). A 16-bit one would need
// 128 KB, which is a lot on a board with no PSRAM.
#pragma once

#include <TFT_eSPI.h>

struct VerticalScale {
  float voltsPerDiv = 0.5f;
  float voltsAtBottom = -0.5f;  // puts 0 V one division up from the bottom edge

  // How to turn a raw reading into volts. These get set from the ADC.
  float voltsPerCode = 3.3f / 4095;
  float voltsOffset = 0;
};

class WaveformView {
 public:
  static const int DIVS_X = 10;
  static const int DIVS_Y = 8;

  explicit WaveformView(TFT_eSPI &tft) : tft_(tft), sprite_(&tft) {}

  // Where the plot goes on screen. Returns false if there wasn't enough
  // memory for the sprite.
  bool begin(int x, int y, int w, int h);

  int width() const { return w_; }
  int height() const { return h_; }

  // One sample per pixel column. Pass triggerX = -1 if nothing triggered.
  void render(const uint16_t *samples, size_t n, const VerticalScale &scale,
              int triggerX, uint16_t triggerLevelCode);

 private:
  int codeToY(float code, const VerticalScale &scale) const;
  void drawGrid();

  TFT_eSPI &tft_;
  TFT_eSprite sprite_;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
};
