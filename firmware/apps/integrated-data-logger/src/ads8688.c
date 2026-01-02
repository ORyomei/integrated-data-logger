#include "ads8688.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(ads8688, LOG_LEVEL_DBG);

static int ads8688_write_register(struct ads8688_data *data, uint8_t reg, uint8_t value)
{
    uint8_t tx_buf[2];
    
    /* Write register command: 0xD0 | (reg << 1) followed by value */
    tx_buf[0] = 0xD0 | ((reg & 0x3F) << 1);
    tx_buf[1] = value;
    
    const struct spi_buf tx_spi_buf = {
        .buf = tx_buf,
        .len = sizeof(tx_buf)
    };
    
    const struct spi_buf_set tx_spi_buf_set = {
        .buffers = &tx_spi_buf,
        .count = 1
    };
    
    return spi_write_dt(&data->spi, &tx_spi_buf_set);
}

int ads8688_init(const struct device *dev)
{
    struct ads8688_data *data = dev->data;
    int ret;
    
    /* Check if SPI is ready */
    if (!spi_is_ready_dt(&data->spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }
    
    /* Reset device */
    uint8_t cmd = ADS8688_CMD_RST;
    const struct spi_buf tx_buf = {
        .buf = &cmd,
        .len = 1
    };
    const struct spi_buf_set tx_buf_set = {
        .buffers = &tx_buf,
        .count = 1
    };
    
    ret = spi_write_dt(&data->spi, &tx_buf_set);
    if (ret < 0) {
        LOG_ERR("Failed to reset ADS8688: %d", ret);
        return ret;
    }
    
    k_msleep(10);
    
    LOG_INF("ADS8688 initialized");
    return 0;
}

int ads8688_set_range(const struct device *dev, uint8_t channel, uint8_t range)
{
    struct ads8688_data *data = dev->data;
    
    if (channel > 7) {
        return -EINVAL;
    }
    
    uint8_t reg = ADS8688_REG_CH0_RANGE + channel;
    return ads8688_write_register(data, reg, range);
}

int ads8688_read_channel(const struct device *dev, uint8_t channel, int16_t *value)
{
    struct ads8688_data *data = dev->data;
    uint8_t tx_buf[4] = {0};
    uint8_t rx_buf[4] = {0};
    
    if (channel > 7 || value == NULL) {
        return -EINVAL;
    }
    
    /* Select channel */
    tx_buf[0] = ADS8688_CMD_MAN_CH_0 + (channel << 2);
    
    const struct spi_buf tx_spi_buf = {
        .buf = tx_buf,
        .len = 4
    };
    
    const struct spi_buf_set tx_spi_buf_set = {
        .buffers = &tx_spi_buf,
        .count = 1
    };
    
    const struct spi_buf rx_spi_buf = {
        .buf = rx_buf,
        .len = 4
    };
    
    const struct spi_buf_set rx_spi_buf_set = {
        .buffers = &rx_spi_buf,
        .count = 1
    };
    
    int ret = spi_transceive_dt(&data->spi, &tx_spi_buf_set, &rx_spi_buf_set);
    if (ret < 0) {
        return ret;
    }
    
    /* Extract 16-bit value from bytes 2 and 3 */
    *value = (int16_t)((rx_buf[2] << 8) | rx_buf[3]);
    
    return 0;
}

int ads8688_start_auto_sequence(const struct device *dev)
{
    struct ads8688_data *data = dev->data;
    int ret;
    
    /* Enable all 8 channels in auto-sequence mode */
    ret = ads8688_write_register(data, ADS8688_REG_AUTO_SEQ_EN, 0xFF);
    if (ret < 0) {
        LOG_ERR("Failed to enable auto-sequence: %d", ret);
        return ret;
    }
    
    /* Send auto-sequence reset command */
    uint8_t cmd = ADS8688_CMD_AUTO_RST;
    const struct spi_buf tx_buf = {
        .buf = &cmd,
        .len = 1
    };
    const struct spi_buf_set tx_buf_set = {
        .buffers = &tx_buf,
        .count = 1
    };
    
    ret = spi_write_dt(&data->spi, &tx_buf_set);
    if (ret < 0) {
        LOG_ERR("Failed to start auto-sequence: %d", ret);
        return ret;
    }
    
    LOG_INF("Auto-sequence started");
    return 0;
}
