#include "Analysis.h"

int findRisingEdge(const uint16_t *s, size_t n, uint16_t level, uint16_t hysteresis,
                   size_t from, size_t to) {
  if (to > n) to = n;
  int armBelow = (int)level - (int)hysteresis;
  bool armed = false;

  // Start one sample early so an edge sitting right on `from` still counts
  size_t i = from > 0 ? from - 1 : 0;
  for (; i < to; i++) {
    if ((int)s[i] < armBelow) {
      armed = true;
    } else if (armed && s[i] >= level) {
      if (i >= from) return (int)i;
      armed = false;
    }
  }
  return -1;
}

Measurements analyse(const uint16_t *s, size_t n, float sampleRateHz) {
  Measurements m;
  if (n == 0) return m;

  uint32_t sum = 0;
  m.minCode = 0xFFFF;
  m.maxCode = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < m.minCode) m.minCode = s[i];
    if (s[i] > m.maxCode) m.maxCode = s[i];
    sum += s[i];
  }
  m.meanCode = (float)sum / n;

  // Basically flat, nothing to measure a frequency on
  uint16_t span = m.maxCode - m.minCode;
  if (span < 16) return m;

  // Frequency: find every rising edge through the middle, then
  // (number of whole cycles) / (time from first edge to last edge)
  uint16_t level = m.minCode + span / 2;
  uint16_t hyst = span / 10;

  int first = -1, last = -1, edges = 0;
  size_t pos = 0;
  while (pos < n) {
    int e = findRisingEdge(s, n, level, hyst, pos, n);
    if (e < 0) break;
    if (first < 0) first = e;
    last = e;
    edges++;
    pos = e + 1;
  }

  if (edges >= 2 && last > first) {
    m.freqHz = (edges - 1) * sampleRateHz / (last - first);
  }
  return m;
}

void formatTime(char *out, size_t len, float t) {
  if (t < 1e-6f)      snprintf(out, len, "%.0fns", t * 1e9f);
  else if (t < 1e-3f) snprintf(out, len, "%.3gus", t * 1e6f);
  else if (t < 1.0f)  snprintf(out, len, "%.3gms", t * 1e3f);
  else                snprintf(out, len, "%.3gs", t);
}

void formatFreq(char *out, size_t len, float hz) {
  if (hz <= 0)        snprintf(out, len, "-- Hz");
  else if (hz < 1e3f) snprintf(out, len, "%.1fHz", hz);
  else if (hz < 1e6f) snprintf(out, len, "%.2fkHz", hz / 1e3f);
  else                snprintf(out, len, "%.2fMHz", hz / 1e6f);
}

void formatRate(char *out, size_t len, float sps) {
  if (sps < 1e3f)      snprintf(out, len, "%.0fS/s", sps);
  else if (sps < 1e6f) snprintf(out, len, "%.3gkS/s", sps / 1e3f);
  else                 snprintf(out, len, "%.3gMS/s", sps / 1e6f);
}
