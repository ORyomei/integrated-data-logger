#pragma once

#include <Arduino.h>
#include <SPI.h>

// ============================================================
// ADS8688 - 16bit 8ch SAR ADC SPI Driver for Teensy 4.x
// Ported from working ESP32 implementation
// Datasheet: https://www.ti.com/jp/lit/ds/symlink/ads8688.pdf
// ============================================================

// --- Command Register (upper 16 bits of 32-bit frame) ---
namespace ADS8688Cmd
{
    constexpr uint16_t NO_OP = 0x0000;
    constexpr uint16_t AUTO_RST = 0xA000;
    constexpr uint16_t RST = 0x8500;
}

// --- Program Register Addresses ---
namespace ADS8688Reg
{
    constexpr uint8_t AUTO_SEQ_EN = 0x01;
    constexpr uint8_t CH_PWR_DN = 0x02;
    constexpr uint8_t RANGE_CH0 = 0x05;
    // CH1=0x06, CH2=0x07 ... CH7=0x0C
}

// --- Input Range Settings ---
namespace ADS8688Range
{
    constexpr uint8_t BIPOLAR_2_5xVREF = 0x00;   // ±2.5 × VREF = ±10.24V
    constexpr uint8_t BIPOLAR_1_25xVREF = 0x01;  // ±1.25 × VREF = ±5.12V
    constexpr uint8_t BIPOLAR_0_625xVREF = 0x02; // ±0.625 × VREF = ±2.56V
    constexpr uint8_t UNIPOLAR_2_5xVREF = 0x05;  // 0~2.5 × VREF = 0~10.24V
    constexpr uint8_t UNIPOLAR_1_25xVREF = 0x06; // 0~1.25 × VREF = 0~5.12V
}

class ADS8688
{
public:
    static constexpr uint8_t NUM_CHANNELS = 8;

    ADS8688(SPIClass &spi, uint8_t csPin)
        : _spi(spi), _csPin(csPin) {}

    // Initialize: configures hardware CS (LPSPI4_PCS0) and ADS8688 registers
    void begin()
    {
        // Pin 10 = GPIO_B0_00 → ALT3 = LPSPI4_PCS0 (hardware CS)
        *(portConfigRegister(_csPin)) = 3;

        // SPI_MODE1 (CPOL=0, CPHA=1), matching ESP32 code
        _spiSettings = SPISettings(5000000, MSBFIRST, SPI_MODE1);

        // Set all 8 channels range
        // ESP32: setReadRanges()
        for (uint8_t i = 0; i < NUM_CHANNELS; i++)
        {
            writeRegister(ADS8688Reg::RANGE_CH0 + i,
                          ADS8688Range::BIPOLAR_2_5xVREF);
        }

        // Enable channels & power on
        // ESP32: setReadChannels()
        writeRegister(ADS8688Reg::CH_PWR_DN, 0x00);   // Power on all
        writeRegister(ADS8688Reg::AUTO_SEQ_EN, 0xFF); // Enable all 8ch

        // Enter AUTO_RST mode
        // ESP32: setReadModeAutoSeq()
        transferCommand32(ADS8688Cmd::AUTO_RST);
    }

    // Read all 8 channels via auto-scan.
    // Matches ESP32 read() exactly:
    //   - N-1 channels with NO_OP
    //   - Last channel with AUTO_RST to restart sequence
    void readAllChannels(int16_t raw[NUM_CHANNELS])
    {
        uint32_t readVal;
        for (uint8_t i = 0; i < NUM_CHANNELS - 1; i++)
        {
            readVal = transferCommand32(ADS8688Cmd::NO_OP);
            raw[i] = static_cast<int16_t>(readVal >> 1);
        }
        // Last channel: send AUTO_RST to restart sequence
        readVal = transferCommand32(ADS8688Cmd::AUTO_RST);
        raw[NUM_CHANNELS - 1] = static_cast<int16_t>(readVal >> 1);
    }

    // Convert raw 16-bit value to voltage for ±10.24V range
    static float toVoltage(int16_t raw)
    {
        return static_cast<float>(raw) * (10.24f / 32768.0f);
    }

    // Write to a program register (24-bit frame, hardware CS)
    void writeRegister(uint8_t addr, uint8_t value)
    {
        _spi.beginTransaction(_spiSettings);

        // Set 24-bit frame size, keep mode/PCS bits
        IMXRT_LPSPI4_S.TCR = (IMXRT_LPSPI4_S.TCR & 0xFFFFF000) | LPSPI_TCR_FRAMESZ(23);

        // [addr<<1|W=1][value][0x00]
        uint32_t data = ((uint32_t)((addr << 1) | 0x01) << 16) | ((uint32_t)value << 8);
        IMXRT_LPSPI4_S.TDR = data;

        while (IMXRT_LPSPI4_S.RSR & LPSPI_RSR_RXEMPTY)
        {
        }
        (void)IMXRT_LPSPI4_S.RDR; // discard

        _spi.endTransaction();
        delayMicroseconds(2);
    }

    // 32-bit command frame: send 16-bit command + receive 16-bit data
    // Single 32-bit LPSPI frame with hardware CS
    uint32_t transferCommand32(uint16_t cmd)
    {
        _spi.beginTransaction(_spiSettings);

        // Set 32-bit frame size, keep mode/PCS bits
        IMXRT_LPSPI4_S.TCR = (IMXRT_LPSPI4_S.TCR & 0xFFFFF000) | LPSPI_TCR_FRAMESZ(31);

        IMXRT_LPSPI4_S.TDR = (uint32_t)cmd << 16;

        while (IMXRT_LPSPI4_S.RSR & LPSPI_RSR_RXEMPTY)
        {
        }
        uint32_t result = IMXRT_LPSPI4_S.RDR;

        _spi.endTransaction();
        delayMicroseconds(2);

        return result;
    }

private:
    SPIClass &_spi;
    uint8_t _csPin;
    SPISettings _spiSettings;
};
