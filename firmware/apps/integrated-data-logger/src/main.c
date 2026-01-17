/*
 * Integrated Data Logger - USB CDC Version
 * - 60Hz GPIO square wave output
 * - USB CDC ACM for PC communication
 * - (Future: ADS8688 8ch ADC @ 10kHz)
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* GPIO for 60Hz output (GPIO2_3 = Teensy pin 4) */
#define GPIO2_NODE DT_NODELABEL(gpio2)
#define SQUARE_WAVE_PIN 3 /* GPIO2_03 */

/* Status LED (GPIO2_3 is also used as indicator) */
#define STATUS_LED_PIN 3

/* USB CDC device */
static const struct device *cdc_dev;

/* GPIO device */
static const struct device *gpio2_dev;

/* 60Hz timer */
static struct k_timer square_wave_timer;
static volatile bool square_wave_state = false;
static volatile uint32_t tick_count = 0;

/* Timer callback for 60Hz square wave */
static void square_wave_timer_handler(struct k_timer *timer) {
  ARG_UNUSED(timer);
  square_wave_state = !square_wave_state;
  gpio_pin_set(gpio2_dev, SQUARE_WAVE_PIN, square_wave_state ? 1 : 0);
  tick_count++;
}

/* Send data via USB CDC */
static int cdc_send(const char *data, size_t len) {
  if (!cdc_dev || !device_is_ready(cdc_dev)) {
    return -ENODEV;
  }

  for (size_t i = 0; i < len; i++) {
    uart_poll_out(cdc_dev, data[i]);
  }
  return len;
}

/* Send formatted string via USB CDC */
static void cdc_printf(const char *fmt, ...) {
  static char buf[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len > 0) {
    cdc_send(buf, len);
  }
}

int main(void) {
  int ret;

  LOG_INF("===========================================");
  LOG_INF("Integrated Data Logger (USB CDC) starting...");
  LOG_INF("===========================================");

  /* Initialize GPIO */
  gpio2_dev = DEVICE_DT_GET(GPIO2_NODE);
  if (!device_is_ready(gpio2_dev)) {
    LOG_ERR("GPIO2 device not ready");
    return -ENODEV;
  }

  ret = gpio_pin_configure(gpio2_dev, SQUARE_WAVE_PIN, GPIO_OUTPUT_LOW);
  if (ret < 0) {
    LOG_ERR("Failed to configure GPIO2_%d: %d", SQUARE_WAVE_PIN, ret);
    return ret;
  }
  LOG_INF("GPIO2_%d configured for 60Hz output", SQUARE_WAVE_PIN);

  /* Initialize USB */
  ret = usb_enable(NULL);
  if (ret != 0) {
    LOG_ERR("Failed to enable USB: %d", ret);
    return ret;
  }
  LOG_INF("USB enabled");

  /* Get USB CDC device */
  cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
  if (!device_is_ready(cdc_dev)) {
    LOG_ERR("USB CDC device not ready");
    return -ENODEV;
  }
  LOG_INF("USB CDC device ready");

  /* Wait for USB enumeration */
  LOG_INF("Waiting for USB host connection...");
  k_sleep(K_SECONDS(2));

  /* Start 60Hz timer (half period = 8.333ms for 60Hz full cycle) */
  k_timer_init(&square_wave_timer, square_wave_timer_handler, NULL);
  k_timer_start(&square_wave_timer, K_USEC(8333), K_USEC(8333));
  LOG_INF("60Hz square wave started on GPIO2_%d", SQUARE_WAVE_PIN);

  /* Main loop - send status every second */
  uint32_t loop_count = 0;
  uint32_t last_tick = 0;

  LOG_INF("Entering main loop...");

  while (1) {
    k_sleep(K_SECONDS(1));
    loop_count++;

    /* Calculate actual frequency */
    uint32_t current_tick = tick_count;
    uint32_t ticks_per_sec = current_tick - last_tick;
    last_tick = current_tick;
    float freq = ticks_per_sec / 2.0f; /* 2 edges per cycle */

    /* Send status via USB CDC */
    cdc_printf("STATUS: loop=%u, ticks=%u, freq=%.1fHz\r\n", loop_count,
               current_tick, (double)freq);

    /* Also log */
    LOG_INF("Loop %u: %u ticks, %.1f Hz", loop_count, ticks_per_sec,
            (double)freq);
  }

  return 0;
}
