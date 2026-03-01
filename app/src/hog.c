/** @file
 *  @brief HoG Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <zephyr/drivers/gpio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
	.version = 0x0111, /* HID 1.11 */
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static uint8_t protocol_mode = 0x01; /* Report Protocol Mode */

static uint8_t simulate_input;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	0x05, 0x0c,                    /* Usage Page (Consumer Devices) */
	0x09, 0x01,                    /* Usage (Consumer Control) */
	0xa1, 0x01,                    /* Collection (Application) */
	0x85, 0x01,                    /*   Report ID (1) */
	0x15, 0x00,                    /*   Logical Minimum (0) */
	0x25, 0x01,                    /*   Logical Maximum (1) */
	0x75, 0x01,                    /*   Report Size (1) */
	0x95, 0x06,                    /*   Report Count (6) */
	0x09, 0xe9,                    /*   Usage (Volume Increment) */
	0x09, 0xea,                    /*   Usage (Volume Decrement) */
	0x09, 0xe2,                    /*   Usage (Mute) */
	0x09, 0xcd,                    /*   Usage (Play/Pause) */
	0x09, 0xb5,                    /*   Usage (Scan Next Track) */
	0x09, 0xb6,                    /*   Usage (Scan Previous Track) */
	0x81, 0x02,                    /*   Input (Data,Variable,Absolute) */
	0x95, 0x02,                    /*   Report Count (2) padding */
	0x81, 0x03,                    /*   Input (Constant,Variable,Absolute) */
	0xc0                           /* End Collection */
};


static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	static uint8_t zero_report = 0;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &zero_report, 1);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

#if CONFIG_SAMPLE_BT_USE_AUTHENTICATION
/* Require encryption using authenticated link-key. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#else
/* Require encryption. */
#define SAMPLE_BT_PERM_READ BT_GATT_PERM_READ_ENCRYPT
#define SAMPLE_BT_PERM_WRITE BT_GATT_PERM_WRITE_ENCRYPT
#endif

static ssize_t read_protocol_mode(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr, void *buf,
				  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &protocol_mode,
				 sizeof(protocol_mode));
}

static ssize_t write_protocol_mode(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   const void *buf, uint16_t len, uint16_t offset,
				   uint8_t flags)
{
	if (offset + len > sizeof(protocol_mode)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(&protocol_mode + offset, buf, len);

	return len;
}

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),                                  // 0
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_PROTOCOL_MODE,                      // 1, 2
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_protocol_mode, write_protocol_mode,
			       &protocol_mode),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,            // 3, 4
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,      // 5, 6
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,                             // 7, 8
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       SAMPLE_BT_PERM_READ,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,                                          // 9
		    SAMPLE_BT_PERM_READ | SAMPLE_BT_PERM_WRITE),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,          // 10
			   read_report, NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,                         // 11, 12
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

void hog_init(void)
{
}

static const struct gpio_dt_spec btn_p5 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct gpio_dt_spec btn_p4 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);

extern void led_set_trigger_active(bool active);

static struct gpio_callback cb_p5;
static struct gpio_callback cb_p4;

static struct k_work button_work;
static bool button_is_pressed;

static void button_work_handler(struct k_work *work)
{
	uint8_t report = 0x01; /* Volume UP (Bit 0) */
	int err;

	if (button_is_pressed) {
		led_set_trigger_active(true);
		
		/* Send Volume Up (Bit 0 in the 1-byte report) */
		err = bt_gatt_notify(NULL, &hog_svc.attrs[8], &report, sizeof(report));
		if (err) {
			printk("HID: Vol UP notify failed (err %d)\n", err);
		} else {
			printk("HID: Sent Vol UP\n");
		}
		
		/* Hold for 100ms then send release */
		k_msleep(100);
		
		report = 0x00; /* Released */
		err = bt_gatt_notify(NULL, &hog_svc.attrs[8], &report, sizeof(report));
		if (err) {
			printk("HID: Release notify failed (err %d)\n", err);
		} else {
			printk("HID: Sent Release\n");
		}
		led_set_trigger_active(false);
		
		/* Debounce guard */
		k_msleep(200);
		button_is_pressed = false;
	}
}

static void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	if (button_is_pressed) {
		return; /* Already processing a trigger */
	}

	/* Check if P4 or P5 went LOW (active) */
	if (gpio_pin_get_dt(&btn_p5) || gpio_pin_get_dt(&btn_p4)) {
		/* Note: gpio_pin_get_dt returns 1 for ACTIVE.
		 * Since we use GPIO_ACTIVE_LOW, it returns 1 when pin is GND.
		 */
		button_is_pressed = true;
		k_work_submit(&button_work);
		printk("GPIO: Trigger detected\n");
	}
}

void hog_button_loop(void)
{
	k_work_init(&button_work, button_work_handler);

	gpio_pin_configure_dt(&btn_p5, GPIO_INPUT | btn_p5.dt_flags);
	gpio_pin_configure_dt(&btn_p4, GPIO_INPUT | btn_p4.dt_flags);

	/* We only care about the Falling edge (P5 -> GND) */
	gpio_pin_interrupt_configure_dt(&btn_p5, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_pin_interrupt_configure_dt(&btn_p4, GPIO_INT_EDGE_TO_ACTIVE);

	gpio_init_callback(&cb_p5, button_pressed, BIT(btn_p5.pin));
	gpio_init_callback(&cb_p4, button_pressed, BIT(btn_p4.pin));

	gpio_add_callback(btn_p5.port, &cb_p5);
	gpio_add_callback(btn_p4.port, &cb_p4);

	printk("Single-trigger HID Ready.\n");
	k_sleep(K_FOREVER);
}
