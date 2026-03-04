#include "pulse.h"

Pulse::Pulse(uint8_t pin, float freqHz)
    : _pin(pin), _freqHz(freqHz) {}

void Pulse::begin()
{
    analogWriteFrequency(_pin, _freqHz);
    analogWrite(_pin, 128); // 50% duty
    _running = true;
}

void Pulse::stop()
{
    analogWrite(_pin, 0);
    pinMode(_pin, OUTPUT);
    digitalWriteFast(_pin, LOW);
    _running = false;
}

void Pulse::setFrequency(float freqHz)
{
    _freqHz = freqHz;
    analogWriteFrequency(_pin, _freqHz);
    if (_running)
    {
        analogWrite(_pin, 128); // 周波数変更後に duty を再設定
    }
}
