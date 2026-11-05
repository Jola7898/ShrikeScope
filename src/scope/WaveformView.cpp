#include "WaveformView.h"

static const uint16_t COLOUR_BG    = TFT_BLACK;
static const uint16_t COLOUR_GRID  = 0x3186;  // dark grey
static const uint16_t COLOUR_AXIS  = 0x6B4D;  // a bit lighter, for the centre lines
static const uint16_t COLOUR_TRACE = TFT_YELLOW;
static const uint16_t COLOUR_TRIG  = TFT_ORANGE;
static const uint16_t COLOUR_ZERO  = TFT_DARKCYAN;

bool WaveformView::begin(int x, int y, int w, int h) {
  x_ = x;
  y_ = y;
  w_ = w;
  h_ = h;

  sprite_.setColorDepth(8);
  if (sprite_.createSprite(w, h) == nullptr) {
    Serial.printf("WaveformView: not enough memory for a %dx%d sprite\n", w, h);
    return false;
  }
  return true;
}

int WaveformView::codeToY(float code, const VerticalScale &s) const {
  float volts = code * s.voltsPerCode + s.voltsOffset;
  float pxPerVolt = (float)h_ / (DIVS_Y * s.voltsPerDiv);

  int y = (h_ - 1) - (int)((volts - s.voltsAtBottom) * pxPerVolt + 0.5f);
  return constrain(y, 0, h_ - 1);  // pin it to the edge if it goes off screen
}

void WaveformView::drawGrid() {
  sprite_.fillSprite(COLOUR_BG);

  // Dotted lines for each division
  for (int d = 0; d <= DIVS_X; d++) {
    int x = d * (w_ - 1) / DIVS_X;
    for (int y = 0; y < h_; y += 4) sprite_.drawPixel(x, y, COLOUR_GRID);
  }
  for (int d = 0; d <= DIVS_Y; d++) {
    int y = d * (h_ - 1) / DIVS_Y;
    for (int x = 0; x < w_; x += 4) sprite_.drawPixel(x, y, COLOUR_GRID);
  }

  // Solid centre lines, with 5 small ticks per division like a real scope
  int cx = w_ / 2, cy = h_ / 2;
  sprite_.drawFastVLine(cx, 0, h_, COLOUR_AXIS);
  sprite_.drawFastHLine(0, cy, w_, COLOUR_AXIS);

  float tickX = (float)w_ / (DIVS_X * 5);
  float tickY = (float)h_ / (DIVS_Y * 5);
  for (float x = 0; x < w_; x += tickX) sprite_.drawFastVLine((int)x, cy - 2, 5, COLOUR_AXIS);
  for (float y = 0; y < h_; y += tickY) sprite_.drawFastHLine(cx - 2, (int)y, 5, COLOUR_AXIS);

  sprite_.drawRect(0, 0, w_, h_, COLOUR_AXIS);
}

void WaveformView::render(const uint16_t *samples, size_t n, const VerticalScale &scale,
                          int triggerX, uint16_t triggerLevelCode) {
  drawGrid();

  // Little arrow on the left showing where 0 V is
  int zeroY = codeToY(-scale.voltsOffset / scale.voltsPerCode, scale);
  sprite_.fillTriangle(0, zeroY - 4, 0, zeroY + 4, 5, zeroY, COLOUR_ZERO);

  // Trigger level arrow on the right, and the trigger point arrow on top
  int trigY = codeToY(triggerLevelCode, scale);
  sprite_.fillTriangle(w_ - 1, trigY - 4, w_ - 1, trigY + 4, w_ - 6, trigY, COLOUR_TRIG);
  if (triggerX >= 0 && triggerX < w_) {
    sprite_.fillTriangle(triggerX - 4, 0, triggerX + 4, 0, triggerX, 5, COLOUR_TRIG);
  }

  // The trace. Joining each point to the next with a line keeps sharp edges
  // visible instead of just leaving dots at the top and bottom.
  if (n > (size_t)w_) n = w_;
  if (n > 0) {
    int prevY = codeToY(samples[0], scale);
    for (size_t x = 1; x < n; x++) {
      int y = codeToY(samples[x], scale);
      sprite_.drawLine(x - 1, prevY, x, y, COLOUR_TRACE);
      prevY = y;
    }
  }

  sprite_.pushSprite(x_, y_);
}
