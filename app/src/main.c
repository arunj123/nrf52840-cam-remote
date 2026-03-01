/* main.c - Application main entry point */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>

/* Exported LED control for HOG */
void led_set_trigger_active(bool active)
{
	static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
	static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

	if (active) {
		gpio_pin_set_dt(&led_green, 0); // Turn off green
		gpio_pin_set_dt(&led_red, 1);   // Turn on red
	} else {
		/* Only restore green if we are actually connected */
		/* For simplicity, we'll just toggle them here */
		gpio_pin_set_dt(&led_red, 0);
		gpio_pin_set_dt(&led_green, 1);
	}
}

#include "hog.h"

#define LED0_NODE DT_ALIAS(led0) // Green
#define LED1_NODE DT_ALIAS(led1) // Red
#define LED2_NODE DT_ALIAS(led2) // Blue

static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static void leds_init(void)
{
	gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		      0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s, err 0x%02x %s\n", addr,
		       err, bt_hci_err_to_str(err));
		return;
	}

	printk("Connected %s\n", addr);
	gpio_pin_set_dt(&led_green, 1);
	gpio_pin_set_dt(&led_blue, 0);

	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		printk("Failed to set security\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s, reason 0x%02x %s\n", addr,
	       reason, bt_hci_err_to_str(reason));
	gpio_pin_set_dt(&led_green, 0);

	/* Restart advertising so the device is discoverable again */
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Re-advertising failed (err %d)\n", err);
	} else {
		printk("Re-advertising started\n");
	}
	gpio_pin_set_dt(&led_blue, 1);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %s(%d)\n", addr, level,
		       bt_security_err_to_str(err), err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	hog_init();

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
		printk("Settings loaded\n");
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
	gpio_pin_set_dt(&led_blue, 1);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

int main(void)
{
	int err;

	printk("Starting Camera Remote...\n");
	leds_init();

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_conn_auth_cb_register(&auth_cb_display);
	printk("Authentication callbacks registered\n");

	hog_button_loop();
	return 0;
}
