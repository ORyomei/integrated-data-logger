#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "ads8688.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* GPIO for 60Hz square wave output - using GPIO1 pin 0 (Teensy pin 19) */
#define SQUARE_WAVE_GPIO_NODE DT_NODELABEL(gpio1)
#define SQUARE_WAVE_PIN 0
static const struct device *gpio_dev;

/* ADS8688 device */
static struct ads8688_data ads_data;
static const struct device ads8688_dev = {
    .data = &ads_data,
};

/* Sampling configuration */
#define SAMPLING_FREQ_HZ    10000
#define SAMPLING_PERIOD_US  (1000000 / SAMPLING_FREQ_HZ)  /* 100us */
#define NUM_CHANNELS        8

/* Data buffers */
static int16_t adc_buffer[NUM_CHANNELS];
static K_MUTEX_DEFINE(adc_mutex);

/* Thread for ADC sampling at 10kHz */
void adc_sampling_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    int ret;
    int64_t next_sample_time;
    
    LOG_INF("ADC sampling thread started");
    
    /* Get initial timestamp */
    next_sample_time = k_uptime_get();
    
    while (1) {
        /* Read all 8 channels */
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            ret = ads8688_read_channel(&ads8688_dev, ch, &adc_buffer[ch]);
            if (ret < 0) {
                LOG_ERR("Failed to read channel %d: %d", ch, ret);
            }
        }
        
        /* Calculate next sample time */
        next_sample_time += SAMPLING_PERIOD_US / 1000;
        
        /* Sleep until next sample time */
        int64_t current_time = k_uptime_get();
        int64_t sleep_time = next_sample_time - current_time;
        
        if (sleep_time > 0) {
            k_usleep(sleep_time * 1000);
        } else {
            /* We missed the deadline */
            LOG_WRN("Sampling missed deadline by %lld ms", -sleep_time);
            next_sample_time = current_time;
        }
    }
}

/* Thread for 60Hz square wave generation */
void square_wave_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Square wave thread started");
    
    /* 60Hz = 16666.67us period, toggle every 8333us */
    const uint32_t half_period_us = 8333;
    
    while (1) {
        gpio_pin_toggle(gpio_dev, SQUARE_WAVE_PIN);
        k_usleep(half_period_us);
    }
}

/* Thread for data processing and logging */
void data_logging_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Data logging thread started");
    
    while (1) {
        /* Log data every 100ms */
        k_sleep(K_MSEC(100));
        
        k_mutex_lock(&adc_mutex, K_FOREVER);
        LOG_INF("ADC: CH0=%d CH1=%d CH2=%d CH3=%d CH4=%d CH5=%d CH6=%d CH7=%d",
                adc_buffer[0], adc_buffer[1], adc_buffer[2], adc_buffer[3],
                adc_buffer[4], adc_buffer[5], adc_buffer[6], adc_buffer[7]);
        k_mutex_unlock(&adc_mutex);
    }
}

/* Define threads */
K_THREAD_DEFINE(adc_thread, 4096, adc_sampling_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(wave_thread, 1024, square_wave_thread, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(log_thread, 2048, data_logging_thread, NULL, NULL, NULL, 7, 0, 0);

int main(void)
{
    int ret;
    
    LOG_INF("Integrated Data Logger starting...");
    
    /* Initialize SPI for ADS8688 */
    ads_data.spi.bus = DEVICE_DT_GET(DT_BUS(DT_NODELABEL(ads8688_device)));
    ads_data.spi.config.frequency = 17000000;  /* 17 MHz */
    ads_data.spi.config.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA;
    ads_data.spi.config.slave = 0;
    
    struct gpio_dt_spec cs_spec = GPIO_DT_SPEC_GET_BY_IDX(DT_BUS(DT_NODELABEL(ads8688_device)), cs_gpios, 0);
    ads_data.spi.config.cs.gpio = cs_spec;
    ads_data.spi.config.cs.delay = 0;
    
    /* Initialize ADS8688 */
    ret = ads8688_init(&ads8688_dev);
    if (ret < 0) {
        LOG_ERR("Failed to initialize ADS8688: %d", ret);
        return ret;
    }
    
    /* Configure all channels for ±10V range */
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        ret = ads8688_set_range(&ads8688_dev, ch, ADS8688_RANGE_PM_10V);
        if (ret < 0) {
            LOG_ERR("Failed to set range for channel %d: %d", ch, ret);
            return ret;
        }
    }
    
    LOG_INF("All channels configured for ±10V range");
    
    /* Configure GPIO for 60Hz square wave output */
    gpio_dev = DEVICE_DT_GET(SQUARE_WAVE_GPIO_NODE);
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure(gpio_dev, SQUARE_WAVE_PIN, GPIO_OUTPUT_LOW);
    if (ret < 0) {
        LOG_ERR("Failed to configure GPIO pin: %d", ret);
        return ret;
    }
    
    LOG_INF("60Hz square wave output configured on GPIO pin %d", SQUARE_WAVE_PIN);
    
    LOG_INF("System initialized successfully");
    LOG_INF("Sampling at %d Hz on %d channels", SAMPLING_FREQ_HZ, NUM_CHANNELS);
    LOG_INF("Square wave output at 60 Hz");
    
    /* Main thread can sleep or perform other tasks */
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}
