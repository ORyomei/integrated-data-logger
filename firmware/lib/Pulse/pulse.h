#pragma once

#include <Arduino.h>

// ============================================================
// Pulse - FlexPWM ハードウェアによる正確な矩形波デジタル出力
// 複数インスタンスが独立動作可能 (CPU 介入なし, ジッターゼロ)
// ============================================================

class Pulse
{
public:
    /// @brief コンストラクタ
    /// @param pin      出力 GPIO ピン (FlexPWM 対応ピンであること)
    /// @param freqHz   周波数 [Hz]
    Pulse(uint8_t pin, float freqHz);

    /// @brief 出力開始 (50% duty の矩形波)
    void begin();

    /// @brief 出力停止 (ピンを LOW にする)
    void stop();

    /// @brief 周波数を変更する
    /// @param freqHz 新しい周波数 [Hz]
    void setFrequency(float freqHz);

private:
    uint8_t _pin;
    float _freqHz;
    bool _running = false;
};
