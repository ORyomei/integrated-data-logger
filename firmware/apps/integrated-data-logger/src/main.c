#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <errno.h>
#include <math.h>
#include "ads8688.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* GPIO for 60Hz square wave output - using GPIO1 pin 0 (Teensy pin 19) */
#define SQUARE_WAVE_GPIO_NODE DT_NODELABEL(gpio1)
#define SQUARE_WAVE_PIN 0
static const struct device *gpio_dev;

/* LED for status indication - using GPIO2 pin 3 (Teensy pin 13, onboard LED) */
#define STATUS_LED_GPIO_NODE DT_NODELABEL(gpio2)
#define STATUS_LED_PIN 3
static const struct device *led_dev;

/* ADS8688 device */
static struct ads8688_data ads_data;
static const struct device ads8688_dev = {
    .data = &ads_data,
};

/* Sampling configuration */
#define SAMPLING_FREQ_HZ    10000
#define SAMPLING_PERIOD_US  (1000000 / SAMPLING_FREQ_HZ)  /* 100us */
#define NUM_CHANNELS        8

/* UDP configuration */
#define UDP_PORT            8888
#define UDP_DEST_ADDR       "192.168.0.242"  // PC のIPアドレス
static int udp_sock = -1;
static struct sockaddr_in dest_addr;

/* Data buffers */
static int16_t adc_buffer[NUM_CHANNELS];
static K_MUTEX_DEFINE(adc_mutex);

/* Network event handler */
static struct net_mgmt_event_callback mgmt_cb;

static void net_event_handler(struct net_mgmt_event_callback *cb,
                             uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        char addr_str[NET_IPV4_ADDR_LEN];
        
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            struct net_if_addr_ipv4 *if_addr = &iface->config.ip.ipv4->unicast[i];
            
            if (if_addr->ipv4.addr_state == NET_ADDR_PREFERRED) {
                net_addr_ntop(AF_INET, &if_addr->ipv4.address.in_addr,
                            addr_str, sizeof(addr_str));
                LOG_INF("IPv4 address assigned: %s", addr_str);
                break;
            }
        }
    }
}

static int setup_udp_socket(void)
{
    udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", errno);
        return -errno;
    }
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);
    zsock_inet_pton(AF_INET, UDP_DEST_ADDR, &dest_addr.sin_addr);
    
    LOG_INF("UDP socket created, will send to %s:%d", UDP_DEST_ADDR, UDP_PORT);
    return 0;
}

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
    
    /* Wait for UDP socket to be ready */
    k_sleep(K_SECONDS(2));
    
    char buf[256];
    int len;
    int counter = 0;
    
    while (1) {
        /* Send data every 100ms (10Hz for testing) */
        k_sleep(K_MSEC(100));
        
        /* Generate dummy data - sine wave pattern */
        for (int i = 0; i < NUM_CHANNELS; i++) {
            adc_buffer[i] = (int16_t)(10000 * sin(2.0 * 3.14159 * counter * 0.1 + i * 0.5));
        }
        counter++;
        
        /* Format as CSV: timestamp,ch0,ch1,...,ch7 */
        len = snprintf(buf, sizeof(buf), "%lld,%d,%d,%d,%d,%d,%d,%d,%d\n",
                      k_uptime_get(),
                      adc_buffer[0], adc_buffer[1], adc_buffer[2], adc_buffer[3],
                      adc_buffer[4], adc_buffer[5], adc_buffer[6], adc_buffer[7]);
        
        /* Send via UDP */
        if (udp_sock >= 0) {
            int ret = zsock_sendto(udp_sock, buf, len, 0,
                           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (ret < 0) {
                LOG_ERR("Failed to send UDP packet: %d", errno);
            }
        }
        
        /* Also log periodically */
        if (counter % 10 == 0) {  // Log every 1 second
            LOG_INF("Sending dummy data: CH0=%d CH1=%d CH2=%d CH3=%d",
                    adc_buffer[0], adc_buffer[1], adc_buffer[2], adc_buffer[3]);
        }
    }
}

/* Define threads */
// Enable test thread to send dummy data
K_THREAD_DEFINE(log_thread, 2048, data_logging_thread, NULL, NULL, NULL, 7, 0, 0);

int main(void)
{
    int ret;
    
    /* Configure status LED */
    led_dev = DEVICE_DT_GET(STATUS_LED_GPIO_NODE);
    if (!device_is_ready(led_dev)) {
        while (1) {
            k_sleep(K_FOREVER);
        }
    }
    
    ret = gpio_pin_configure(led_dev, STATUS_LED_PIN, GPIO_OUTPUT_LOW);
    if (ret < 0) {
        while (1) {
            k_sleep(K_FOREVER);
        }
    }
    
    /* Startup: 3 quick blinks */
    for (int i = 0; i < 3; i++) {
        gpio_pin_set(led_dev, STATUS_LED_PIN, 1);
        k_sleep(K_MSEC(100));
        gpio_pin_set(led_dev, STATUS_LED_PIN, 0);
        k_sleep(K_MSEC(100));
    }
    
    LOG_INF("Integrated Data Logger starting...");
    
    /* Setup network event handler */
    net_mgmt_init_event_callback(&mgmt_cb, net_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgmt_cb);
    
    /* Wait for network to be ready */
    LOG_INF("Waiting for network...");
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        /* Fast blink = error */
        while (1) {
            gpio_pin_toggle(led_dev, STATUS_LED_PIN);
            k_sleep(K_MSEC(100));
        }
    }
    
    /* Start DHCP */
    LOG_INF("Starting DHCP...");
    net_dhcpv4_start(iface);
    
    /* Wait for IP address - blink while waiting */
    for (int i = 0; i < 10; i++) {
        gpio_pin_toggle(led_dev, STATUS_LED_PIN);
        k_sleep(K_MSEC(500));
    }
    
    /* LED on = network ready */
    gpio_pin_set(led_dev, STATUS_LED_PIN, 1);
    
    /* Setup UDP socket */
    ret = setup_udp_socket();
    if (ret < 0) {
        LOG_WRN("UDP socket setup failed, will continue without network");
    }
    
    /* Skip ADS8688 and GPIO initialization for testing */
    LOG_INF("Skipping ADS8688 initialization for network testing");
    
    LOG_INF("System initialized successfully (network only)");
    LOG_INF("UDP streaming to %s:%d at 100Hz", UDP_DEST_ADDR, UDP_PORT);
    
    /* Main thread - blink LED every 2 seconds to show we're running */
    while (1) {
        k_sleep(K_SECONDS(2));
        gpio_pin_toggle(led_dev, STATUS_LED_PIN);
    }
    
    return 0;
}
