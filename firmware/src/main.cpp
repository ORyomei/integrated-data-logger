#include <Arduino.h>
#include <SPI.h>
#include <ADC.h>
#include <pulse.h>
#include "constants.h"

// ============================================================
// Teensy 4.1 + ADS8688 Data Logger
// 8ch ±10V analog input @ 1kHz → USB CDC output
// 3ch square wave output on GPIO (FlexPWM)
// ============================================================

ADC adc(SPI, pin::ADC_CS);
Pulse pulse0(pin::PULSE_0, config::PULSE_0_FREQ_HZ);
Pulse pulse1(pin::PULSE_1, config::PULSE_1_FREQ_HZ);
Pulse pulse2(pin::PULSE_2, config::PULSE_2_FREQ_HZ);

void setup()
{
  Serial.begin(config::SERIAL_BAUD);

  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs < config::SERIAL_WAIT_MS))
  {
  }

  pulse0.begin();
  pulse1.begin();
  pulse2.begin();

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
