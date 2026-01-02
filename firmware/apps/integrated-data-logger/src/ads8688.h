#ifndef ADS8688_H
#define ADS8688_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

/* ADS8688 Commands */
#define ADS8688_CMD_NO_OP           0x00
#define ADS8688_CMD_STDBY           0x82
#define ADS8688_CMD_PWR_DN          0x83
#define ADS8688_CMD_RST             0x85
#define ADS8688_CMD_AUTO_RST        0xA0
#define ADS8688_CMD_MAN_CH_0        0xC0
#define ADS8688_CMD_MAN_CH_1        0xC4
#define ADS8688_CMD_MAN_CH_2        0xC8
#define ADS8688_CMD_MAN_CH_3        0xCC
#define ADS8688_CMD_MAN_CH_4        0xD0
#define ADS8688_CMD_MAN_CH_5        0xD4
#define ADS8688_CMD_MAN_CH_6        0xD8
#define ADS8688_CMD_MAN_CH_7        0xDC

/* ADS8688 Register Addresses */
#define ADS8688_REG_AUTO_SEQ_EN     0x01
#define ADS8688_REG_CH0_RANGE       0x05
#define ADS8688_REG_CH1_RANGE       0x06
#define ADS8688_REG_CH2_RANGE       0x07
#define ADS8688_REG_CH3_RANGE       0x08
#define ADS8688_REG_CH4_RANGE       0x09
#define ADS8688_REG_CH5_RANGE       0x0A
#define ADS8688_REG_CH6_RANGE       0x0B
#define ADS8688_REG_CH7_RANGE       0x0C

/* Input Range Settings */
#define ADS8688_RANGE_PM_2_5V       0x00  // ±2.5V
#define ADS8688_RANGE_PM_5V         0x01  // ±5V
#define ADS8688_RANGE_PM_10V        0x02  // ±10V
#define ADS8688_RANGE_0_TO_5V       0x03  // 0 to 5V
#define ADS8688_RANGE_0_TO_10V      0x04  // 0 to 10V
#define ADS8688_RANGE_0_TO_2_5V     0x05  // 0 to 2.5V

/* ADS8688 Device Structure */
struct ads8688_data {
    struct spi_dt_spec spi;
};

/* Function prototypes */
int ads8688_init(const struct device *dev);
int ads8688_read_channel(const struct device *dev, uint8_t channel, int16_t *value);
int ads8688_set_range(const struct device *dev, uint8_t channel, uint8_t range);
int ads8688_start_auto_sequence(const struct device *dev);

#endif /* ADS8688_H */
