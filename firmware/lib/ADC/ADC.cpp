#include "ADC.h"

// --- static member ---
ADC *ADC::_instance = nullptr;

ADC::ADC(SPIClass &spi, uint8_t csPin)
    : _dev(spi, csPin) {}

void ADC::begin(uint8_t range)
{
    _instance = this;
    _range = range;
    _dev.begin();

    // 全チャネルのレンジを設定
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    {
        _dev.writeRegister(ADS8688Reg::RANGE_CH0 + i, _range);
    }
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

uint16_t ADC::rawValue(uint8_t ch) const
{
    if (ch >= NUM_CHANNELS)
        return 0;
    return _raw[ch];
}

float ADC::voltage(uint8_t ch) const
{
    if (ch >= NUM_CHANNELS)
        return 0.0f;
    return toVoltage(_raw[ch]);
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
        float v = toVoltage(_raw[ch]);
        len += snprintf(buf + len, sizeof(buf) - len, ",%.4f", v);
    }

    buf[len++] = '\n';
    buf[len] = '\0';

    out.write(buf, len);
}

// --- Voltage conversion ---
float ADC::toVoltage(uint16_t raw) const
{
    // Full-scale voltage for each range setting
    float fullScale;
    switch (_range)
    {
    case ADS8688Range::BIPOLAR_2_5xVREF:
        fullScale = 10.24f;
        break; // ±10.24V
    case ADS8688Range::BIPOLAR_1_25xVREF:
        fullScale = 5.12f;
        break; // ±5.12V
    case ADS8688Range::BIPOLAR_0_625xVREF:
        fullScale = 2.56f;
        break; // ±2.56V
    case ADS8688Range::UNIPOLAR_2_5xVREF:
        fullScale = 10.24f;
        break; // 0~10.24V
    case ADS8688Range::UNIPOLAR_1_25xVREF:
        fullScale = 5.12f;
        break; // 0~5.12V
    default:
        fullScale = 10.24f;
        break;
    }

    if (_range <= 0x02)
    {
        // Bipolar: offset binary (0x8000 = 0V)
        return (static_cast<float>(raw) - 32768.0f) * (fullScale / 32768.0f);
    }
    else
    {
        // Unipolar: 0x0000 = 0V, 0xFFFF = full scale
        return static_cast<float>(raw) * (fullScale / 65536.0f);
    }
}

// --- ISR callback ---
void ADC::_onTimer()
{
    if (_instance)
        _instance->_sampleFlag = true;
}
