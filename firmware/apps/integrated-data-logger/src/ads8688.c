/*
 * ADS8688 8-Channel 16-bit ADC Driver Implementation
 */

#include "ads8688.h"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ads8688, LOG_LEVEL_INF);

/* Full scale voltage for each range (in microvolts) */
static const int32_t range_scale_uv[] = {
    10240000, /* ±10.24V */
    5120000,  /* ±5.12V */
    2560000,  /* ±2.56V */
    1280000,  /* ±1.28V */
    640000,   /* ±0.64V */
    10240000, /* 0 to 10.24V */
    5120000,  /* 0 to 5.12V */
};

/* Manual channel select commands */
static const uint16_t man_ch_cmd[] = {
    ADS8688_CMD_MAN_CH0, ADS8688_CMD_MAN_CH1, ADS8688_CMD_MAN_CH2,
    ADS8688_CMD_MAN_CH3, ADS8688_CMD_MAN_CH4, ADS8688_CMD_MAN_CH5,
    ADS8688_CMD_MAN_CH6, ADS8688_CMD_MAN_CH7,
};

/* Write a program register */
static int ads8688_write_reg(struct ads8688_dev *dev, uint8_t reg,
                             uint8_t value) {
  /* Write command: [REG_ADDR << 1 | 1] [DATA] */
  uint8_t tx_buf[2] = {(reg << 1) | 0x01, value};

  struct spi_buf tx = {.buf = tx_buf, .len = 2};
  struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};

  /* CS LOW */
  gpio_pin_set(dev->cs_gpio_dev, dev->cs_pin, 0);

  int ret = spi_write(dev->spi_dev, &dev->spi_cfg, &tx_set);

  /* CS HIGH */
  gpio_pin_set(dev->cs_gpio_dev, dev->cs_pin, 1);

  return ret;
}

/* Send command and read result */
static int ads8688_cmd_read(struct ads8688_dev *dev, uint16_t cmd,
                            uint16_t *result) {
  /* ADS8688 uses 32-bit frames:
   * TX: [CMD_HIGH] [CMD_LOW] [0x00] [0x00]
   * RX: [data_high] [data_low] [x] [x]
   * Data from PREVIOUS command is in first 16 bits
   */
  uint8_t tx_buf[4] = {(cmd >> 8) & 0xFF, cmd & 0xFF, 0x00, 0x00};
  uint8_t rx_buf[4] = {0xAA, 0xBB, 0xCC,
                       0xDD}; /* Pre-fill to detect if SPI overwrites */

  struct spi_buf tx = {.buf = tx_buf, .len = 4};
  struct spi_buf rx = {.buf = rx_buf, .len = 4};
  struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};
  struct spi_buf_set rx_set = {.buffers = &rx, .count = 1};

  /* CS LOW - active */
  gpio_pin_set(dev->cs_gpio_dev, dev->cs_pin, 0);
  k_busy_wait(1); /* 1us delay after CS low */

  int ret = spi_transceive(dev->spi_dev, &dev->spi_cfg, &tx_set, &rx_set);

  k_busy_wait(1); /* 1us delay before CS high */
  /* CS HIGH - inactive */
  gpio_pin_set(dev->cs_gpio_dev, dev->cs_pin, 1);

  if (ret < 0) {
    LOG_ERR("SPI transceive failed: %d", ret);
    return ret;
  }

  /* Debug: log raw SPI data */
  static int debug_count = 0;
  if (debug_count < 20) {
    LOG_INF("SPI TX: %02X %02X %02X %02X -> RX: %02X %02X %02X %02X", tx_buf[0],
            tx_buf[1], tx_buf[2], tx_buf[3], rx_buf[0], rx_buf[1], rx_buf[2],
            rx_buf[3]);
    debug_count++;
  }

  /* Result is in bytes 0-1 (first 16 bits of SDO) */
  *result = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];
  return 0;
}

int ads8688_init(struct ads8688_dev *dev, const struct device *spi_dev,
                 const struct device *cs_gpio_dev, gpio_pin_t cs_pin) {
  LOG_INF("ADS8688 init: SPI Mode 1 (CPHA=1), 500kHz, GPIO CS on pin %d",
          cs_pin);

  if (!device_is_ready(spi_dev)) {
    LOG_ERR("SPI device not ready");
    return -ENODEV;
  }

  if (!device_is_ready(cs_gpio_dev)) {
    LOG_ERR("CS GPIO device not ready");
    return -ENODEV;
  }

  /* Configure CS GPIO as output, initially high (deasserted) */
  int ret = gpio_pin_configure(cs_gpio_dev, cs_pin, GPIO_OUTPUT_HIGH);
  if (ret < 0) {
    LOG_ERR("Failed to configure CS GPIO: %d", ret);
    return ret;
  }

  memset(dev, 0, sizeof(*dev));
  dev->spi_dev = spi_dev;
  dev->cs_gpio_dev = cs_gpio_dev;
  dev->cs_pin = cs_pin;

  /* SPI configuration: Mode 1 (CPOL=0, CPHA=1), MSB first, 8-bit */
  /* ADS8688 clocks data out on falling edge, samples on rising edge */
  dev->spi_cfg.frequency = 500000; /* 500 kHz (slower for testing) */
  dev->spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA;
  dev->spi_cfg.slave = 0;
  dev->spi_cfg.cs.gpio.port = NULL; /* Use software CS control */

  /* Reset device */
  ret = ads8688_reset(dev);
  if (ret < 0) {
    LOG_ERR("Failed to reset ADS8688: %d", ret);
    return ret;
  }

  /* Set all channels to ±10.24V range */
  for (int ch = 0; ch < ADS8688_NUM_CHANNELS; ch++) {
    ret = ads8688_set_range(dev, ch, ADS8688_RANGE_PM_10V24);
    if (ret < 0) {
      LOG_ERR("Failed to set range for channel %d: %d", ch, ret);
      return ret;
    }
  }

  dev->initialized = true;
  LOG_INF("ADS8688 initialized successfully");
  return 0;
}

int ads8688_reset(struct ads8688_dev *dev) {
  uint16_t dummy;

  LOG_INF("ADS8688 reset: sending RST command (0x85)");

  /* Ensure CS is high before starting */
  gpio_pin_set(dev->cs_gpio_dev, dev->cs_pin, 1);
  k_msleep(10); /* Wait 10ms */

  int ret = ads8688_cmd_read(dev, ADS8688_CMD_RST, &dummy);
  if (ret < 0) {
    return ret;
  }

  /* Wait for reset to complete - longer delay */
  k_msleep(50);

  /* Send another NO_OP to check device response */
  ret = ads8688_cmd_read(dev, ADS8688_CMD_NO_OP, &dummy);
  LOG_INF("After reset, NO_OP response: 0x%04X", dummy);

  /* Power on all channels */
  ret = ads8688_write_reg(dev, ADS8688_REG_CH_PWR_DN, 0x00);
  if (ret < 0) {
    return ret;
  }

  return 0;
}

int ads8688_set_range(struct ads8688_dev *dev, uint8_t channel, uint8_t range) {
  if (channel >= ADS8688_NUM_CHANNELS) {
    return -EINVAL;
  }

  if (range > ADS8688_RANGE_0_5V12) {
    return -EINVAL;
  }

  uint8_t reg = ADS8688_REG_CH0_INPUT_RANGE + channel;
  int ret = ads8688_write_reg(dev, reg, range);
  if (ret < 0) {
    return ret;
  }

  dev->input_range[channel] = range;
  return 0;
}

int ads8688_read_channel(struct ads8688_dev *dev, uint8_t channel,
                         int16_t *raw_value) {
  if (channel >= ADS8688_NUM_CHANNELS) {
    return -EINVAL;
  }

  uint16_t result;

  /* First command selects the channel and starts conversion */
  int ret = ads8688_cmd_read(dev, man_ch_cmd[channel], &result);
  if (ret < 0) {
    return ret;
  }

  /* Second command reads the result */
  ret = ads8688_cmd_read(dev, ADS8688_CMD_NO_OP, &result);
  if (ret < 0) {
    return ret;
  }

  *raw_value = (int16_t)result;
  return 0;
}

int ads8688_read_all_channels(struct ads8688_dev *dev,
                              int16_t values[ADS8688_NUM_CHANNELS]) {
  uint16_t result;
  int ret;

  /* Start with channel 0 */
  ret = ads8688_cmd_read(dev, man_ch_cmd[0], &result);
  if (ret < 0) {
    return ret;
  }

  /* Read channels 0-6, while selecting next channel */
  for (int ch = 0; ch < ADS8688_NUM_CHANNELS - 1; ch++) {
    ret = ads8688_cmd_read(dev, man_ch_cmd[ch + 1], &result);
    if (ret < 0) {
      return ret;
    }
    values[ch] = (int16_t)result;
  }

  /* Read final channel 7 */
  ret = ads8688_cmd_read(dev, ADS8688_CMD_NO_OP, &result);
  if (ret < 0) {
    return ret;
  }
  values[7] = (int16_t)result;

  return 0;
}

int32_t ads8688_to_mv(int16_t raw_value, uint8_t range) {
  if (range > ADS8688_RANGE_0_5V12) {
    range = ADS8688_RANGE_PM_10V24;
  }

  /* For bipolar ranges: voltage = raw * full_scale / 32768 */
  /* Result in millivolts */
  int64_t uv = ((int64_t)raw_value * range_scale_uv[range]) / 32768;
  return (int32_t)(uv / 1000);
}

float ads8688_to_volts(int16_t raw_value, uint8_t range) {
  if (range > ADS8688_RANGE_0_5V12) {
    range = ADS8688_RANGE_PM_10V24;
  }

  float full_scale = (float)range_scale_uv[range] / 1000000.0f;
  return ((float)raw_value / 32768.0f) * full_scale;
}
