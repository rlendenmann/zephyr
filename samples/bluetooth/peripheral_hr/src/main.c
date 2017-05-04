/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <gatt/hrs.h>
#include <gatt/dis.h>
#include <gatt/bas.h>

#include <sensor.h>

#define DEVICE_NAME		CONFIG_BLUETOOTH_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static struct bt_conn *default_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void start_adv(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully STARTED\n");
}

static void stop_adv(void)
{
	int err;

	err = bt_le_adv_stop();
	if (err) {
		printk("Advertising failed to stop (err %d)\n", err);
		return;
	}

	printk("Advertising successfully STOPPED\n");
}

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
	} else {
		default_conn = bt_conn_ref(conn);
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	hrs_init(0x01);
	bas_init();
	dis_init(CONFIG_SOC, "Manufacturer");
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

#define ODR_RATE 50

struct sample {
	uint32_t ticks;
	struct sensor_value xyz[3];
};

static uint16_t wr;
static struct sample acc[ODR_RATE];
static int adv_to;
static bool check_anym;

static int reset_trig_drdy(struct device *lis2dh)
{
	const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};

	return sensor_trigger_set(lis2dh, (struct sensor_trigger *)&trig,
				  NULL);
}

static int reset_trig_anym(struct device *lis2dh)
{
	const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DELTA,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};

	return sensor_trigger_set(lis2dh, (struct sensor_trigger *)&trig,
				  NULL);
}

static void drdy_handler(struct device *dev, struct sensor_trigger *trigger)
{
	int err;

	acc[wr].ticks = k_cycle_get_32();
	err = sensor_sample_fetch(dev);
	if (unlikely(err < 0)) {
		printk("sample fetch failed err=%d\n", err);
		return;
	}

	sensor_channel_get(dev, trigger->chan, acc[wr++].xyz);
	if (wr >= ODR_RATE) {
		wr = 0;
	}
}

static int set_trig_drdy(struct device *lis2dh)
{
	const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};
	const struct sensor_value rate = {
		.val1 = ODR_RATE,
		.val2 = 0,
	};
	int err;

	err = sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ,
				 SENSOR_ATTR_SAMPLING_FREQUENCY,
				 &rate);
	if (err < 0) {
		return err;
	}

	err = sensor_trigger_set(lis2dh,
				    (struct sensor_trigger *)&trig,
				    drdy_handler);
	return err;
}

static void anym_handler(struct device *dev, struct sensor_trigger *trigger)
{
	int err;

	printk("--ANY MOTION--\n");

	err = reset_trig_anym(dev);
	if (unlikely(err < 0)) {
		printk("Stop any trigger failed (err %d)", err);
	}

	err = sensor_sample_fetch(dev);
	if (unlikely((err != -EBADMSG) && (err < 0))) {
		printk("sample fetch failed err=%d\n", err);
		return;
	}

	sensor_channel_get(dev, trigger->chan, acc[wr++].xyz);
	if (wr >= ODR_RATE) {
		wr = 0;
	}

	start_adv();
	adv_to = 3;
	check_anym = false;

	err = set_trig_drdy(dev);
	if (unlikely(err < 0)) {
		printk("set_trig_drdy failed with err=%d", err);
	}
}

static int set_trig_anym(struct device *lis2dh)
{
	const struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DELTA,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};
	struct sensor_value attr_val;
	struct sensor_value s[3];
	int err;

	do {
		err = sensor_sample_fetch(lis2dh);
		k_sleep(MSEC_PER_SEC/ODR_RATE);
		printk(".");
	} while (err != 0);
	sensor_channel_get(lis2dh, trig.chan, s);
	printk("%10u: err: %d x.x=%2d.%6d y.y=%2d.%6d z.z=%2d.%6d m/s^2\n",
	       k_cycle_get_32(), err,
	       s[0].val1, s[0].val2,
	       s[1].val1, s[1].val2,
	       s[2].val1, s[2].val2);

	/* z + 10% */
	attr_val.val1 = s[2].val1;
	attr_val.val2 = s[2].val2 + s[2].val2 / 10 + s[2].val1 * 1000000 / 10;
	if (attr_val.val2 > 999999) {
		attr_val.val1 += 1;
		attr_val.val2 -= 1000000;
	}
	err = sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ,
				 SENSOR_ATTR_SLOPE_TH, &attr_val);
	if (err < 0) {
		return err;
	}

	/* 0 -> immediate trig */
	attr_val.val1 = 1;
	attr_val.val2 = 0;
	err = sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ,
				 SENSOR_ATTR_SLOPE_DUR, &attr_val);
	if (err < 0) {
		return err;
	}

	err = sensor_trigger_set(lis2dh, (struct sensor_trigger *)&trig,
				 anym_handler);
	return err;
}

void main(void)
{
	int err;
	struct device *lis2dh;
	int n = 0;
	int i;

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);

	printk("--> Get sensor LIS2DH\n");
	lis2dh = device_get_binding(CONFIG_LIS2DH_NAME);
	if (!lis2dh) {
		printk("-->Could not find %s\n", CONFIG_LIS2DH_NAME);
	} else {
		const struct sensor_value rate = {
			.val1 = ODR_RATE,
			.val2 = 0,
		};

		err = sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ,
					 SENSOR_ATTR_SAMPLING_FREQUENCY,
					 &rate);
		if (err < 0) {
			printk("Set odr failed err=%d", err);
		}

		//err = set_trig_drdy(lis2dh);

		err = set_trig_anym(lis2dh);
		if (err < 0) {
			printk("Trigger set failed err=%d", err);
		}
	}
	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {

		k_sleep(MSEC_PER_SEC);

		if (adv_to) {
			if (--adv_to == 0) {
				err = reset_trig_drdy(lis2dh);
				if (err < 0) {
					printk("reset_trig_drdy failed (err %d"
					       ")", err);
				}

				stop_adv();

				err = set_trig_anym(lis2dh);
				if (err < 0) {
					printk("set_trig_anym failed (err %d"
					       ")", err);
				}
				check_anym = true;
			}
			for (i = 0; i < ODR_RATE; i++) {
				printk("%10u: x.x=%2d.%6d y.y=%2d.%6d z.z=%2d.%6d "
				       "m/s^2\n", acc[i].ticks,
				       acc[i].xyz[0].val1, acc[i].xyz[0].val2,
				       acc[i].xyz[1].val1, acc[i].xyz[1].val2,
				       acc[i].xyz[2].val1, acc[i].xyz[2].val2);
			}
		}
		printk("--- %d) ---\n", n++);

		/* Heartrate measurements simulation */
		hrs_notify();

		/* Battery level simulation */
		bas_notify();


	}
}
