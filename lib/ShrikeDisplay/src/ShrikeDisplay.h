// ShrikeDisplay
// The screen code from ShrikeTFT, moved into its own library so other
// projects can use it. Pins and driver settings come from display.ini.
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace shrike {

class Display {
 public:
  // Starts the panel and turns the backlight on.
  // Rotation 1 is landscape (320x240), 0 is portrait.
  void begin(uint8_t rotation = 1);

  // Red, green, blue, white, black. If the colours look wrong, check the
  // inversion and RGB order flags in display.ini.
  void colourTest(uint16_t msPerColour = 300);

  // Title, resolution and wiring, shown for a couple of seconds at boot.
  void splash(const char *title, uint32_t holdMs = 2500);

  // A row of text fields across the screen. First one is left aligned, last
  // one right aligned, anything in between is centred. The text is padded
  // with the background colour, so redrawing it doesn't flicker.
  void drawBar(int y, int h, uint16_t bg, uint16_t fg,
               const char *const *fields, uint8_t count, uint8_t font = 2);

  // Paint a bar's background once before you start drawing text on it.
  void clearBar(int y, int h, uint16_t bg);

  int width() { return tft_.width(); }
  int height() { return tft_.height(); }

  // For everything else (sprites, shapes, ...)
  TFT_eSPI &tft() { return tft_; }

 private:
  TFT_eSPI tft_;
};

// Call tick() every frame. It returns true roughly once a second, when
// fps() has a new value.
class FpsCounter {
 public:
  bool tick();
  float fps() const { return fps_; }

 private:
  uint32_t frames_ = 0;
  uint32_t lastMs_ = 0;
  float fps_ = 0;
};

}  // namespace shrike
