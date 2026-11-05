// Trigger and measurement maths. No hardware stuff in here, it just works on
// arrays of raw ADC readings.
#pragma once

#include <Arduino.h>

struct Measurements {
  uint16_t minCode = 0;
  uint16_t maxCode = 0;
  float meanCode = 0;
  float freqHz = 0;  // 0 if I couldn't find a repeating signal
};

// Finds a rising edge between from and to. The signal has to dip below
// (level - hysteresis) first, then come back up to level. The dip stops
// noise from causing false triggers. Returns -1 if there's no edge.
int findRisingEdge(const uint16_t *s, size_t n, uint16_t level, uint16_t hysteresis,
                   size_t from, size_t to);

// Min, max, average and frequency
Measurements analyse(const uint16_t *s, size_t n, float sampleRateHz);

// Little helpers for the screen text: "320us", "1.00kHz", "83.3kS/s"
void formatTime(char *out, size_t len, float seconds);
void formatFreq(char *out, size_t len, float hz);
void formatRate(char *out, size_t len, float samplesPerSec);
