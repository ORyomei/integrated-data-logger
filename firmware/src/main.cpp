#include <Arduino.h>
#include <SPI.h>
#include <ADC.h>
#include <pulse.h>
#include "constants.h"

// ============================================================
// Teensy 4.1 + ADS8688 Data Logger
// 8ch ±10V analog input @ 1kHz → USB CDC output
// 60Hz square wave output on GPIO
// ============================================================

ADC adc(SPI, pin::ADC_CS);
Pulse squareWave(pin::SQUARE_WAVE, config::SQUARE_WAVE_FREQ_HZ);

void setup()
{
  Serial.begin(config::SERIAL_BAUD);

  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs < config::SERIAL_WAIT_MS))
  {
  }

  squareWave.begin();

  SPI.begin();
  adc.begin();
  adc.printCSVHeader(Serial);
  adc.startSampling(config::ADC_SAMPLE_INTERVAL_US);
}

void loop()
{
  if (!adc.available())
    return;

  uint32_t t = micros();
  adc.read();
  adc.printCSVLine(Serial, t);
}