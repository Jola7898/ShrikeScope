#include "DmaAdc.h"

#include <driver/adc.h>

// Each result is 4 bytes on the S3, so a 256 byte chunk is 64 samples.
static const uint32_t CHUNK_BYTES = 256;
static const uint32_t BYTES_PER_SAMPLE = SOC_ADC_DIGI_DATA_BYTES_PER_CONV;

// The driver's buffer. It fills up and overflows while a frame is being
// drawn, but that's fine, capture() throws the old stuff away anyway.
static const uint32_t RING_BYTES = 4096;

static const adc_atten_t ATTEN = ADC_ATTEN_DB_12;  // gives the full ~0-3.1 V range

DmaAdc::DmaAdc(uint8_t pin, uint32_t sampleRateHz) : pin_(pin), rate_(sampleRateHz) {
  snprintf(name_, sizeof(name_), "ADC IO%u", pin);
}

bool DmaAdc::begin() {
  // On the S3, GPIO n is ADC1 channel n-1
  if (pin_ < 1 || pin_ > 10) {
    Serial.printf("DmaAdc: IO%u isn't an ADC1 pin\n", pin_);
    return false;
  }
  channel_ = pin_ - 1;

  adc_digi_init_config_t init = {};
  init.max_store_buf_size = RING_BYTES;
  init.conv_num_each_intr = CHUNK_BYTES;
  init.adc1_chan_mask = BIT(channel_);
  init.adc2_chan_mask = 0;

  esp_err_t err = adc_digi_initialize(&init);
  if (err != ESP_OK) {
    Serial.printf("DmaAdc: adc_digi_initialize failed (%s)\n", esp_err_to_name(err));
    return false;
  }

  calibrate();
  return configure();
}

bool DmaAdc::configure() {
  // Can't change the rate while it's running
  if (running_) {
    adc_digi_stop();
    running_ = false;
  }

  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ATTEN;
  pattern.channel = channel_;
  pattern.unit = 0;  // ADC1
  pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_digi_configuration_t cfg = {};
  cfg.conv_limit_en = false;
  cfg.pattern_num = 1;
  cfg.adc_pattern = &pattern;
  cfg.sample_freq_hz = rate_;
  cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

  esp_err_t err = adc_digi_controller_configure(&cfg);
  if (err != ESP_OK) {
    Serial.printf("DmaAdc: configure failed (%s)\n", esp_err_to_name(err));
    return false;
  }

  err = adc_digi_start();
  if (err != ESP_OK) {
    Serial.printf("DmaAdc: start failed (%s)\n", esp_err_to_name(err));
    return false;
  }

  running_ = true;
  Serial.printf("DmaAdc: IO%u at %u S/s\n", pin_, rate_);
  return true;
}

bool DmaAdc::setSampleRate(uint32_t hz) {
  rate_ = constrain(hz, MIN_RATE, MAX_RATE);
  return configure();
}

void DmaAdc::calibrate() {
  // esp_adc_cal knows how this particular chip's ADC is off (it's stored in
  // eFuse at the factory). It might use a curve rather than a straight line,
  // so I just ask it for two points and draw a line through them. Keeps the
  // rest of the scope simple: volts = code * gain + offset.
  esp_adc_cal_characteristics_t chars;
  esp_adc_cal_value_t type =
      esp_adc_cal_characterize(ADC_UNIT_1, ATTEN, ADC_WIDTH_BIT_12, 1100, &chars);

  uint32_t lo = 400, hi = 3600;
  float mvLo = esp_adc_cal_raw_to_voltage(lo, &chars);
  float mvHi = esp_adc_cal_raw_to_voltage(hi, &chars);

  gain_ = (mvHi - mvLo) / (hi - lo) / 1000.0f;
  offset_ = mvLo / 1000.0f - gain_ * lo;

  const char *source = "unknown";
  switch (type) {
    case ESP_ADC_CAL_VAL_EFUSE_VREF:   source = "eFuse Vref"; break;
    case ESP_ADC_CAL_VAL_EFUSE_TP:     source = "eFuse two-point"; break;
    case ESP_ADC_CAL_VAL_DEFAULT_VREF: source = "default Vref"; break;
    case ESP_ADC_CAL_VAL_EFUSE_TP_FIT: source = "eFuse two-point + fit"; break;
    default: break;
  }
  Serial.printf("DmaAdc: calibration from %s, %.3f mV/code, offset %.1f mV\n",
                source, gain_ * 1000.0f, offset_ * 1000.0f);
}

void DmaAdc::flush() {
  // Empty the driver's buffer. The loop is capped just in case data keeps
  // arriving as fast as I can read it.
  uint8_t scratch[CHUNK_BYTES];
  uint32_t got = 0;
  for (int i = 0; i < 64; i++) {
    esp_err_t err = adc_digi_read_bytes(scratch, sizeof(scratch), &got, 0);
    if (err == ESP_ERR_TIMEOUT || got == 0) break;
  }
}

size_t DmaAdc::capture(uint16_t *buf, size_t n) {
  if (!running_) return 0;

  // Whatever's sitting in the buffer is from before (or during) the last
  // frame, and there may be gaps where it overflowed. Bin it and start
  // fresh so the whole capture is one continuous piece of signal.
  flush();

  // Wait long enough for about 3 chunks to arrive, plus a bit of slack
  uint32_t timeoutMs = 3 * (CHUNK_BYTES / BYTES_PER_SAMPLE) * 1000 / rate_ + 20;

  uint8_t chunk[CHUNK_BYTES];
  size_t count = 0;

  while (count < n) {
    uint32_t got = 0;
    esp_err_t err = adc_digi_read_bytes(chunk, sizeof(chunk), &got, timeoutMs);

    // INVALID_STATE just means the buffer overflowed at some point earlier.
    // The data we got back is still good.
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) break;

    for (uint32_t i = 0; i + BYTES_PER_SAMPLE <= got && count < n; i += BYTES_PER_SAMPLE) {
      auto *r = reinterpret_cast<const adc_digi_output_data_t *>(&chunk[i]);
      if (r->type2.unit != 0 || r->type2.channel != channel_) continue;  // not ours
      buf[count++] = r->type2.data;
    }
  }
  return count;
}
