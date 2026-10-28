#include <string.h>
#include <printf.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>
#include <platform/timer.h>
#include "tpd_himax.h"

/*
 * Himax HX83112A touch controller (Realme C21 / RMX3201).
 *
 * The register map follows drivers/input/touchscreen/himax_hx83112b.c in
 * mainline Linux, plus the 4-byte address protocol used by Himax's own
 * Android driver.  Both ways of reaching the internal AHB window are
 * implemented: some HX83112A firmwares only answer one of them.
 *
 *   window : write the 32-bit AHB address to register 0x00, select "read"
 *            in 0x0c, then read the payload from 0x08.
 *   direct : write the 32-bit AHB address as a plain 4-byte transfer and
 *            read the payload right after it.
 */

#define HIMAX_REG_AHB_ADDR		0x00
#define HIMAX_REG_AHB_RDATA		0x08
#define HIMAX_REG_AHB_DIR		0x0c
#define HIMAX_REG_EVENT_STACK		0x30

#define HIMAX_AHB_CMD_READ		0x00

#define HIMAX_REG_ICID			0x900000d0
#define HIMAX_REG_FW_EVENT_STACK	0x90060000

#define HIMAX_EVENT_LEN			56
#define HIMAX_EVENT_CHUNK		4
#define HIMAX_MAX_POINTS		10
#define HIMAX_INVALID_COORD		0xffff

/* Product IDs: HX83112A/B report 0x83112a/0x83112b, HX83100A 0x83100a. */
#define HIMAX_PRODUCT_ID_FAMILY		0x83	/* 0x83xxxx */

/* The 56 byte frame is protected by a simple 8 bit checksum. */
#define HIMAX_CHECK_CHECKSUM		1

/* Event stack access modes.  Mode 0 is the HX83112A/B path (register file
 * at 0x30), mode 1 is the HX83100A path (AHB window at 0x90060000).  The
 * driver starts in mode 0 and switches automatically when the frame
 * checksum keeps failing. */
#define HIMAX_EVENT_MODE_STACK		0
#define HIMAX_EVENT_MODE_AHB		1
#define HIMAX_EVENT_MODE_MAX		2

/* All LK I2C transfers are limited to the 8 byte FIFO. */
#define HIMAX_I2C_BUF			I2C_FIFO_SIZE

static mt_i2c himax_i2c;
static int himax_ready;
static u32 himax_pid;
static int himax_event_mode = HIMAX_EVENT_MODE_STACK;
static int himax_bad_frames;

/*
 * Write a 32-bit value to a byte register (register + 4 little-endian
 * bytes, exactly what regmap does with 8 bit registers/32 bit values).
 */
static int himax_write_u32(u8 reg, u32 value)
{
	u8 buf[5];

	buf[0] = reg;
	buf[1] = value & 0xff;
	buf[2] = (value >> 8) & 0xff;
	buf[3] = (value >> 16) & 0xff;
	buf[4] = (value >> 24) & 0xff;

	return i2c_write(&himax_i2c, buf, sizeof(buf));
}

/* AHB window read (mainline himax_hx83112b protocol). */
static int himax_window_read(u32 addr, u8 *data, u32 len)
{
	u8 buf[HIMAX_I2C_BUF];

	if (len == 0 || len > sizeof(buf))
		return -1;

	if (himax_write_u32(HIMAX_REG_AHB_ADDR, addr))
		return -1;

	buf[0] = HIMAX_REG_AHB_DIR;
	buf[1] = HIMAX_AHB_CMD_READ;
	if (i2c_write(&himax_i2c, buf, 2))
		return -1;

	buf[0] = HIMAX_REG_AHB_RDATA;
	if (i2c_write_read(&himax_i2c, buf, 1, len) != I2C_OK)
		return -1;

	memcpy(data, buf, len);
	return 0;
}

/* AHB read through the 4-byte address protocol used by the vendor driver. */
static int himax_direct_read(u32 addr, u8 *data, u32 len)
{
	u8 buf[HIMAX_I2C_BUF];

	if (len == 0 || len > sizeof(buf) - 4)
		return -1;

	buf[0] = addr & 0xff;
	buf[1] = (addr >> 8) & 0xff;
	buf[2] = (addr >> 16) & 0xff;
	buf[3] = (addr >> 24) & 0xff;

	if (i2c_write_read(&himax_i2c, buf, 4, len) != I2C_OK)
		return -1;

	memcpy(data, buf, len);
	return 0;
}

/* Try both AHB access protocols. */
static int himax_ahb_read(u32 addr, u8 *data, u32 len)
{
	if (himax_window_read(addr, data, len) == 0)
		return 0;

	return himax_direct_read(addr, data, len);
}

/*
 * Reset pulse.  Same order as the mainline driver: assert, wait, release.
 * Skipped when the board file does not know the GPIO (the pin is then left
 * exactly as the preloader/kernel left it, which is what the stock LK
 * does).
 */
static void himax_hw_reset(int rst_gpio)
{
	if (rst_gpio < 0)
		return;

	/*
	 * Same pulse as the SPI transport: assert (low) for 5 ms, release
	 * (high) and let the firmware boot.  The line must be left released,
	 * otherwise the controller stays in reset for the rest of the boot.
	 */
	mt_set_gpio_mode(rst_gpio, GPIO_MODE_00);
	mt_set_gpio_dir(rst_gpio, GPIO_DIR_OUT);
	mt_set_gpio_out(rst_gpio, GPIO_OUT_ZERO);
	mdelay(5);
	mt_set_gpio_out(rst_gpio, GPIO_OUT_ONE);
	mdelay(30);
}

static int himax_read_product_id(u32 *product_id)
{
	u8 data[4];

	if (himax_ahb_read(HIMAX_REG_ICID, data, sizeof(data)))
		return -1;

	*product_id = ((u32)data[0]) |
		      ((u32)data[1] << 8) |
		      ((u32)data[2] << 16) |
		      ((u32)data[3] << 24);
	*product_id >>= 8;

	return 0;
}

static int himax_product_id_ok(u32 id)
{
	/* Every Himax touch controller reports 0x83xxxx. */
	if ((id >> 16) == HIMAX_PRODUCT_ID_FAMILY)
		return 1;

	return 0;
}

/* Does anything ACK at the configured address?  A read/write to the AHB
 * address register is harmless and only used as a liveness check. */
static int himax_i2c_alive(void)
{
	u8 buf[2];

	buf[0] = HIMAX_REG_AHB_ADDR;
	buf[1] = 0x00;

	return i2c_write(&himax_i2c, buf, sizeof(buf)) == I2C_OK;
}

int himax_tpd_probe(int bus, u8 addr, int rst_gpio, u32 *product_id)
{
	u32 id = 0;
	int id_ret;

	himax_ready = 0;
	memset(&himax_i2c, 0, sizeof(himax_i2c));

	himax_i2c.id = bus;
	himax_i2c.addr = addr;
	himax_i2c.mode = FS_MODE;
	/* Probe at 100 kHz: the touch bus is not always muxed/tuned the same
	 * way as the PMIC bus, and a slower clock is far more tolerant. */
	himax_i2c.speed = 100;
	himax_i2c.pushpull = FALSE;

	himax_hw_reset(rst_gpio);

	/* The controller firmware needs a moment after power-on/reset before
	 * the register window answers. */
	mdelay(rst_gpio >= 0 ? 30 : 5);

	id_ret = himax_read_product_id(&id);

	dprintf(CRITICAL, "tpd: i2c%d addr 0x%02x: id %s (0x%x)\n",
		bus, addr, id_ret ? "read failed" : "read ok", id);

	if (id_ret == 0 && himax_product_id_ok(id)) {
		/* Known Himax controller. */
	} else if (himax_i2c_alive()) {
		/* Something answers at the Himax address but the ID did not
		 * look familiar; keep it and let the event reader decide. */
		dprintf(CRITICAL,
			"tpd: i2c%d addr 0x%02x: unexpected id, using it anyway\n",
			bus, addr);
	} else {
		return -1;
	}

	if (product_id)
		*product_id = id;

	himax_pid = id;
	himax_event_mode = HIMAX_EVENT_MODE_STACK;
	himax_bad_frames = 0;
	himax_ready = 1;

	/* The controller is configured, use the normal bus clock from now on. */
	himax_i2c.speed = 400;

	return 0;
}

int himax_tpd_start(int bus, u8 addr)
{
	if (!himax_ready)
		return -1;

	himax_i2c.id = bus;
	himax_i2c.addr = addr;

	return 0;
}

static int himax_read_event_raw(u8 *ev)
{
	u32 base;
	int offset;

	/*
	 * Both chips expose the 56 byte event frame as an AHB region:
	 *
	 *   HX83112A/B : 0x30..0x67   (HIMAX_AHB_ADDR_EVENT_STACK)
	 *   HX83100A   : 0x90060000
	 *
	 * The mainline driver reads the whole frame in one burst; the LK I2C
	 * controller is limited to the 8 byte FIFO, so the frame is fetched
	 * in 4 byte pieces - the same transfers the mainline driver uses for
	 * short reads.
	 */
	if (himax_event_mode == HIMAX_EVENT_MODE_AHB)
		base = HIMAX_REG_FW_EVENT_STACK;
	else
		base = HIMAX_REG_EVENT_STACK;

	for (offset = 0; offset < HIMAX_EVENT_LEN; offset += HIMAX_EVENT_CHUNK) {
		if (himax_ahb_read(base + offset, ev + offset, HIMAX_EVENT_CHUNK))
			return -1;
	}

	return 0;
}

#if HIMAX_CHECK_CHECKSUM
static int himax_event_checksum_ok(const u8 *ev)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < HIMAX_EVENT_LEN; i++)
		sum += ev[i];

	return (sum & 0x00ff) == 0;
}
#endif

int himax_tpd_read_event(int *x, int *y)
{
	u8 ev[HIMAX_EVENT_LEN];
	int num_points;
	int i;

	if (!himax_ready)
		return -1;

	if (himax_read_event_raw(ev))
		return -1;

	num_points = ev[52];
	if (num_points == 0xff) {
		himax_bad_frames = 0;
		return 0;
	}

#if HIMAX_CHECK_CHECKSUM
	if (!himax_event_checksum_ok(ev)) {
		himax_bad_frames++;
		if (himax_bad_frames >= 8) {
			himax_bad_frames = 0;
			himax_event_mode++;
			if (himax_event_mode >= HIMAX_EVENT_MODE_MAX)
				himax_event_mode = HIMAX_EVENT_MODE_STACK;
			dprintf(CRITICAL,
				"tpd: bad checksum, event read mode -> %d\n",
				himax_event_mode);
		}
		return 0;
	}
	himax_bad_frames = 0;
#endif

	num_points &= 0x0f;
	if (!num_points)
		return 0;

	for (i = 0; i < num_points && i < HIMAX_MAX_POINTS; i++) {
		u16 px = ((u16)ev[i * 4] << 8) | ev[i * 4 + 1];
		u16 py = ((u16)ev[i * 4 + 2] << 8) | ev[i * 4 + 3];

		if (px == HIMAX_INVALID_COORD || py == HIMAX_INVALID_COORD)
			continue;

		*x = px;
		*y = py;
		return 1;
	}

	return 0;
}

u32 himax_tpd_product_id(void)
{
	return himax_pid;
}
