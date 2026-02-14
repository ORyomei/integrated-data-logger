#include <Arduino.h>
#include <SPI.h>
#include <ADC.h>

// ============================================================
// Teensy 4.1 + ADS8688 Data Logger
// 8ch ±10V analog input @ 1kHz → USB CDC output
// ============================================================

constexpr uint8_t ADC_CS_PIN = 10;

ADC adc(SPI, ADC_CS_PIN);

void setup()
{
  Serial.begin(115200);

  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs < 3000))
  {
  }

  SPI.begin();
  adc.begin();
  adc.printCSVHeader(Serial);
  adc.startSampling(); // 1kHz
}

void loop()
{
  if (!adc.available())
    return;

  uint32_t t = micros();
  adc.read();
  adc.printCSVLine(Serial, t);
}