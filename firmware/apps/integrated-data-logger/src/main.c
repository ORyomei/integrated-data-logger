#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/devicetree.h>
#include <zephyr/net/phy.h>
#include <zephyr/net/mii.h>
#include <zephyr/drivers/mdio.h>

#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ----- LED (Teensy 4.1 onboard LED: gpio2 pin 3) ----- */
#define STATUS_LED_GPIO_NODE DT_NODELABEL(gpio2)
#define STATUS_LED_PIN 3
static const struct device *led_dev;

/* ----- TCP ----- */
#define TCP_PORT 8888
static int tcp_server_sock = -1;
static int tcp_client_sock = -1;

/* ----- Network events ----- */
static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(link_up_sem, 0, 1);

#if DT_NODE_HAS_PROP(DT_NODELABEL(phy), reset_gpios)
static const struct gpio_dt_spec phy_reset =
	GPIO_DT_SPEC_GET(DT_NODELABEL(phy), reset_gpios);
#endif
#define PHY_ADDR DT_REG_ADDR(DT_NODELABEL(phy))

/* DP83825 specific registers */
#define DP83825_PHYSCR_REG 0x11
#define DP83825_MISR_REG   0x12
#define DP83825_RCSR_REG   0x17
#define DP83825_RCSR_REF_CLK_SEL BIT(7)

#if defined(NET_EVENT_L2_CONNECTED)
static bool wait_for_link_up(struct net_if *iface, k_timeout_t timeout)
{
	/* Link might already be up (e.g. after reflashing) and no event fires. */
	if (net_if_is_carrier_ok(iface)) {
		return true;
	}

	if (k_sem_take(&link_up_sem, timeout) == 0) {
		return true;
	}

	return net_if_is_carrier_ok(iface);
}
#endif

static void phy_reset_pulse(void)
{
#if DT_NODE_HAS_PROP(DT_NODELABEL(phy), reset_gpios)
	if (!device_is_ready(phy_reset.port)) {
		LOG_WRN("PHY reset GPIO not ready");
		return;
	}

	/* Handle active-low explicitly; gpio_pin_set_dt is raw level. */
	int inactive_level = (phy_reset.dt_flags & GPIO_ACTIVE_LOW) ? 1 : 0;
	int active_level = (phy_reset.dt_flags & GPIO_ACTIVE_LOW) ? 0 : 1;

	(void)gpio_pin_configure_dt(&phy_reset, GPIO_OUTPUT_INACTIVE);
	(void)gpio_pin_set_dt(&phy_reset, inactive_level);
	k_sleep(K_MSEC(1));
	(void)gpio_pin_set_dt(&phy_reset, active_level);
	k_sleep(K_MSEC(10));
	(void)gpio_pin_set_dt(&phy_reset, inactive_level);
	k_sleep(K_MSEC(50));
#else
	LOG_WRN("No PHY reset-gpios in DTS");
#endif
}

static void dump_phy_status(void)
{
	const struct device *mdio = DEVICE_DT_GET(DT_NODELABEL(enet_mdio));
	uint32_t id1 = 0, id2 = 0, bmcr = 0, bmsr1 = 0, bmsr2 = 0;
	uint32_t anar = 0, anlpar = 0;
	uint32_t physcr = 0, misr = 0, rcsr = 0;

	if (!device_is_ready(mdio)) {
		LOG_ERR("MDIO device not ready");
		return;
	}

	mdio_bus_enable(mdio);
	if (mdio_read(mdio, PHY_ADDR, MII_PHYID1R, &id1) ||
	    mdio_read(mdio, PHY_ADDR, MII_PHYID2R, &id2) ||
	    mdio_read(mdio, PHY_ADDR, MII_BMCR, &bmcr) ||
	    mdio_read(mdio, PHY_ADDR, MII_BMSR, &bmsr1) ||
	    mdio_read(mdio, PHY_ADDR, MII_BMSR, &bmsr2) ||
	    mdio_read(mdio, PHY_ADDR, MII_ANAR, &anar) ||
	    mdio_read(mdio, PHY_ADDR, MII_ANLPAR, &anlpar) ||
	    mdio_read(mdio, PHY_ADDR, DP83825_PHYSCR_REG, &physcr) ||
	    mdio_read(mdio, PHY_ADDR, DP83825_MISR_REG, &misr) ||
	    mdio_read(mdio, PHY_ADDR, DP83825_RCSR_REG, &rcsr)) {
		LOG_ERR("PHY MDIO read failed");
		mdio_bus_disable(mdio);
		return;
	}
	mdio_bus_disable(mdio);

	LOG_INF("PHY addr %d ID: 0x%04x 0x%04x", PHY_ADDR, (uint16_t)id1, (uint16_t)id2);
	LOG_INF("PHY BMCR: 0x%04x BMSR: 0x%04x/0x%04x",
		(uint16_t)bmcr, (uint16_t)bmsr1, (uint16_t)bmsr2);
	LOG_INF("PHY ANAR: 0x%04x ANLPAR: 0x%04x", (uint16_t)anar, (uint16_t)anlpar);
	LOG_INF("PHY PHYSCR: 0x%04x MISR: 0x%04x RCSR: 0x%04x",
		(uint16_t)physcr, (uint16_t)misr, (uint16_t)rcsr);
}

static bool mdio_scan(void)
{
	const struct device *mdio = DEVICE_DT_GET(DT_NODELABEL(enet_mdio));
	int found = 0;
	int first_err = 0;
	int err_count = 0;

	if (!device_is_ready(mdio)) {
		LOG_ERR("MDIO device not ready");
		return false;
	}

	mdio_bus_enable(mdio);
	for (int addr = 0; addr < 32; addr++) {
		uint32_t id1 = 0xffff;
		uint32_t id2 = 0xffff;

		int ret = mdio_read(mdio, addr, MII_PHYID1R, &id1);
		if (ret != 0) {
			if (first_err == 0) {
				first_err = ret;
			}
			err_count++;
			continue;
		}

		ret = mdio_read(mdio, addr, MII_PHYID2R, &id2);
		if (ret != 0) {
			if (first_err == 0) {
				first_err = ret;
			}
			err_count++;
			continue;
		}

		if ((id1 == 0xffff && id2 == 0xffff) ||
		    (id1 == 0x0000 && id2 == 0x0000)) {
			continue;
		}

		LOG_INF("MDIO scan: addr %d id 0x%04x 0x%04x",
			addr, (uint16_t)id1, (uint16_t)id2);
		found++;
	}
	mdio_bus_disable(mdio);

	if (found == 0) {
		if (err_count > 0) {
			LOG_ERR("MDIO scan failed: first err %d (count %d)", first_err, err_count);
		} else {
			LOG_ERR("MDIO scan found no PHY devices");
		}
	}

	return found > 0;
}

static void set_phy_refclk_output(bool enable_output)
{
	const struct device *mdio = DEVICE_DT_GET(DT_NODELABEL(enet_mdio));
	uint32_t rcsr = 0;
	uint32_t bmcr = 0;

	if (!device_is_ready(mdio)) {
		LOG_ERR("MDIO device not ready");
		return;
	}

	mdio_bus_enable(mdio);
	if (mdio_read(mdio, PHY_ADDR, DP83825_RCSR_REG, &rcsr) == 0) {
		if (enable_output) {
			rcsr |= DP83825_RCSR_REF_CLK_SEL;
		} else {
			rcsr &= ~DP83825_RCSR_REF_CLK_SEL;
		}
		(void)mdio_write(mdio, PHY_ADDR, DP83825_RCSR_REG, rcsr);
		LOG_INF("Set PHY RCSR REF_CLK_SEL %s (0x%04x)",
			enable_output ? "ON" : "OFF", (uint16_t)rcsr);
	}

	if (mdio_read(mdio, PHY_ADDR, MII_BMCR, &bmcr) == 0) {
		bmcr |= MII_BMCR_AUTONEG_ENABLE | MII_BMCR_AUTONEG_RESTART;
		(void)mdio_write(mdio, PHY_ADDR, MII_BMCR, bmcr);
		LOG_INF("Restart PHY autoneg (BMCR=0x%04x)", (uint16_t)bmcr);
	}
	mdio_bus_disable(mdio);
}

static void force_phy_100m_full_mdio(void)
{
	const struct device *mdio = DEVICE_DT_GET(DT_NODELABEL(enet_mdio));
	uint32_t bmcr = 0;

	if (!device_is_ready(mdio)) {
		LOG_ERR("MDIO device not ready");
		return;
	}

	mdio_bus_enable(mdio);
	if (mdio_read(mdio, PHY_ADDR, MII_BMCR, &bmcr) == 0) {
		/* Disable autoneg, force 100M full duplex */
		bmcr &= ~MII_BMCR_AUTONEG_ENABLE;
		bmcr &= ~MII_BMCR_SPEED_MASK;
		bmcr |= MII_BMCR_SPEED_100 | MII_BMCR_DUPLEX_MODE;
		(void)mdio_write(mdio, PHY_ADDR, MII_BMCR, bmcr);
		LOG_INF("Force PHY 100M full via MDIO (BMCR=0x%04x)", (uint16_t)bmcr);
	}
	mdio_bus_disable(mdio);
}

static void restart_phy_autoneg(void)
{
	const struct device *phy = DEVICE_DT_GET(DT_NODELABEL(phy));

	if (!device_is_ready(phy)) {
		LOG_ERR("PHY device not ready");
		return;
	}

	int ret = phy_configure_link(phy,
				     LINK_HALF_10BASE | LINK_FULL_10BASE |
				     LINK_HALF_100BASE | LINK_FULL_100BASE,
				     0);
	if (ret) {
		LOG_ERR("Restart PHY autoneg failed: %d", ret);
	} else {
		LOG_INF("Restarted PHY autoneg via cfg_link");
	}
}

static void phy_soft_reset_and_autoneg(void)
{
	const struct device *mdio = DEVICE_DT_GET(DT_NODELABEL(enet_mdio));
	uint32_t bmcr = 0;

	if (!device_is_ready(mdio)) {
		LOG_ERR("MDIO device not ready");
		return;
	}

	mdio_bus_enable(mdio);
	if (mdio_read(mdio, PHY_ADDR, MII_BMCR, &bmcr) == 0) {
		bmcr |= MII_BMCR_RESET;
		(void)mdio_write(mdio, PHY_ADDR, MII_BMCR, bmcr);
	}

	/* Wait for reset to complete */
	for (int i = 0; i < 20; i++) {
		k_sleep(K_MSEC(10));
		if (mdio_read(mdio, PHY_ADDR, MII_BMCR, &bmcr) == 0) {
			if ((bmcr & MII_BMCR_RESET) == 0) {
				break;
			}
		}
	}
	mdio_bus_disable(mdio);

	restart_phy_autoneg();
}

static bool phy_link_is_up(void)
{
	const struct device *phy = DEVICE_DT_GET(DT_NODELABEL(phy));
	struct phy_link_state state = {0};

	if (!device_is_ready(phy)) {
		return false;
	}

	if (phy_get_link_state(phy, &state) == 0 && state.is_up) {
		return true;
	}

	return false;
}

static bool wait_for_link_with_poll(struct net_if *iface, int retries, int interval_ms)
{
	for (int i = 0; i < retries; i++) {
#if defined(NET_EVENT_L2_CONNECTED)
		if (phy_link_is_up()) {
			net_if_carrier_on(iface);
			return true;
		}
		if (wait_for_link_up(iface, K_MSEC(interval_ms))) {
			return true;
		}
#else
		k_sleep(K_MSEC(interval_ms));
		if (phy_link_is_up()) {
			net_if_carrier_on(iface);
			return true;
		}
		if (net_if_is_carrier_ok(iface)) {
			return true;
		}
#endif
		/* Short blink while waiting for link */
		gpio_pin_toggle(led_dev, STATUS_LED_PIN);
	}

	return net_if_is_carrier_ok(iface);
}

static void net_event_handler(struct net_mgmt_event_callback *cb,
			      uint64_t mgmt_event,
			      struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

#if defined(NET_EVENT_L2_CONNECTED)
	if (mgmt_event == NET_EVENT_L2_CONNECTED) {
		LOG_INF("L2 link UP");
		k_sem_give(&link_up_sem);
	} else if (mgmt_event == NET_EVENT_L2_DISCONNECTED) {
		LOG_WRN("L2 link DOWN");
	}
#else
	/* もしこの分岐に入るなら、prj.conf/Zephyrの世代でL2イベントが無効。
	 * その場合は「代替」を下に書いてある手順で使ってください。
	 */
	ARG_UNUSED(mgmt_event);
#endif
}

static int setup_tcp_server(void)
{
	struct sockaddr_in bind_addr;

	tcp_server_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tcp_server_sock < 0) {
		LOG_ERR("socket() failed: %d", errno);
		return -errno;
	}

	int reuse = 1;
	(void)zsock_setsockopt(tcp_server_sock, SOL_SOCKET, SO_REUSEADDR,
			       &reuse, sizeof(reuse));

	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(TCP_PORT);

	if (zsock_bind(tcp_server_sock, (struct sockaddr *)&bind_addr,
		       sizeof(bind_addr)) < 0) {
		LOG_ERR("bind() failed: %d", errno);
		zsock_close(tcp_server_sock);
		tcp_server_sock = -1;
		return -errno;
	}

	if (zsock_listen(tcp_server_sock, 1) < 0) {
		LOG_ERR("listen() failed: %d", errno);
		zsock_close(tcp_server_sock);
		tcp_server_sock = -1;
		return -errno;
	}

	LOG_INF("TCP server listening on port %d", TCP_PORT);
	return 0;
}

static void set_static_ipv4(struct net_if *iface)
{
	struct in_addr addr;
	struct in_addr netmask;
	struct in_addr gateway;

	/* あなたの環境の固定値（必要に応じて変更） */
	zsock_inet_pton(AF_INET, "192.168.0.84", &addr);
	zsock_inet_pton(AF_INET, "255.255.255.0", &netmask);
	zsock_inet_pton(AF_INET, "192.168.0.1", &gateway);

	struct net_if_addr *ifaddr =
		net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);

	if (!ifaddr) {
		LOG_ERR("Failed to add IPv4 address");
		return;
	}

	/* deprecated warning は出るが動作はOK */
	net_if_ipv4_set_netmask(iface, &netmask);
	net_if_ipv4_set_gw(iface, &gateway);

	LOG_INF("Static IPv4 configured: 192.168.0.84/24 gw 192.168.0.1");
}

/* ---- data sender thread (dummy) ---- */
static void data_logging_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	char buf[256];
	int counter = 0;

	while (1) {
		if (tcp_client_sock < 0) {
			LOG_INF("Waiting for TCP client connection...");
			tcp_client_sock = zsock_accept(tcp_server_sock,
						       (struct sockaddr *)&client_addr,
						       &client_addr_len);
			if (tcp_client_sock < 0) {
				LOG_ERR("accept() failed: %d", errno);
				k_sleep(K_SECONDS(1));
				continue;
			}

			char client_ip[NET_IPV4_ADDR_LEN];
			zsock_inet_ntop(AF_INET, &client_addr.sin_addr,
					client_ip, sizeof(client_ip));
			LOG_INF("Client connected from %s:%d",
				client_ip, ntohs(client_addr.sin_port));
		}

		k_sleep(K_MSEC(10));

		int len = snprintk(buf, sizeof(buf), "%lld,%d\n",
				  k_uptime_get(), counter++);
		int ret = zsock_send(tcp_client_sock, buf, len, 0);
		if (ret < 0) {
			LOG_ERR("send() failed: %d, closing", errno);
			zsock_close(tcp_client_sock);
			tcp_client_sock = -1;
		}
	}
}

K_THREAD_STACK_DEFINE(log_stack, 2048);
static struct k_thread log_thread;
static k_tid_t log_tid;

int main(void)
{
	int ret;

	/* LED setup */
	led_dev = DEVICE_DT_GET(STATUS_LED_GPIO_NODE);
	if (!device_is_ready(led_dev)) {
		while (1) { k_sleep(K_FOREVER); }
	}
	ret = gpio_pin_configure(led_dev, STATUS_LED_PIN, GPIO_OUTPUT_LOW);
	if (ret != 0) {
		while (1) { k_sleep(K_FOREVER); }
	}

	/* Startup blink */
	for (int i = 0; i < 3; i++) {
		gpio_pin_set(led_dev, STATUS_LED_PIN, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set(led_dev, STATUS_LED_PIN, 0);
		k_sleep(K_MSEC(100));
	}

	LOG_INF("Integrated Data Logger starting...");

	/* 重要：PHY/リンク安定待ち（電源投入直後〜flash直後の揺れ対策） */
	k_sleep(K_MSEC(500));

	/* Net mgmt callback: wait for L2 link up */
	net_mgmt_init_event_callback(&mgmt_cb, net_event_handler,
#if defined(NET_EVENT_L2_CONNECTED)
				     NET_EVENT_L2_CONNECTED | NET_EVENT_L2_DISCONNECTED
#else
				     0
#endif
	);
	net_mgmt_add_event_callback(&mgmt_cb);

	struct net_if *iface = net_if_get_default();
	if (!iface) {
		LOG_ERR("No network interface found");
		while (1) {
			gpio_pin_toggle(led_dev, STATUS_LED_PIN);
			k_sleep(K_MSEC(100));
		}
	}

	LOG_INF("Bringing network interface up...");
	net_if_up(iface);
	k_sleep(K_MSEC(200));
	if (!mdio_scan()) {
		LOG_WRN("No PHY on MDIO; trying PHY reset and rescan");
		phy_reset_pulse();
		k_sleep(K_MSEC(200));
		mdio_scan();
	}
	set_phy_refclk_output(false);
	dump_phy_status();

	/* If link still down after a short wait, re-init autoneg */
	k_sleep(K_SECONDS(2));
	if (!net_if_is_carrier_ok(iface)) {
		restart_phy_autoneg();
	}
	k_sleep(K_SECONDS(2));
	if (!net_if_is_carrier_ok(iface)) {
		phy_soft_reset_and_autoneg();
	}
	k_sleep(K_SECONDS(2));
	if (!net_if_is_carrier_ok(iface)) {
		LOG_WRN("Link still down; try REF_CLK output mode");
		set_phy_refclk_output(true);
	}
	k_sleep(K_SECONDS(2));
	if (!net_if_is_carrier_ok(iface)) {
		LOG_WRN("Link still down; try forcing 100M full");
		force_phy_100m_full_mdio();
	}

	LOG_INF("Waiting for link up (poll 10s)...");
	if (!wait_for_link_with_poll(iface, 10, 1000)) {
		LOG_ERR("Link did not come up. Retrying net_if_down/up up to 3 times...");
		bool link_ok = false;

		for (int attempt = 0; attempt < 3; attempt++) {
			net_if_down(iface);
			k_sleep(K_MSEC(300));
			net_if_up(iface);

			if (wait_for_link_with_poll(iface, 10, 1000)) {
				link_ok = true;
				break;
			}
		}

		if (!link_ok) {
			LOG_ERR("Link still down. Giving up.");
			while (1) {
				gpio_pin_toggle(led_dev, STATUS_LED_PIN);
				k_sleep(K_MSEC(200));
			}
		}
	}

	/* Configure IP AFTER link is expected up */
	set_static_ipv4(iface);

	/* LED on = network ready */
	gpio_pin_set(led_dev, STATUS_LED_PIN, 1);

	/* Setup TCP server */
	ret = setup_tcp_server();
	if (ret < 0) {
		LOG_ERR("TCP server setup failed");
		while (1) {
			gpio_pin_toggle(led_dev, STATUS_LED_PIN);
			k_sleep(K_MSEC(100));
		}
	}

	/* Start data thread AFTER TCP server is ready */
	log_tid = k_thread_create(&log_thread, log_stack,
				  K_THREAD_STACK_SIZEOF(log_stack),
				  data_logging_thread,
				  NULL, NULL, NULL,
				  7, 0, K_NO_WAIT);
	k_thread_name_set(log_tid, "data_log");

	LOG_INF("System initialized successfully");

	/* heartbeat */
	while (1) {
		k_sleep(K_SECONDS(2));
		gpio_pin_toggle(led_dev, STATUS_LED_PIN);
	}

	return 0;
}
