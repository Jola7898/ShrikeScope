// DmaAdc
// Reads the S3's ADC1 in continuous mode. The ADC's own controller decides
// when each sample is taken and DMA drops the results into RAM, so the timing
// is exact and the CPU doesn't have to babysit it. Same trick I used on the
// STM32 scope (timer -> ADC -> DMA).
//
// The catch: the IDF 4.4 driver won't go above 83.3 kS/s. Good enough for
// a few kHz, anything faster needs the external ADC.
#pragma once

#include <esp_adc_cal.h>

#include "AdcSource.h"

class DmaAdc : public AdcSource {
 public:
  static constexpr uint32_t MIN_RATE = 611;
  static constexpr uint32_t MAX_RATE = 83333;

  // Needs an ADC1 pin (IO1-IO10). IO1-IO7 are free on the Shrike-Fi's left
  // header. ADC2 is out because it fights with WiFi.
  explicit DmaAdc(uint8_t pin = 1, uint32_t sampleRateHz = MAX_RATE);

  const char *name() const override { return name_; }
  bool begin() override;
  size_t capture(uint16_t *buf, size_t n) override;
  float sampleRateHz() const override { return rate_; }
  uint8_t bits() const override { return 12; }
  float fullScaleVolts() const override { return 3.1f; }  // roughly, at 12 dB attenuation

  // From the chip's factory calibration
  float voltsPerCode() const override { return gain_; }
  float voltsOffset() const override { return offset_; }

  bool setSampleRate(uint32_t hz);

 private:
  bool configure();
  void flush();
  void calibrate();

  uint8_t pin_;
  uint8_t channel_ = 0;
  uint32_t rate_;
  bool running_ = false;
  char name_[12];
  float gain_ = 3.1f / 4095;
  float offset_ = 0;
};
