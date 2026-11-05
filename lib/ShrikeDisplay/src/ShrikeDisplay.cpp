#include "ShrikeDisplay.h"

namespace shrike {

void Display::begin(uint8_t rotation) {
  tft_.init();
  tft_.setRotation(rotation);
  Serial.printf("display ready: %d x %d\n", tft_.width(), tft_.height());
}

void Display::colourTest(uint16_t msPerColour) {
  struct {
    uint16_t colour;
    const char *name;
  } steps[] = {
      {TFT_RED, "red"},     {TFT_GREEN, "green"}, {TFT_BLUE, "blue"},
      {TFT_WHITE, "white"}, {TFT_BLACK, "black"},
  };

  for (auto &step : steps) {
    Serial.printf("colour test: %s\n", step.name);
    tft_.fillScreen(step.colour);
    delay(msPerColour);
  }
}

void Display::splash(const char *title, uint32_t holdMs) {
  int centreX = tft_.width() / 2;
  char line[40];

  tft_.fillScreen(TFT_BLACK);
  tft_.setTextPadding(0);
  tft_.setTextDatum(MC_DATUM);

  tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.drawString(title, centreX, 40, 4);

  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  snprintf(line, sizeof(line), "%d x %d", tft_.width(), tft_.height());
  tft_.drawString(line, centreX, 75, 4);

  // The pin numbers come straight from the build flags, so if I rewire
  // something and update display.ini, this updates too.
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
  snprintf(line, sizeof(line), "SCK  -> IO%d   MOSI -> IO%d", TFT_SCLK, TFT_MOSI);
  tft_.drawString(line, 20, 105, 2);
  snprintf(line, sizeof(line), "MISO -> IO%d   CS   -> IO%d", TFT_MISO, TFT_CS);
  tft_.drawString(line, 20, 127, 2);
  snprintf(line, sizeof(line), "DC   -> IO%d   RST  -> IO%d", TFT_DC, TFT_RST);
  tft_.drawString(line, 20, 149, 2);
  snprintf(line, sizeof(line), "LED  -> IO%d   VCC  -> 3.3V", TFT_BL);
  tft_.drawString(line, 20, 171, 2);

  delay(holdMs);
}

void Display::clearBar(int y, int h, uint16_t bg) {
  tft_.fillRect(0, y, tft_.width(), h, bg);
}

void Display::drawBar(int y, int h, uint16_t bg, uint16_t fg,
                      const char *const *fields, uint8_t count, uint8_t font) {
  if (count == 0) return;

  int margin = 4;
  int fieldW = tft_.width() / count;
  int midY = y + h / 2;

  tft_.setTextColor(fg, bg);
  tft_.setTextPadding(fieldW - 2 * margin);  // wipes out leftovers from longer text

  for (uint8_t i = 0; i < count; i++) {
    int left = i * fieldW;
    if (i == 0) {
      tft_.setTextDatum(ML_DATUM);
      tft_.drawString(fields[i], left + margin, midY, font);
    } else if (i == count - 1) {
      tft_.setTextDatum(MR_DATUM);
      tft_.drawString(fields[i], left + fieldW - margin, midY, font);
    } else {
      tft_.setTextDatum(MC_DATUM);
      tft_.drawString(fields[i], left + fieldW / 2, midY, font);
    }
  }

  tft_.setTextPadding(0);
  tft_.setTextDatum(TL_DATUM);
}

bool FpsCounter::tick() {
  frames_++;
  uint32_t now = millis();

  if (lastMs_ == 0) {  // first frame, just start the clock
    lastMs_ = now;
    return false;
  }
  if (now - lastMs_ < 1000) return false;

  fps_ = frames_ * 1000.0f / (now - lastMs_);
  frames_ = 0;
  lastMs_ = now;
  return true;
}

}  // namespace shrike
