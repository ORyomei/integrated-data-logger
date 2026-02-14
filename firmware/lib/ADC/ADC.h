#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <ADS8688.h>

// ============================================================
// ADC - ADS8688 を抽象化した高レベル ADC インターフェース
// 1kHz サンプリング、8ch ±10V アナログ入力
// ============================================================

class ADC
{
public:
    static constexpr uint8_t NUM_CHANNELS = ADS8688::NUM_CHANNELS;
    static constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_US = 1000; // 1kHz

    /// @brief コンストラクタ
    /// @param spi      使用する SPIClass (SPI, SPI1 等)
    /// @param csPin    チップセレクトピン
    ADC(SPIClass &spi, uint8_t csPin);

    /// @brief 初期化 (SPI.begin() は呼び出し元で行うこと)
    /// @param range  ADS8688Range:: のレンジ定数 (デフォルト: BIPOLAR_2_5xVREF)
    void begin(uint8_t range = ADS8688Range::BIPOLAR_2_5xVREF);

    /// @brief サンプリングタイマーを開始する
    /// @param intervalUs サンプリング間隔 [us] (デフォルト 1000 = 1kHz)
    void startSampling(uint32_t intervalUs = DEFAULT_SAMPLE_INTERVAL_US);

    /// @brief サンプリングタイマーを停止する
    void stopSampling();

    /// @brief 新しいサンプルが利用可能か確認する (ISR フラグベース)
    /// @return true: 新しいデータあり
    bool available() const;

    /// @brief 全チャネルを読み取り、内部バッファに格納する
    ///        available() が true のときに呼ぶ
    void read();

    /// @brief 指定チャネルの生データを取得する
    /// @param ch チャネル番号 (0–7)
    /// @return 16bit 生値
    uint16_t rawValue(uint8_t ch) const;

    /// @brief 指定チャネルの電圧値を取得する [V]
    /// @param ch チャネル番号 (0–7)
    /// @return 電圧値 (現在のレンジに応じた変換)
    float voltage(uint8_t ch) const;

    /// @brief CSV ヘッダ行を出力する
    /// @param out 出力先 Stream (Serial 等)
    void printCSVHeader(Print &out) const;

    /// @brief 現在のデータを CSV 行として出力する
    /// @param out        出力先 Stream (Serial 等)
    /// @param timestampUs タイムスタンプ [us]
    void printCSVLine(Print &out, uint32_t timestampUs) const;

private:
    ADS8688 _dev;
    IntervalTimer _timer;
    uint16_t _raw[NUM_CHANNELS] = {};
    uint8_t _range = ADS8688Range::BIPOLAR_2_5xVREF;
    volatile bool _sampleFlag = false;

    /// @brief 生値を電圧に変換する (現在のレンジに基づく)
    float toVoltage(uint16_t raw) const;

    static ADC *_instance; // ISR コールバック用
    static void _onTimer();
};
