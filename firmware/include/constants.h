#pragma once

#include <cstdint>

// ============================================================
// constants.h - ピン割り当て・システム定数
// ============================================================

namespace pin
{
    constexpr uint8_t ADC_CS = 10; // LPSPI4_PCS0 (hardware CS)

    // 矩形波出力
    constexpr uint8_t SQUARE_WAVE = 2;
}

namespace config
{
    // USB CDC
    constexpr uint32_t SERIAL_BAUD = 115200;
    constexpr uint32_t SERIAL_WAIT_MS = 3000;

    // ADC サンプリング
    constexpr uint32_t ADC_SAMPLE_INTERVAL_US = 1000; // 1kHz

    // 矩形波出力
    constexpr float SQUARE_WAVE_FREQ_HZ = 60.0f;
}
