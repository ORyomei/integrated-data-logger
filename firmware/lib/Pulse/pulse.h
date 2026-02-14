#pragma once

#include <Arduino.h>

// ============================================================
// Pulse - PIT タイマーによる正確な矩形波デジタル出力
// ============================================================

class Pulse
{
public:
    /// @brief コンストラクタ
    /// @param pin      出力 GPIO ピン
    /// @param freqHz   周波数 [Hz]
    Pulse(uint8_t pin, float freqHz);

    /// @brief 出力開始
    void begin();

    /// @brief 出力停止 (ピンを LOW にする)
    void stop();

    /// @brief 周波数を変更する
    /// @param freqHz 新しい周波数 [Hz]
    void setFrequency(float freqHz);

private:
    uint8_t _pin;
    float _freqHz;
    IntervalTimer _timer;

    static Pulse *_instance;
    static void _onToggle();
};
