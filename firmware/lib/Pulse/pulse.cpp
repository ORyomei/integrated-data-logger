#include "pulse.h"

Pulse *Pulse::_instance = nullptr;

Pulse::Pulse(uint8_t pin, float freqHz)
    : _pin(pin), _freqHz(freqHz) {}

void Pulse::begin()
{
    _instance = this;
    pinMode(_pin, OUTPUT);
    digitalWriteFast(_pin, LOW);
    // トグル周波数 = 2 × freqHz → 半周期 = 1000000 / (2 * freqHz) us
    _timer.begin(_onToggle, 1000000.0 / (2.0 * _freqHz));
}

void Pulse::stop()
{
    _timer.end();
    digitalWriteFast(_pin, LOW);
}

void Pulse::setFrequency(float freqHz)
{
    _freqHz = freqHz;
    _timer.update(1000000.0 / (2.0 * _freqHz));
}

void Pulse::_onToggle()
{
    if (_instance)
        digitalToggleFast(_instance->_pin);
}
