/*
 * ADS8688 8-Channel 16-bit ADC Driver
 * SPI Interface for Texas Instruments ADS8688
 *
 * Features:
 *   - 8 channels, 16-bit resolution
 *   - ±10.24V, ±5.12V, ±2.56V input ranges
 *   - Up to 500kSPS (we use 10kHz per channel = 80kSPS total)
 */

#ifndef ADS8688_H
#define ADS8688_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

/* ADS8688 Command Registers (Program Register) */
#define ADS8688_CMD_NO_OP 0x0000
#define ADS8688_CMD_STDBY 0x8200
#define ADS8688_CMD_PWR_DN 0x8300
#define ADS8688_CMD_RST 0x8500
#define ADS8688_CMD_AUTO_RST 0xA000
#define ADS8688_CMD_MAN_CH0 0xC000
#define ADS8688_CMD_MAN_CH1 0xC400
#define ADS8688_CMD_MAN_CH2 0xC800
#define ADS8688_CMD_MAN_CH3 0xCC00
#define ADS8688_CMD_MAN_CH4 0xD000
#define ADS8688_CMD_MAN_CH5 0xD400
#define ADS8688_CMD_MAN_CH6 0xD800
#define ADS8688_CMD_MAN_CH7 0xDC00

/* ADS8688 Program Registers */
#define ADS8688_REG_AUTO_SEQ_EN 0x01
#define ADS8688_REG_CH_PWR_DN 0x02
#define ADS8688_REG_FEATURE_SEL 0x03
#define ADS8688_REG_CH0_INPUT_RANGE 0x05
#define ADS8688_REG_CH1_INPUT_RANGE 0x06
#define ADS8688_REG_CH2_INPUT_RANGE 0x07
#define ADS8688_REG_CH3_INPUT_RANGE 0x08
#define ADS8688_REG_CH4_INPUT_RANGE 0x09
#define ADS8688_REG_CH5_INPUT_RANGE 0x0A
#define ADS8688_REG_CH6_INPUT_RANGE 0x0B
#define ADS8688_REG_CH7_INPUT_RANGE 0x0C

/* Input Range Settings */
#define ADS8688_RANGE_PM_10V24 0x00 /* ±10.24V (default) */
#define ADS8688_RANGE_PM_5V12 0x01  /* ±5.12V */
#define ADS8688_RANGE_PM_2V56 0x02  /* ±2.56V */
#define ADS8688_RANGE_PM_1V28 0x03  /* ±1.28V */
#define ADS8688_RANGE_PM_0V64 0x04  /* ±0.64V */
#define ADS8688_RANGE_0_10V24 0x05  /* 0 to 10.24V */
#define ADS8688_RANGE_0_5V12 0x06   /* 0 to 5.12V */

/* Number of channels */
#define ADS8688_NUM_CHANNELS 8

/* ADS8688 driver context */
struct ads8688_dev {
  const struct device *spi_dev;
  struct spi_config spi_cfg;
  const struct device *cs_gpio_dev;
  gpio_pin_t cs_pin;
  uint8_t input_range[ADS8688_NUM_CHANNELS];
  bool initialized;
};

/**
 * Initialize ADS8688 device
 */
int ads8688_init(struct ads8688_dev *dev, const struct device *spi_dev,
                 const struct device *cs_gpio_dev, gpio_pin_t cs_pin);

/**
 * Reset ADS8688 to default state
 */
int ads8688_reset(struct ads8688_dev *dev);

/**
 * Set input range for a channel
 */
int ads8688_set_range(struct ads8688_dev *dev, uint8_t channel, uint8_t range);

/**
 * Read a single channel (manual mode)
 */
int ads8688_read_channel(struct ads8688_dev *dev, uint8_t channel,
                         int16_t *raw_value);

/**
 * Read all 8 channels sequentially
 */
int ads8688_read_all_channels(struct ads8688_dev *dev,
                              int16_t values[ADS8688_NUM_CHANNELS]);

/**
 * Convert raw ADC value to voltage (millivolts)
 */
int32_t ads8688_to_mv(int16_t raw_value, uint8_t range);

/**
 * Convert raw ADC value to voltage (float, volts)
 */
float ads8688_to_volts(int16_t raw_value, uint8_t range);

#endif /* ADS8688_H */
