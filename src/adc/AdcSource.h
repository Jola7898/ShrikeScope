// AdcSource
// Anything that can give the scope samples. Right now that's just the S3's
// own ADC, but the plan is to plug in a proper high-speed ADC later, and the
// rest of the code shouldn't have to care which one it's talking to.
#pragma once

#include <Arduino.h>

class AdcSource {
 public:
  virtual ~AdcSource() = default;

  virtual const char *name() const = 0;  // shown in the status bar
  virtual bool begin() = 0;

  // Fill buf with n raw readings taken back to back at sampleRateHz().
  // Returns how many it actually got.
  virtual size_t capture(uint16_t *buf, size_t n) = 0;

  virtual float sampleRateHz() const = 0;
  virtual uint8_t bits() const = 0;
  virtual float fullScaleVolts() const = 0;

  uint16_t maxCode() const { return (1u << bits()) - 1; }

  // volts = code * voltsPerCode() + voltsOffset()
  // The defaults assume a perfect ADC. Override them if you have calibration data.
  virtual float voltsPerCode() const { return fullScaleVolts() / maxCode(); }
  virtual float voltsOffset() const { return 0; }
  float toVolts(float code) const { return code * voltsPerCode() + voltsOffset(); }
};
