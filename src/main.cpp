// ShrikeScope
// A small oscilloscope on the Vicharak Shrike-Fi with a 2.4" ST7789 screen.
//
// For now it reads the S3's built-in ADC on IO1. Later on I'll hook up a
// proper high-speed ADC and stream the data over USB / WiFi.
//
// Screen layout (320x240, landscape):
//   top 20 px     status: input, time/div, volts/div, trigger, fps
//   middle 200 px the waveform, 10 x 8 divisions
//   bottom 20 px  measurements: Vpp, average, frequency, sample rate
//
// BOOT button changes the timebase. Type h in the serial monitor for the
// rest of the commands.

#include <Arduino.h>
#include <ShrikeDisplay.h>

#include "adc/DmaAdc.h"
#include "scope/Analysis.h"
#include "scope/WaveformView.h"

// Screen layout
const int BAR_H  = 20;
const int PLOT_Y = BAR_H;
const int PLOT_W = 320;
const int PLOT_H = 200;
const int MEAS_Y = PLOT_Y + PLOT_H;

const uint16_t BAR_BG = TFT_NAVY;
const uint16_t BAR_FG = TFT_WHITE;

// I grab two screens' worth of samples each time. That leaves room to
// look for a trigger point and still have a full screen around it.
const size_t CAPTURE_LEN = PLOT_W * 2;
const int TRIGGER_X = PLOT_W / 2;  // trigger point sits in the middle

const uint8_t ADC_PIN = 1;      // ESP_IO1 on the left header (ADC1 channel 0)
const uint8_t BOOT_BUTTON = 0;

// Sample rates for each timebase. 32 pixels per division, so
// time/div = 32 / rate. 83.3k is the most the driver allows.
const uint32_t RATES[] = {83333, 40000, 20000, 10000, 4000, 2000, 1000};
const int RATE_COUNT = sizeof(RATES) / sizeof(RATES[0]);
int rateIndex = 0;

shrike::Display display;
shrike::FpsCounter fps;
WaveformView view(display.tft());

DmaAdc adcInput(ADC_PIN, RATES[0]);
AdcSource *adc = &adcInput;

uint16_t samples[CAPTURE_LEN];
size_t sampleCount = 0;
VerticalScale vscale;
bool triggered = false;
Measurements meas;

volatile bool bootPressed = false;

// ---------------------------------------------------------------------------

// The button uses an interrupt because at the slow timebases one capture
// can take over half a second, and a quick press would get missed if I
// only checked it once per frame.
void IRAM_ATTR onBootButton() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 200) {  // debounce
    bootPressed = true;
    last = now;
  }
}

void drawStatusBar() {
  char timeDiv[16], voltDiv[16], right[20];

  formatTime(timeDiv, sizeof(timeDiv), PLOT_W / WaveformView::DIVS_X / adc->sampleRateHz());
  strncat(timeDiv, "/d", sizeof(timeDiv) - strlen(timeDiv) - 1);
  snprintf(voltDiv, sizeof(voltDiv), "%.2gV/d", vscale.voltsPerDiv);
  snprintf(right, sizeof(right), "%s %.0ffps", triggered ? "TRIG" : "AUTO", fps.fps());

  const char *fields[] = {adc->name(), timeDiv, voltDiv, right};
  display.drawBar(0, BAR_H, BAR_BG, BAR_FG, fields, 4);
}

void drawMeasurementBar() {
  char vpp[16], mean[16], freq[16], rate[16];

  // Vpp is a difference, so the offset cancels out and only the gain matters
  snprintf(vpp, sizeof(vpp), "Vpp %.2fV", (meas.maxCode - meas.minCode) * adc->voltsPerCode());
  snprintf(mean, sizeof(mean), "Avg %.2fV", adc->toVolts(meas.meanCode));
  formatFreq(freq, sizeof(freq), meas.freqHz);
  formatRate(rate, sizeof(rate), adc->sampleRateHz());

  const char *fields[] = {vpp, mean, freq, rate};
  display.drawBar(MEAS_Y, BAR_H, BAR_BG, TFT_GREENYELLOW, fields, 4);
}

void printHelp() {
  Serial.printf(
      "\nShrikeScope - input on ESP_IO%u (keep it between 0 and 3.1 V)\n"
      "  < / >   slower / faster timebase (the BOOT button does this too)\n"
      "  + / -   volts per division\n"
      "  d       print what's on screen as CSV\n"
      "  h       this help\n",
      ADC_PIN);
}

// Prints the samples that are on screen right now. Paste it into a
// spreadsheet or Python to plot it. Proper USB streaming comes later.
void dumpSamples(size_t start) {
  Serial.printf("# %s, %.0f S/s, %u samples\n", adc->name(), adc->sampleRateHz(), PLOT_W);
  Serial.println("index,code,volts");
  for (int i = 0; i < PLOT_W && start + i < sampleCount; i++) {
    uint16_t c = samples[start + i];
    Serial.printf("%d,%u,%.4f\n", i, c, adc->toVolts(c));
  }
}

void setTimebase(int index) {
  // wrap around in both directions
  rateIndex = (index % RATE_COUNT + RATE_COUNT) % RATE_COUNT;
  adcInput.setSampleRate(RATES[rateIndex]);
}

void stepVoltsPerDiv(int dir) {
  static const float steps[] = {0.05f, 0.1f, 0.2f, 0.5f, 1.0f};
  const int count = sizeof(steps) / sizeof(steps[0]);

  int idx = 0;
  for (int i = 0; i < count; i++) {
    if (steps[i] <= vscale.voltsPerDiv + 1e-6f) idx = i;
  }
  idx = constrain(idx + dir, 0, count - 1);
  vscale.voltsPerDiv = steps[idx];
}

void handleInput(size_t screenStart) {
  if (bootPressed) {
    bootPressed = false;
    setTimebase(rateIndex + 1);
  }

  while (Serial.available()) {
    switch (Serial.read()) {
      case '<': setTimebase(rateIndex + 1); break;  // lower sample rate = more time per division
      case '>': setTimebase(rateIndex - 1); break;
      case '+': stepVoltsPerDiv(+1); break;
      case '-': stepVoltsPerDiv(-1); break;
      case 'd': dumpSamples(screenStart); break;
      case 'h':
      case '?': printHelp(); break;
    }
  }
}

// Something went badly wrong. Put it on the screen and stop.
void fatal(const char *msg) {
  Serial.println(msg);
  display.tft().fillScreen(TFT_RED);
  display.tft().setTextColor(TFT_WHITE, TFT_RED);
  display.tft().drawString(msg, 10, 110, 2);
  while (true) delay(1000);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nShrikeScope starting");

  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(BOOT_BUTTON, onBootButton, FALLING);

  display.begin(1);
  display.colourTest();
  display.splash("ShrikeScope");

  if (!view.begin(0, PLOT_Y, PLOT_W, PLOT_H)) fatal("Out of memory for the plot");
  if (!adc->begin()) fatal("ADC didn't start, check serial");

  vscale.voltsPerCode = adc->voltsPerCode();
  vscale.voltsOffset = adc->voltsOffset();

  display.tft().fillScreen(TFT_BLACK);
  display.clearBar(0, BAR_H, BAR_BG);
  display.clearBar(MEAS_Y, BAR_H, BAR_BG);
  printHelp();
}

void loop() {
  // Grab a fresh block of samples
  sampleCount = adc->capture(samples, CAPTURE_LEN);
  if (sampleCount < (size_t)PLOT_W) {
    Serial.printf("capture came up short: %u samples\n", (unsigned)sampleCount);
    handleInput(0);
    return;
  }

  // Measure it, then look for a trigger. The trigger level sits halfway
  // between the lowest and highest reading, so it follows the signal.
  meas = analyse(samples, sampleCount, adc->sampleRateHz());
  uint16_t span = meas.maxCode - meas.minCode;
  uint16_t level = meas.minCode + span / 2;
  uint16_t hyst = max<uint16_t>(span / 10, 4);

  int edge = -1;
  if (sampleCount >= CAPTURE_LEN) {
    edge = findRisingEdge(samples, sampleCount, level, hyst, TRIGGER_X,
                          sampleCount - (PLOT_W - TRIGGER_X));
  }
  triggered = edge >= 0;

  // No trigger? Just show the start of the capture (free running).
  size_t start = triggered ? edge - TRIGGER_X : 0;

  view.render(samples + start, PLOT_W, vscale, triggered ? TRIGGER_X : -1, level);

  // Text is slow to draw, so the bars only update a few times a second
  static uint32_t lastBars = 0;
  bool newFps = fps.tick();
  if (newFps || millis() - lastBars > 250) {
    lastBars = millis();
    drawStatusBar();
    drawMeasurementBar();
  }

  handleInput(start);
}
