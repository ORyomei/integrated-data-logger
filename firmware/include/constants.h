#pragma once

#include <cstdint>

// ============================================================
// constants.h - ピン割り当て・システム定数
// ============================================================

namespace pin
{
    constexpr uint8_t ADC_CS = 10; // LPSPI4_PCS0 (hardware CS)

    // 矩形波出力 (FlexPWM 対応ピン, サブモジュール別)
    constexpr uint8_t PULSE_0 = 2; // FlexPWM4 SM2
    constexpr uint8_t PULSE_1 = 4; // FlexPWM2 SM0
    constexpr uint8_t PULSE_2 = 6; // FlexPWM2 SM2
}

namespace config
{
    // USB CDC
    constexpr uint32_t SERIAL_BAUD = 115200;
    constexpr uint32_t SERIAL_WAIT_MS = 3000;

    // ADC サンプリング
    constexpr uint32_t ADC_SAMPLE_INTERVAL_US = 1000; // 1kHz

    // 矩形波出力
    constexpr float PULSE_0_FREQ_HZ = 60.0f;  // ch0: 60Hz
    constexpr float PULSE_1_FREQ_HZ = 50.0f;  // ch1: 50Hz
    constexpr float PULSE_2_FREQ_HZ = 100.0f; // ch2: 100Hz
}
