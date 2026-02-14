#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>

#define CDC_NODE DT_ALIAS(usb_data_uart)

static void wait_for_dtr(const struct device *dev) {
  uint32_t dtr = 0;
  while (true) {
    (void)uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    if (dtr) {
      break;
    }
    k_msleep(10);
  }
}

int main(void) {
  printk("[UART-LOG] boot\n");

  int ret = usb_enable(NULL);
  if (ret) {
    printk("[UART-LOG] usb_enable failed: %d\n", ret);
    return 0;
  }
  printk("[UART-LOG] USB enabled\n");

  const struct device *cdc = DEVICE_DT_GET(CDC_NODE);
  if (!device_is_ready(cdc)) {
    printk("[UART-LOG] CDC device not ready\n");
    return 0;
  }

  printk("[UART-LOG] waiting for USB DTR...\n");
  wait_for_dtr(cdc);
  printk("[UART-LOG] DTR ON\n");

  const char *hello = "[USB-DATA] hello from CDC ACM\r\n";
  int res = uart_tx(cdc, hello, strlen(hello), SYS_FOREVER_US);
  printk("[UART-LOG] uart_tx res: %d", res);

  int cnt = 0;
  while (1) {
    char buf[64];
    int n = snprintk(buf, sizeof(buf), "[USB-DATA] tick %d\r\n", cnt++);
    int res = uart_tx(cdc, buf, n, SYS_FOREVER_US);
    printk("[UART-LOG] uart_tx res: %d", res);

    printk("[UART-LOG] alive %d\n", cnt);
    k_sleep(K_SECONDS(1));
  }
}
