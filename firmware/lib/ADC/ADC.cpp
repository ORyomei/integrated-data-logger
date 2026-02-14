#include "ADC.h"

// --- static member ---
ADC *ADC::_instance = nullptr;

ADC::ADC(SPIClass &spi, uint8_t csPin)
    : _dev(spi, csPin) {}

void ADC::begin()
{
    _instance = this;
    _dev.begin();
}

void ADC::startSampling(uint32_t intervalUs)
{
    _sampleFlag = false;
    _timer.begin(_onTimer, intervalUs);
}

void ADC::stopSampling()
{
    _timer.end();
}

bool ADC::available() const
{
    return _sampleFlag;
}

void ADC::read()
{
    _sampleFlag = false;
    _dev.readAllChannels(_raw);
}

int16_t ADC::rawValue(uint8_t ch) const
{
    if (ch >= NUM_CHANNELS)
        return 0;
    return _raw[ch];
}

float ADC::voltage(uint8_t ch) const
{
    if (ch >= NUM_CHANNELS)
        return 0.0f;
    return ADS8688::toVoltage(_raw[ch]);
}

void ADC::printCSVHeader(Print &out) const
{
    out.print("time_us");
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
    {
        out.print(",ch");
        out.print(ch);
    }
    out.println();
}

void ADC::printCSVLine(Print &out, uint32_t timestampUs) const
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "%lu", (unsigned long)timestampUs);

    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++)
    {
        float v = ADS8688::toVoltage(_raw[ch]);
        len += snprintf(buf + len, sizeof(buf) - len, ",%.4f", v);
    }

    buf[len++] = '\n';
    buf[len] = '\0';

    out.write(buf, len);
}

// --- ISR callback ---
void ADC::_onTimer()
{
    if (_instance)
        _instance->_sampleFlag = true;
}
