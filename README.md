# ShrikeScope

A small oscilloscope built on the [Vicharak Shrike-Fi](https://vicharak.in)
(ESP32-S3 + FPGA) with a 2.4" ST7789 TFT.

For now it reads the ESP32-S3's built-in ADC on **IO1**. The plan is to add a
proper high-speed ADC later and send the readings over USB or WiFi.

## Features

- Live waveform on a 10 x 8 division grid
- Hardware-timed sampling (ADC + DMA), so the sample rate is exact
- 7 timebases, from 83.3 kS/s down to 1 kS/s (about 384 µs/div to 32 ms/div)
- Auto trigger on a rising edge
- Vpp, average voltage and frequency readouts
- Voltages calibrated from the chip's factory data
- Dumps the on-screen samples as CSV over serial

## Hardware

- Vicharak Shrike-Fi
- 2.4" ST7789 240x320 SPI display
- Jumper wires

![Shrike-Fi pinout](docs/shrike_fi_pinouts.jpg)

### Display wiring

| Display | Shrike-Fi |
|---------|-----------|
| VCC     | 3.3V      |
| GND     | GND       |
| SCK     | IO36      |
| MOSI    | IO35      |
| MISO    | IO37      |
| CS      | IO34      |
| DC      | IO33      |
| RST     | IO38      |
| LED     | IO18      |

### Scope input

Connect your signal to **IO1** and its ground to any **GND** pin.

> Keep the input between **0 and 3.1 V**. Anything negative or higher can
> damage the board.

If nothing is connected, the pin floats and the trace wanders around. That's
normal.

## Building

You need [PlatformIO](https://platformio.org/).

```bash
pio run -t upload
pio device monitor
```

## Controls

| Control       | What it does |
|---------------|--------------|
| BOOT button   | Next timebase |
| `<` / `>`     | Slower / faster timebase |
| `+` / `-`     | Volts per division |
| `d`           | Print the on-screen samples as CSV |
| `h`           | Help |

Type the letters in the serial monitor (115200 baud).

## Project layout

```
platformio.ini      board settings
display.ini         display pins and driver settings
lib/ShrikeDisplay/  screen helpers (taken from ShrikeTFT)
src/main.cpp        main loop: capture, trigger, draw
src/adc/            ADC code
src/scope/          trigger, measurements and drawing
docs/               board pinout
```

## Limitations

- The ESP32's ADC tops out at 83.3 kS/s, so it's only good for signals up to
  a few kHz.
- Only one channel for now.

## To do

- Hook up a high-speed ADC through the FPGA
- Stream the data over USB and WiFi
- Trigger level and edge controls
