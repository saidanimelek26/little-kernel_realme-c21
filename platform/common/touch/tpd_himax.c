#include <string.h>
#include <printf.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>
#include <platform/timer.h>
#include "tpd_himax.h"

/*
 * Himax HX83112A register map (I2C register address space).
 * See drivers/input/touchscreen/himax_hx83112b.c in mainline Linux.
 */
#define HIMAX_REG_AHB_ADDR		0x00
#define HIMAX_REG_AHB_RDATA		0x08
#define HIMAX_REG_AHB_DIR		0x0c
#define HIMAX_REG_EVENT_STACK		0x30

#define HIMAX_AHB_CMD_READ		0x00

#define HIMAX_REG_ICID			0x900000d0
#define HIMAX_REG_FW_EVENT_STACK	0x90060000

#define HIMAX_EVENT_LEN			56
#define HIMAX_EVENT_CHUNK		8
#define HIMAX_MAX_POINTS		10
#define HIMAX_INVALID_COORD		0xffff

#define HIMAX_PRODUCT_ID_83112		0x831100	/* HX83112A/B */
#define HIMAX_PRODUCT_ID_83100A		0x83100a

/* The 56 byte frame is protected by a simple 8 bit checksum. */
#define HIMAX_CHECK_CHECKSUM		1

/* Event stack access modes.  Mode 0 is the HX83112A/B path (I2C register
 * 0x30), mode 1 is the HX83100A path (AHB window).  The driver starts in
 * mode 0 and switches automatically when the frame checksum keeps failing,
 * which happens when the controller does not implement the 0x30 window
 * the way the mainline driver expects. */
#define HIMAX_EVENT_MODE_STACK		0
#define HIMAX_EVENT_MODE_AHB		1

/* Only used for the very first frame of a probe, keeps logs readable. */
#define HIMAX_TPD_DEBUG			0

static mt_i2c himax_i2c;
static int himax_ready;
static u32 himax_pid;
static int himax_event_mode = HIMAX_EVENT_MODE_STACK;
static int himax_bad_frames;

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

static int himax_read_reg(u8 reg, u8 *data, u32 len)
{
	u8 buf[HIMAX_EVENT_CHUNK];
	int ret;

	if (len > sizeof(buf))
		return -1;

	buf[0] = reg;
	ret = i2c_write_read(&himax_i2c, buf, 1, len);
	if (ret != I2C_OK)
		return -1;

	memcpy(data, buf, len);
	return 0;
}

static int himax_ahb_read(u32 addr, u8 *data, u32 len)
{
	u8 buf[4];
	int ret;

	if (len == 0 || len > sizeof(buf))
		return -1;

	if (himax_write_u32(HIMAX_REG_AHB_ADDR, addr))
		return -1;

	buf[0] = HIMAX_REG_AHB_DIR;
	buf[1] = HIMAX_AHB_CMD_READ;
	if (i2c_write(&himax_i2c, buf, 2))
		return -1;

	buf[0] = HIMAX_REG_AHB_RDATA;
	ret = i2c_write_read(&himax_i2c, buf, 1, len);
	if (ret != I2C_OK)
		return -1;

	memcpy(data, buf, len);
	return 0;
}

static void himax_hw_reset(int rst_gpio)
{
	if (rst_gpio < 0)
		return;

	/* Same sequence as the mainline driver: assert for 20 ms, release,
	 * then give the controller ~1 ms before the first transfer. */
	mt_set_gpio_mode(rst_gpio, GPIO_MODE_00);
	mt_set_gpio_dir(rst_gpio, GPIO_DIR_OUT);
	mt_set_gpio_out(rst_gpio, GPIO_OUT_ONE);
	mdelay(20);
	mt_set_gpio_out(rst_gpio, GPIO_OUT_ZERO);
	mdelay(2);
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
	if ((id & 0xffff00) == HIMAX_PRODUCT_ID_83112)
		return 1;
	if (id == HIMAX_PRODUCT_ID_83100A)
		return 1;

	return 0;
}

int himax_tpd_probe(int bus, u8 addr, int rst_gpio, u32 *product_id)
{
	u32 id = 0;

	himax_ready = 0;
	memset(&himax_i2c, 0, sizeof(himax_i2c));

	himax_i2c.id = bus;
	himax_i2c.addr = addr;
	himax_i2c.mode = FS_MODE;
	himax_i2c.speed = 400;
	himax_i2c.pushpull = FALSE;

	himax_hw_reset(rst_gpio);

	if (himax_read_product_id(&id))
		return -1;

	if (product_id)
		*product_id = id;

#if HIMAX_TPD_DEBUG
	dprintf(CRITICAL, "tpd: i2c%d addr 0x%x id 0x%x\n", bus, addr, id);
#endif

	if (!himax_product_id_ok(id))
		return -1;

	himax_pid = id;
	himax_ready = 1;

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
	int offset;

	if (himax_event_mode == HIMAX_EVENT_MODE_AHB) {
		/* HX83100A style access: read the FW event stack through the
		 * AHB window, 4 bytes at a time (LK FIFO limit). */
		for (offset = 0; offset < HIMAX_EVENT_LEN; offset += 4) {
			if (himax_ahb_read(HIMAX_REG_FW_EVENT_STACK + offset,
					   ev + offset, 4))
				return -1;
		}
		return 0;
	}

	/* HX83112A/B style access: the 56 byte event is mapped in the I2C
	 * register space starting at 0x30.  Read it back in 8 byte chunks. */
	for (offset = 0; offset < HIMAX_EVENT_LEN; offset += HIMAX_EVENT_CHUNK) {
		if (himax_read_reg(HIMAX_REG_EVENT_STACK + offset,
				   ev + offset, HIMAX_EVENT_CHUNK))
			return -1;
	}

	return 0;
}

static int himax_event_checksum_ok(const u8 *ev)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < HIMAX_EVENT_LEN; i++)
		sum += ev[i];

	return (sum & 0x00ff) == 0;
}

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

	if (!himax_event_checksum_ok(ev)) {
		himax_bad_frames++;
		if (himax_bad_frames >= 8) {
			himax_bad_frames = 0;
			himax_event_mode ^= 1;
			dprintf(CRITICAL,
				"tpd: bad checksum, event read mode -> %d\n",
				himax_event_mode);
		}
		return 0;
	}
	himax_bad_frames = 0;

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
