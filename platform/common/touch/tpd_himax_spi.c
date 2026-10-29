/*
 * Himax HX83102D touch controller, SPI transport (Realme C21 / RMX3201).
 *
 * Hardware facts come from the official Realme C21 kernel sources:
 *
 *   arch/arm64/boot/dts/mediatek/oppo6765_20701.dts
 *     touch is a child of &spi2 (0x11012000), chip select 0
 *     spi-max-frequency = <15000000>
 *     reset-gpio = <&pio 178 0>
 *     irq-gpio   = <&pio 0 0x2002>
 *     pins: GPIO17 = SPI2_MO, GPIO18 = SPI2_CLK,
 *           GPIO19 = SPI2_MI, GPIO20 = SPI2_CSB  (all function 2)
 *     "noflash_support", "lcd_trigger_load_tp_fw_support"
 *
 *   drivers/input/touchscreen/oppo_touchscreen/Himax/hx83102d_noflash/
 *     hx83102d_noflash.c  (the sequences ported below)
 *
 * WHY THIS FILE DOWNLOADS FIRMWARE
 * --------------------------------
 * The HX83102D is a zero-flash TDDI: its touch firmware lives in SRAM only
 * and is gone after every reset of the touch reset line (GPIO178).  The LCM
 * init of this device pulses GPIO178 together with the panel reset, and on
 * Android the kernel reloads the firmware right afterwards through
 * lcd_queue_load_tp_fw().  LK never did that, so by the time the boot menu
 * ran, the controller answered on SPI (ID register, register echo) but had
 * no firmware, the event stack stayed empty and touch never worked.
 *
 * Bring-up sequence (himax_mcu_0f_operation / himax_mcu_firmware_update_0f
 * / hx83102d_nf_zf_part_info / himax_sense_on(0)):
 *
 *   1. hardware reset pulse on GPIO178
 *   2. system reset (0x31=0x27, 0x32=0x95, 0x31=0) and "sense off"
 *      (enter safe mode so SRAM can be written)
 *   3. main block   : fw[0 .. size0)      -> SRAM 0x20000000, HW CRC == 0
 *   4. config blocks: all partitions 1..n -> one image at the lowest
 *                     address, HW CRC == software CRC
 *   5. overlay      : "border" overlay section -> SRAM, HW CRC == 0, then
 *                     handshake word 0x10007ffc <- 0x66
 *   6. clear the mode switch word 0x10007294
 *   7. disable flash reload (0x10007f00 <- 0x9aa9)
 *   8. sense on: system reset again and wait for the firmware handshake
 *      (0x90000048 <- 0xec, reads back 0xec 0x01)
 *
 * Wire protocol (himax_bus_read / himax_bus_write, SPI mode 3, MSB first):
 *     write : { 0xF2, reg, data... }
 *     read  : { 0xF3, reg, 0x00 } followed by N clocked-in bytes
 * AHB window: reg 0x00 = address (LE32), 0x0c = direction, 0x08 = data.
 * Register 0x00 written with { addr(4), data... } is a burst write into the
 * AHB address space (auto increment when 0x0d bit0 is set).
 *
 * Event frame (56 bytes at register 0x30, HX_TOUCH_INFO_POINT_CNT = 52):
 *     point i      : byte i*4 .. i*4+3, x = hi<<8 | lo, y = hi<<8 | lo
 *     byte 52      : number of points (0xff = none, else value & 0x0f)
 *     checksum     : (sum of the 56 bytes) & 0xff == 0
 */

#include <string.h>
#include <printf.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_gpio.h>
#include <platform/timer.h>
#include <spi.h>
#include "tpd_himax.h"

/* ------------------------------------------------------------------ */
/* Hardware description                                               */
/* ------------------------------------------------------------------ */

#define HIMAX_SPI_REG_BASE	0x11012000	/* spi2 */
#define HIMAX_SPI_PAD_SELECT	0		/* all signals from SPI pads */
/*
 * The kernel runs this link at 15 MHz.  LK leaves the pad drive strength at
 * its reset default, so stay well below that: the firmware download is
 * ~130 KB and takes a fraction of a second even at this speed.
 */
#define HIMAX_SPI_SPEED_HZ	9600000
#define HIMAX_SPI_MODE		(SPI_CPHA | SPI_CPOL)	/* SPI mode 3 */

#define HIMAX_SPI_RST_GPIO	GPIO178
#define HIMAX_SPI_INT_GPIO	GPIO0

/* SPI2 gate in the infra clock gating block (INFRA_PDN, bit 9). */
#define HIMAX_INFRA_CG2_CLR	((volatile unsigned int *)0x100010A8)
#define HIMAX_INFRA_CG2_SPI2	(1u << 9)

/* Reset timing, from hx83102d_resetgpio_set() (5 ms per edge). */
#define HIMAX_RST_LOW_MS	5
#define HIMAX_RST_HIGH_MS	20

#define HIMAX_BRINGUP_TRIES	2

/* ------------------------------------------------------------------ */
/* Controller registers / constants                                   */
/* ------------------------------------------------------------------ */

#define HIMAX_REG_AHB_ADDR	0x00
#define HIMAX_REG_AHB_RDATA	0x08
#define HIMAX_REG_AHB_DIR	0x0c
#define HIMAX_REG_AHB_CTRL	0x11	/* 0 = plain register access */
#define HIMAX_REG_BURST_CTRL	0x13
#define HIMAX_REG_BURST_MODE	0x0d
#define HIMAX_REG_EVENT_STACK	0x30

#define HIMAX_AHB_ICID		0x900000d0
#define HIMAX_AHB_SYS_STATE	0x900000e4	/* 0x00 0x02 after system reset */
#define HIMAX_AHB_SAFE_MODE	0x9000005c	/* 0xa5 request, 0x87 granted */
#define HIMAX_AHB_MODE_STATUS	0x900000a8
#define HIMAX_AHB_HANDSHAKE	0x90000048	/* write 0xec, expect 0xec 0x01 */
#define HIMAX_AHB_TCON_RST	0x80020020
#define HIMAX_AHB_ADC_RST	0x80020094
#define HIMAX_AHB_CRC_START	0x80050020
#define HIMAX_AHB_CRC_LEN	0x80050028
#define HIMAX_AHB_CRC_STATUS	0x80050000
#define HIMAX_AHB_CRC_RESULT	0x80050018

#define HIMAX_SRAM_MAIN		0x20000000	/* zf_data_sram_start_addr */
#define HIMAX_AHB_RELOAD_DIS	0x10007f00	/* zf_addr_dis_flash_reload */
#define HIMAX_RELOAD_DIS_VAL	0x00009aa9
#define HIMAX_AHB_MODE_SWITCH	0x10007294	/* zf_data_mode_switch */
#define HIMAX_AHB_OVL_HANDSHAKE	0x10007ffc

#define HIMAX_OVL_BORDER_IDX	2		/* ovl_idx[2] */
#define HIMAX_OVL_BORDER_REPLY	0x66
#define HIMAX_OVL_FAULT		0xff

#define HIMAX_SPI_WRITE		0xf2
#define HIMAX_SPI_READ		0xf3

/* Firmware image layout. */
#define HX64K			0x10000
#define HX1K			0x400
#define HIMAX_MAX_PARTS		16
#define HIMAX_CFG_BUF_SZ	0x4000		/* FW_BIN_16K_SZ */

/* Event frame. */
#define HIMAX_EVENT_LEN		56
#define HIMAX_EVENT_POINTS	52
#define HIMAX_MAX_POINTS	10
#define HIMAX_INVALID_COORD	0xffff
#define HIMAX_BAD_FLIP_LIMIT	8

/* Payload per SRAM write transfer: 2 + 4 + 1016 = 1022 <= MTK packet. */
#define HIMAX_STREAM_PAYLOAD	1016
#define HIMAX_STREAM_HDR	6
#define HIMAX_FIFO_DEPTH	32

/* The image is linked in by hx83102d_fw.S. */
extern const u8 hx83102d_fw_img[];
extern const u8 hx83102d_fw_img_end[];

/*
 * Transfer buffers.  The SPI core switches to DMA above 32 bytes and
 * cleans/invalidates the cache around the buffers, so they are static,
 * zeroed and cache line aligned: nothing else shares a line with them.
 */
static u8 himax_spi_tx[64] __attribute__((aligned(64)));
static u8 himax_spi_rx[64] __attribute__((aligned(64)));
static u8 himax_stream[HIMAX_STREAM_HDR + HIMAX_STREAM_PAYLOAD + 2]
	__attribute__((aligned(64)));
static u8 himax_cfg_buf[HIMAX_CFG_BUF_SZ] __attribute__((aligned(64)));

struct himax_part {
	u32 sram;
	u32 size;
	u32 off;
	int ovl;		/* overlay section number, -1 for plain data */
};

static struct himax_part himax_parts[HIMAX_MAX_PARTS];
static int himax_nparts;
static u32 himax_main_off;
static u32 himax_cfg_base;
static u32 himax_cfg_sz;
static u32 himax_cfg_crc;

static int himax_spi_bus = -1;
static int himax_spi_ready;
static int himax_spi_dead;	/* a transfer failed: controller unusable */
static int himax_spi_no_dma;	/* event reads use chained FIFO frames */
static int himax_wr_fifo;	/* SRAM writes use chained FIFO frames */
static int himax_bad_run;
static u32 himax_spi_pid;

#define DIAG	g_tpd_diag

/* ------------------------------------------------------------------ */
/* Low level bus access                                               */
/* ------------------------------------------------------------------ */

static int himax_spi_xfer(u32 len)
{
	struct spi_transfer xfer;
	int ret;

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = himax_spi_tx;
	xfer.rx_buf = himax_spi_rx;
	xfer.len = len;
	xfer.speed_hz = HIMAX_SPI_SPEED_HZ;
	xfer.cs_change = 1;	/* frame finished: release CS */
	xfer.tick_delay = 0;

	ret = spi_sync(himax_spi_bus, &xfer);
	if (ret) {
		himax_spi_dead = 1;
		DIAG.xfer_errors++;
	}

	return ret;
}

/* { 0xF2, reg, data... } */
static int himax_bus_write(u8 reg, const u8 *data, u32 len)
{
	u32 i;

	if (len + 2 > sizeof(himax_spi_tx))
		return -1;

	himax_spi_tx[0] = HIMAX_SPI_WRITE;
	himax_spi_tx[1] = reg;
	for (i = 0; i < len; i++)
		himax_spi_tx[i + 2] = data[i];

	return himax_spi_xfer(len + 2);
}

/* { 0xF3, reg, 0x00 } + len dummy bytes, payload from rx[3]. */
static int himax_bus_read(u8 reg, u8 *data, u32 len)
{
	u32 i;

	if (len + 3 > sizeof(himax_spi_tx))
		return -1;

	memset(himax_spi_tx, 0, len + 3);
	himax_spi_tx[0] = HIMAX_SPI_READ;
	himax_spi_tx[1] = reg;
	himax_spi_tx[2] = 0x00;

	if (himax_spi_xfer(len + 3))
		return -1;

	/*
	 * The chip shifts the payload out on the bytes that follow the
	 * header, so rx[3] is the first payload byte; rx[0..2] carry the
	 * header echo.
	 */
	for (i = 0; i < len; i++)
		data[i] = himax_spi_rx[i + 3];

	return 0;
}

static int himax_burst_enable(u8 auto_add)
{
	u8 v;

	v = 0x31;
	if (himax_bus_write(HIMAX_REG_BURST_CTRL, &v, 1))
		return -1;

	v = 0x10 | (auto_add & 0x01);
	if (himax_bus_write(HIMAX_REG_BURST_MODE, &v, 1))
		return -1;

	return 0;
}

/* AHB window read, exactly as himax_register_read() does it (len <= 32). */
static int himax_ahb_read(u32 addr, u8 *data, u32 len)
{
	u8 tmp[4];
	int burst = len > 4;

	if (len == 0 || len > 32)
		return -1;

	if (himax_burst_enable(burst ? 1 : 0))
		return -1;

	tmp[0] = (u8)(addr);
	tmp[1] = (u8)(addr >> 8);
	tmp[2] = (u8)(addr >> 16);
	tmp[3] = (u8)(addr >> 24);

	if (himax_bus_write(HIMAX_REG_AHB_ADDR, tmp, 4))
		return -1;

	tmp[0] = 0x00;
	if (himax_bus_write(HIMAX_REG_AHB_DIR, tmp, 1))
		return -1;

	if (himax_bus_read(HIMAX_REG_AHB_RDATA, data, len))
		return -1;

	if (burst) {
		if (himax_burst_enable(0))
			return -1;
	}

	return 0;
}

/* One 32 bit AHB word, himax_flash_write_burst(): { LE32(addr), LE32(val) }. */
static int himax_flash_write_burst(u32 addr, u32 val)
{
	u8 b[8];
	int i;

	for (i = 0; i < 4; i++) {
		b[i] = (u8)(addr >> (8 * i));
		b[i + 4] = (u8)(val >> (8 * i));
	}

	return himax_bus_write(HIMAX_REG_AHB_ADDR, b, sizeof(b));
}

/* himax_register_write(addr, 4, data): burst off, one word. */
static int himax_reg_write(u32 addr, u32 val)
{
	if (himax_burst_enable(0))
		return -1;

	return himax_flash_write_burst(addr, val);
}

static int himax_reg_read32(u32 addr, u32 *val)
{
	u8 d[4];

	if (himax_ahb_read(addr, d, 4))
		return -1;

	*val = (u32)d[0] | ((u32)d[1] << 8) | ((u32)d[2] << 16) |
	       ((u32)d[3] << 24);

	return 0;
}

/*
 * Burst write of up to HIMAX_STREAM_PAYLOAD bytes into the AHB space:
 *     { 0xF2, 0x00, LE32(addr), data... }
 * (himax_flash_write_burst_length).  Needs himax_burst_enable(1) first.
 *
 * Normally a single DMA transfer.  If DMA ever corrupts the data (caught by
 * the hardware CRC check of the caller) the same frame is clocked out as a
 * chain of 32 byte FIFO transfers with CS held in between.
 */
static int himax_stream_write(u32 addr, const u8 *data, u32 len)
{
	struct spi_transfer xfer;
	u32 total;
	u32 off;
	u32 n;
	u32 i;

	if (len == 0 || len > HIMAX_STREAM_PAYLOAD)
		return -1;

	himax_stream[0] = HIMAX_SPI_WRITE;
	himax_stream[1] = HIMAX_REG_AHB_ADDR;
	himax_stream[2] = (u8)(addr);
	himax_stream[3] = (u8)(addr >> 8);
	himax_stream[4] = (u8)(addr >> 16);
	himax_stream[5] = (u8)(addr >> 24);
	for (i = 0; i < len; i++)
		himax_stream[HIMAX_STREAM_HDR + i] = data[i];

	total = len + HIMAX_STREAM_HDR;

	if (!himax_wr_fifo) {
		memset(&xfer, 0, sizeof(xfer));
		xfer.tx_buf = himax_stream;
		xfer.rx_buf = NULL;
		xfer.len = total;
		xfer.speed_hz = HIMAX_SPI_SPEED_HZ;
		xfer.cs_change = 1;
		xfer.tick_delay = 0;

		if (spi_sync(himax_spi_bus, &xfer)) {
			himax_spi_dead = 1;
			DIAG.xfer_errors++;
			return -1;
		}
		return 0;
	}

	for (off = 0; off < total; off += n) {
		n = total - off;
		if (n > HIMAX_FIFO_DEPTH)
			n = HIMAX_FIFO_DEPTH;

		memset(&xfer, 0, sizeof(xfer));
		xfer.tx_buf = himax_stream + off;
		xfer.rx_buf = NULL;
		xfer.len = n;
		xfer.speed_hz = HIMAX_SPI_SPEED_HZ;
		xfer.cs_change = (off + n == total) ? 1 : 0;
		xfer.tick_delay = 0;

		if (spi_sync(himax_spi_bus, &xfer)) {
			himax_spi_dead = 1;
			DIAG.xfer_errors++;
			return -1;
		}
	}

	return 0;
}

/* himax_mcu_write_sram_0f(): burst on, then chunks with a rising address. */
static int himax_sram_write(u32 addr, const u8 *data, u32 len)
{
	u32 off = 0;
	u32 n;

	if (himax_burst_enable(1))
		return -1;

	while (off < len) {
		n = len - off;
		if (n > HIMAX_STREAM_PAYLOAD)
			n = HIMAX_STREAM_PAYLOAD;

		if (himax_stream_write(addr + off, data + off, n))
			return -1;

		off += n;
		udelay(100);
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Pin mux, clock gate, reset                                         */
/* ------------------------------------------------------------------ */

static void himax_spi_pads_init(void)
{
	/*
	 * Make sure the SPI2 block is clocked.  Writing the clear register is
	 * idempotent: it only ungates the bit.
	 */
	mt_reg_sync_writel(HIMAX_INFRA_CG2_SPI2, HIMAX_INFRA_CG2_CLR);

	/* ctp_ts_spi_mode: all four signals on function 2 (SPI2). */
	mt_set_gpio_mode(GPIO17, GPIO_MODE_02);	/* SPI2_MO  */
	mt_set_gpio_mode(GPIO18, GPIO_MODE_02);	/* SPI2_CLK */
	mt_set_gpio_mode(GPIO19, GPIO_MODE_02);	/* SPI2_MI  */
	mt_set_gpio_mode(GPIO20, GPIO_MODE_02);	/* SPI2_CSB */

	/* IRQ line: input with pull-up, only sampled for diagnostics. */
	mt_set_gpio_mode(HIMAX_SPI_INT_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(HIMAX_SPI_INT_GPIO, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(HIMAX_SPI_INT_GPIO, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(HIMAX_SPI_INT_GPIO, GPIO_PULL_UP);
}

static void himax_hw_reset(void)
{
	mt_set_gpio_mode(HIMAX_SPI_RST_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(HIMAX_SPI_RST_GPIO, GPIO_DIR_OUT);

	mt_set_gpio_out(HIMAX_SPI_RST_GPIO, GPIO_OUT_ZERO);
	mdelay(HIMAX_RST_LOW_MS);
	mt_set_gpio_out(HIMAX_SPI_RST_GPIO, GPIO_OUT_ONE);
	mdelay(HIMAX_RST_HIGH_MS);
}

static int himax_int_level(void)
{
	int v = mt_get_gpio_in(HIMAX_SPI_INT_GPIO);

	return v ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Link bring-up                                                      */
/* ------------------------------------------------------------------ */

/*
 * himax_interface_on(): knock on the bus, then enable continuous burst and
 * check that both registers echo what was written.  This proves that the
 * chip decoded the command bytes (MSB-first bit order, SPI mode) and that
 * MISO is really driven.
 *
 * Return: 0 link alive, 1 no echo, -1 transfer failure.
 */
static int himax_interface_on(void)
{
	u8 d[4];
	u8 v;
	u8 r13 = 0;
	u8 r0d = 0;
	int cnt = 0;

	if (himax_bus_read(HIMAX_REG_AHB_RDATA, d, 4))
		return -1;

	do {
		v = 0x31;
		if (himax_bus_write(HIMAX_REG_BURST_CTRL, &v, 1))
			return -1;

		v = 0x10;
		if (himax_bus_write(HIMAX_REG_BURST_MODE, &v, 1))
			return -1;

		if (himax_bus_read(HIMAX_REG_BURST_CTRL, &r13, 1))
			return -1;

		if (himax_bus_read(HIMAX_REG_BURST_MODE, &r0d, 1))
			return -1;

		if (r13 == 0x31 && r0d == 0x10)
			return 0;

		mdelay(1);
	} while (++cnt < 10);

	return 1;
}

/* Decode 0x900000d0 into DIAG.id (informational, never fatal). */
static void himax_read_id(void)
{
	u8 d[4];
	u32 id;
	int i;

	if (himax_ahb_read(HIMAX_AHB_ICID, d, sizeof(d)))
		return;

	for (i = 0; i < 4; i++)
		DIAG.id_raw[i] = d[i];

	id = ((u32)d[0]) | ((u32)d[1] << 8) | ((u32)d[2] << 16) |
	     ((u32)d[3] << 24);
	DIAG.id = id >> 8;
}

/* himax_mcu_sys_reset() */
static void himax_sys_reset(void)
{
	u8 d[4];
	u8 v;
	int retry = 0;

	do {
		memset(d, 0, sizeof(d));

		v = 0x27;
		himax_bus_write(0x31, &v, 1);
		v = 0x95;
		himax_bus_write(0x32, &v, 1);
		v = 0x00;
		himax_bus_write(0x31, &v, 1);
		mdelay(1);

		himax_ahb_read(HIMAX_AHB_SYS_STATE, d, 4);
	} while ((d[1] != 0x02 || d[0] != 0x00) && retry++ < 5 &&
		 !himax_spi_dead);
}

/*
 * himax_sense_off(): stop the MCU so that the SRAM can be written.
 * Returns 0 once the controller reports save mode (0x0c).
 */
static int himax_sense_off(void)
{
	u8 d[4] = { 0, 0, 0, 0 };
	u8 v;
	int cnt = 0;

	do {
		if (cnt == 0 ||
		    (d[0] != 0xa5 && d[0] != 0x00 && d[0] != 0x87))
			himax_flash_write_burst(HIMAX_AHB_SAFE_MODE, 0xa5);

		mdelay(1);

		himax_ahb_read(HIMAX_AHB_MODE_STATUS, d, 4);
		if (d[0] != 0x05)
			break;

		mdelay(10);
		himax_ahb_read(HIMAX_AHB_SAFE_MODE, d, 4);
		cnt++;
	} while (d[0] != 0x87 && cnt < 50 && !himax_spi_dead);

	cnt = 0;
	do {
		v = 0x27;
		himax_bus_write(0x31, &v, 1);
		v = 0x95;
		himax_bus_write(0x32, &v, 1);

		himax_ahb_read(HIMAX_AHB_MODE_STATUS, d, 4);

		if (d[0] == 0x0c) {
			/* Reset TCON */
			himax_flash_write_burst(HIMAX_AHB_TCON_RST, 0x00);
			mdelay(1);
			himax_flash_write_burst(HIMAX_AHB_TCON_RST, 0x01);

			/* Reset ADC */
			himax_flash_write_burst(HIMAX_AHB_ADC_RST, 0x00);
			mdelay(1);
			himax_flash_write_burst(HIMAX_AHB_ADC_RST, 0x01);
			return 0;
		}

		mdelay(10);
		himax_hw_reset();
		himax_interface_on();
	} while (cnt++ < 15 && !himax_spi_dead);

	return -1;
}

/* ------------------------------------------------------------------ */
/* Firmware image handling                                            */
/* ------------------------------------------------------------------ */

/* Software CRC used for the config image (himax_mcu_Calculate_CRC_with_AP). */
static u32 himax_sw_crc(const u8 *buf, u32 len)
{
	u32 crc = 0xffffffff;
	u32 i;
	u32 j;

	for (i = 0; i < len / 4; i++) {
		u32 w = (u32)buf[i * 4] | ((u32)buf[i * 4 + 1] << 8) |
			((u32)buf[i * 4 + 2] << 16) |
			((u32)buf[i * 4 + 3] << 24);

		crc ^= w;
		for (j = 0; j < 32; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x82f63b78;
			else
				crc >>= 1;
		}
	}

	return crc;
}

/* hx_parse_bin_cfg_data(): partition table + config image. */
static int himax_fw_parse(const u8 *fw, u32 fw_len)
{
	u32 tbl;
	u32 lo = 0xffffffff;
	u32 hi = 0;
	int hdr1k;
	int part_num;
	int i;

	himax_nparts = 0;

	if (fw_len < HX64K + 0x20)
		return -1;

	hdr1k = (fw[0] == 0 && fw[1] == 0 && fw[2] == 0 && fw[3] == 0 &&
		 fw[4] == 0 && fw[5] == 0 && fw[6] == 0 && fw[7] == 0 &&
		 fw[0x0e] == 0x87);

	tbl = HX64K + (hdr1k ? HX1K : 0);
	himax_main_off = hdr1k ? HX1K : 0;

	if (tbl + 16 > fw_len)
		return -1;

	part_num = fw[tbl + 12];
	if (part_num <= 1 || part_num > HIMAX_MAX_PARTS)
		return -1;
	if (tbl + (u32)part_num * 16 > fw_len)
		return -1;

	for (i = 0; i < part_num; i++) {
		const u8 *b = fw + tbl + i * 16;
		struct himax_part *p = &himax_parts[i];

		p->sram = (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) |
			  ((u32)b[3] << 24);
		p->size = (u32)b[4] | ((u32)b[5] << 8);
		p->off = (u32)b[8] | ((u32)b[9] << 8) | ((u32)b[10] << 16);
		p->ovl = -1;

		/* Overlay sections carry the marker 0x66 0x55 in bytes 14/15. */
		if (b[15] == 0x55 && b[14] == 0x66)
			p->ovl = b[13];
		else if (b[3] == 0x20 && b[2] == 0x00 && b[1] == 0x8c &&
			 b[0] == 0xe0)
			p->ovl = 0x7f;	/* legacy overlay, never downloaded */

		if (p->size && p->off + p->size > fw_len)
			return -1;
	}
	himax_nparts = part_num;

	if (himax_parts[0].sram != HIMAX_SRAM_MAIN)
		return -1;
	if (himax_main_off + himax_parts[0].size > fw_len)
		return -1;

	/* Config image: every plain partition after the main block. */
	for (i = 1; i < part_num; i++) {
		struct himax_part *p = &himax_parts[i];
		u32 base = p->sram & ~3u;

		if (p->ovl >= 0 || p->size == 0)
			continue;
		if (base < lo)
			lo = base;
		if (base + p->size > hi)
			hi = base + p->size;
	}

	if (lo == 0xffffffff || hi <= lo)
		return -1;

	himax_cfg_base = lo;
	himax_cfg_sz = (hi - lo + 3) & ~3u;
	himax_cfg_sz += himax_cfg_sz % 16;	/* as the vendor driver does */

	if (himax_cfg_sz > HIMAX_CFG_BUF_SZ)
		return -1;

	memset(himax_cfg_buf, 0, sizeof(himax_cfg_buf));
	for (i = 1; i < part_num; i++) {
		struct himax_part *p = &himax_parts[i];
		u32 base = p->sram & ~3u;

		if (p->ovl >= 0 || p->size == 0)
			continue;

		memcpy(himax_cfg_buf + (base - lo), fw + p->off, p->size);
	}

	himax_cfg_crc = himax_sw_crc(himax_cfg_buf, himax_cfg_sz);

	return 0;
}

/*
 * himax_hw_check_CRC(): let the controller CRC an SRAM range.
 * Return 0 and *result when the engine finished, -1 otherwise.  (The vendor
 * driver returns 0 on a timeout, which reads as "CRC ok"; here a timeout is
 * an error.)
 */
static int himax_hw_crc(u32 addr, u32 len, u32 *result)
{
	u32 st;
	u32 words = len / 4;
	int cnt;

	if (himax_flash_write_burst(HIMAX_AHB_CRC_START, addr))
		return -1;
	if (himax_flash_write_burst(HIMAX_AHB_CRC_LEN,
				    0x00990000 | (words & 0xffff)))
		return -1;

	for (cnt = 0; cnt < 100; cnt++) {
		if (himax_reg_read32(HIMAX_AHB_CRC_STATUS, &st))
			return -1;

		if ((st & 0x01) != 0x01)
			return himax_reg_read32(HIMAX_AHB_CRC_RESULT, result);

		udelay(50);
	}

	return -1;
}

/* Write a block and keep rewriting it until the HW CRC residue is 0. */
static int himax_sram_write_crc0(u32 addr, const u8 *src, u32 len, int *tries)
{
	u32 crc = 0xffffffff;
	int t;

	for (t = 0; t < 10; t++) {
		if (t == 3 && !himax_wr_fifo) {
			/* DMA writes keep failing the CRC: use FIFO chains. */
			himax_wr_fifo = 1;
			DIAG.dma_fallback = 1;
		}

		if (himax_sram_write(addr, src, len))
			return -1;

		*tries = t + 1;

		if (himax_hw_crc(addr, len, &crc) == 0 && crc == 0)
			return 0;

		if (himax_spi_dead)
			return -1;
	}

	return -2;
}

/* Config image: HW CRC over the whole range must equal the software CRC. */
static int himax_cfg_write(int *tries)
{
	u32 crc = 0;
	int t;

	for (t = 0; t < 10; t++) {
		if (himax_sram_write(himax_cfg_base, himax_cfg_buf,
				     himax_cfg_sz))
			return -1;

		*tries = t + 1;

		if (himax_hw_crc(himax_cfg_base, himax_cfg_sz, &crc) == 0 &&
		    crc == himax_cfg_crc)
			return 0;

		if (himax_spi_dead)
			return -1;
	}

	return -2;
}

/* hx_dis_rload_0f(0): the controller must not reload from flash. */
static int himax_reload_disable(void)
{
	u32 v;
	int t;

	for (t = 0; t < 10; t++) {
		himax_flash_write_burst(HIMAX_AHB_RELOAD_DIS,
					HIMAX_RELOAD_DIS_VAL);

		if (himax_reg_read32(HIMAX_AHB_RELOAD_DIS, &v) == 0 &&
		    v == HIMAX_RELOAD_DIS_VAL)
			return 0;

		mdelay(5);
	}

	return -1;
}

/* himax_hx83102d_reload_to_active(): firmware answers 0xec 0x01. */
static int himax_reload_to_active(void)
{
	u8 d[4];
	int t;

	for (t = 0; t < 10; t++) {
		himax_reg_write(HIMAX_AHB_HANDSHAKE, 0xec);
		mdelay(1);

		memset(d, 0, sizeof(d));
		himax_ahb_read(HIMAX_AHB_HANDSHAKE, d, 4);

		if (d[1] == 0x01 && d[0] == 0xec)
			return 0;

		if (himax_spi_dead)
			break;
	}

	return -1;
}

/* himax_sense_on(0) */
static int himax_sense_on(void)
{
	himax_interface_on();
	himax_reg_write(HIMAX_AHB_SAFE_MODE, 0x00);
	himax_sys_reset();

	return himax_reload_to_active();
}

/*
 * Download the firmware.  Returns 0 when every block was verified by the
 * controller's own CRC engine.
 */
static int himax_fw_download(void)
{
	const u8 *fw = hx83102d_fw_img;
	u32 fw_len = (u32)(hx83102d_fw_img_end - hx83102d_fw_img);
	const struct himax_part *ovl = NULL;
	u32 reply;
	u32 v;
	int ret;
	int t;
	int i;

	DIAG.stage = TPD_STAGE_PARSE;
	if (himax_fw_parse(fw, fw_len)) {
		DIAG.err = TPD_ERR_FW_PARSE;
		return -1;
	}
	DIAG.nparts = himax_nparts;

	DIAG.stage = TPD_STAGE_SENSE_OFF;
	himax_sys_reset();
	DIAG.sense_off_ok = (himax_sense_off() == 0);
	if (himax_spi_dead) {
		DIAG.err = TPD_ERR_XFER;
		return -1;
	}
	/*
	 * The vendor driver ignores the result and goes on with the download;
	 * a controller that really did not stop will fail the CRC below.
	 */

	/* 3. main code block */
	DIAG.stage = TPD_STAGE_FW_MAIN;
	ret = himax_sram_write_crc0(HIMAX_SRAM_MAIN, fw + himax_main_off,
				    himax_parts[0].size, &t);
	DIAG.crc_main_tries = t;
	if (ret) {
		DIAG.err = himax_spi_dead ? TPD_ERR_XFER : TPD_ERR_FW_CRC;
		return -1;
	}

	/* 4. configuration blocks */
	DIAG.stage = TPD_STAGE_FW_CFG;
	ret = himax_cfg_write(&t);
	DIAG.crc_cfg_tries = t;
	if (ret) {
		DIAG.err = himax_spi_dead ? TPD_ERR_XFER : TPD_ERR_FW_CRC;
		return -1;
	}

	/* 5. overlay section ("border"), then the handshake word. */
	DIAG.stage = TPD_STAGE_FW_OVL;
	for (i = 1; i < himax_nparts; i++) {
		if (himax_parts[i].ovl == HIMAX_OVL_BORDER_IDX) {
			ovl = &himax_parts[i];
			break;
		}
	}

	reply = HIMAX_OVL_BORDER_REPLY;
	if (!ovl || ovl->size == 0) {
		reply = HIMAX_OVL_FAULT;
	} else {
		ret = himax_sram_write_crc0(ovl->sram, fw + ovl->off,
					    ovl->size, &t);
		DIAG.crc_ovl_tries = t;
		if (ret) {
			if (himax_spi_dead) {
				DIAG.err = TPD_ERR_XFER;
				return -1;
			}
			reply = HIMAX_OVL_FAULT;
		}
	}

	for (t = 0; t < 10; t++) {
		himax_reg_write(HIMAX_AHB_OVL_HANDSHAKE, reply);
		if (himax_reg_read32(HIMAX_AHB_OVL_HANDSHAKE, &v) == 0 &&
		    v == reply)
			break;
	}

	/* 6. mode switch word */
	himax_reg_write(HIMAX_AHB_MODE_SWITCH, 0x00);
	mdelay(10);

	/* 7. + 8. start the firmware */
	DIAG.stage = TPD_STAGE_FW_START;
	DIAG.reload_dis_ok = (himax_reload_disable() == 0);
	mdelay(10);
	DIAG.handshake_ok = (himax_sense_on() == 0);
	mdelay(30);

	if (himax_spi_dead) {
		DIAG.err = TPD_ERR_XFER;
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Probe                                                              */
/* ------------------------------------------------------------------ */

int himax_spi_tpd_probe(u32 *product_id)
{
	struct mtk_spi_bus_config cfg;
	int attempt;
	int ret;

	himax_spi_ready = 0;
	himax_spi_dead = 0;
	himax_spi_no_dma = 0;
	himax_wr_fifo = 0;
	himax_bad_run = 0;

	memset(&DIAG, 0, sizeof(DIAG));
	DIAG.last_cnt = 0xff;

	DIAG.stage = TPD_STAGE_PADS;
	himax_spi_pads_init();

	if (himax_spi_bus < 0) {
		DIAG.stage = TPD_STAGE_SPI;

		memset(&cfg, 0, sizeof(cfg));
		cfg.spi_mode = HIMAX_SPI_MODE;
		cfg.tick_delay = 0;
		/*
		 * The vendor SPI core runs MSB first.  The LK controller
		 * defaults to LSB first, which bit-reverses every byte on the
		 * wire (0xf3 goes out as 0xcf) and leaves the chip unable to
		 * decode any command.
		 */
		cfg.msb_first = 1;

		himax_spi_bus = init_spi_bus_by_reg(HIMAX_SPI_REG_BASE,
						    HIMAX_SPI_PAD_SELECT,
						    &cfg);
		if (himax_spi_bus < 0) {
			dprintf(CRITICAL, "tpd: spi2 registration failed\n");
			DIAG.err = TPD_ERR_NO_SPI;
			return -1;
		}
	}

	for (attempt = 1; attempt <= HIMAX_BRINGUP_TRIES; attempt++) {
		DIAG.attempts = attempt;
		DIAG.err = TPD_ERR_NONE;
		himax_spi_dead = 0;

		/*
		 * The LCM init already reset this line, which wiped the
		 * firmware.  Reset it again so the ROM is in a known state.
		 */
		DIAG.stage = TPD_STAGE_RESET;
		himax_hw_reset();

		DIAG.stage = TPD_STAGE_LINK;
		ret = himax_interface_on();
		DIAG.link_ok = (ret == 0);
		if (ret < 0) {
			DIAG.err = TPD_ERR_XFER;
			dprintf(CRITICAL,
				"tpd: spi%d transfer failed, controller not clocked?\n",
				himax_spi_bus);
			continue;
		}
		if (ret > 0) {
			DIAG.err = TPD_ERR_NO_CHIP;
			dprintf(CRITICAL, "tpd: spi%d no register echo\n",
				himax_spi_bus);
			continue;
		}

		himax_read_id();
		dprintf(CRITICAL, "tpd: spi%d id 0x%06x (%02x %02x %02x %02x)\n",
			himax_spi_bus, DIAG.id, DIAG.id_raw[0], DIAG.id_raw[1],
			DIAG.id_raw[2], DIAG.id_raw[3]);

		if (himax_fw_download() == 0) {
			DIAG.err = TPD_ERR_NONE;
			break;
		}

		dprintf(CRITICAL, "tpd: fw download failed stage %d err %d\n",
			DIAG.stage, DIAG.err);
	}

	if (DIAG.err != TPD_ERR_NONE)
		return -1;

	DIAG.stage = TPD_STAGE_READY;

	if (product_id)
		*product_id = DIAG.id;

	himax_spi_pid = DIAG.id;
	himax_spi_ready = 1;

	dprintf(CRITICAL, "tpd: spi%d fw active, handshake %d\n",
		himax_spi_bus, DIAG.handshake_ok);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Event stack                                                        */
/* ------------------------------------------------------------------ */

/*
 * FIFO-only variant of the event read: two transfers, CS held between
 * them (cs_change = 0 pauses the frame, the next call resumes it).
 *
 *   transfer 1 : { 0xF3, 0x30, 0x00 } + 29 dummy bytes -> ev[0..28]
 *   transfer 2 : 27 dummy bytes                        -> ev[29..55]
 */
static int himax_spi_read_event_paused(u8 *ev)
{
	struct spi_transfer xfer;
	u32 i;

	memset(himax_spi_tx, 0, 32);
	himax_spi_tx[0] = HIMAX_SPI_READ;
	himax_spi_tx[1] = HIMAX_REG_EVENT_STACK;
	himax_spi_tx[2] = 0x00;

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = himax_spi_tx;
	xfer.rx_buf = himax_spi_rx;
	xfer.len = 32;
	xfer.speed_hz = HIMAX_SPI_SPEED_HZ;
	xfer.cs_change = 0;	/* keep CS asserted */
	xfer.tick_delay = 0;

	if (spi_sync(himax_spi_bus, &xfer)) {
		DIAG.xfer_errors++;
		return -1;
	}

	for (i = 0; i < 29; i++)
		ev[i] = himax_spi_rx[i + 3];

	memset(himax_spi_tx, 0, 27);

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = himax_spi_tx;
	xfer.rx_buf = himax_spi_rx;
	xfer.len = 27;
	xfer.speed_hz = HIMAX_SPI_SPEED_HZ;
	xfer.cs_change = 1;	/* finish the frame, release CS */
	xfer.tick_delay = 0;

	if (spi_sync(himax_spi_bus, &xfer)) {
		DIAG.xfer_errors++;
		return -1;
	}

	for (i = 0; i < 27; i++)
		ev[29 + i] = himax_spi_rx[i];

	return 0;
}

static int himax_spi_read_event_raw(u8 *ev)
{
	u8 v;
	int ret;

	/*
	 * Vendor order (himax_read_event_stack): no AHB burst, plain window
	 * access, read, then burst back on.
	 */
	if (himax_burst_enable(0))
		return -1;

	v = 0x00;	/* AHB auto read off */
	if (himax_bus_write(HIMAX_REG_AHB_CTRL, &v, 1))
		return -1;

	if (!himax_spi_no_dma)
		ret = himax_bus_read(HIMAX_REG_EVENT_STACK, ev,
				     HIMAX_EVENT_LEN);
	else
		ret = himax_spi_read_event_paused(ev);

	v = 0x01;	/* AHB auto read on */
	if (himax_bus_write(HIMAX_REG_AHB_CTRL, &v, 1))
		return -1;

	return ret;
}

static int himax_spi_checksum_ok(const u8 *ev)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < HIMAX_EVENT_LEN; i++)
		sum += ev[i];

	return (sum & 0x00ff) == 0;
}

/* True when every byte of the frame carries the same ESD marker value. */
static int himax_spi_esd_pattern(const u8 *ev)
{
	u8 first = ev[0];
	int i;

	if (first != 0x00 && first != 0xeb && first != 0xec && first != 0xed)
		return 0;

	for (i = 1; i < HIMAX_EVENT_LEN; i++) {
		if (ev[i] != first)
			return 0;
	}

	return 1;
}

/*
 * A run of unusable frames means the current read method is broken on this
 * SPI controller (for example DMA reads returning shifted data).  Switch to
 * the other method a few times before giving up.
 */
static void himax_spi_note_bad(void)
{
	DIAG.bad_frames++;

	if (++himax_bad_run >= HIMAX_BAD_FLIP_LIMIT && DIAG.rx_flips < 4) {
		himax_bad_run = 0;
		himax_spi_no_dma = !himax_spi_no_dma;
		DIAG.rx_flips++;
	}
}

int himax_spi_tpd_read_event(int *x, int *y)
{
	u8 ev[HIMAX_EVENT_LEN];
	int num_points;
	int i;

	if (!himax_spi_ready)
		return -1;

	DIAG.int_level = himax_int_level();

	if (himax_spi_read_event_raw(ev)) {
		himax_spi_dead = 0;	/* the bus answered during bring-up */
		himax_spi_note_bad();
		return -1;
	}

	DIAG.frames++;

	if (himax_spi_esd_pattern(ev)) {
		DIAG.zero_frames++;
		himax_spi_note_bad();
		return 0;
	}

	num_points = ev[HIMAX_EVENT_POINTS];
	DIAG.last_cnt = (u8)num_points;

	if (num_points == 0xff) {
		himax_bad_run = 0;
		DIAG.idle_frames++;
		return 0;
	}

	if (!himax_spi_checksum_ok(ev)) {
		himax_spi_note_bad();
		return 0;
	}
	himax_bad_run = 0;

	num_points &= 0x0f;
	if (!num_points) {
		DIAG.idle_frames++;
		return 0;
	}

	/*
	 * Finger slots are not necessarily contiguous: take the first slot
	 * that carries valid coordinates.
	 */
	for (i = 0; i < HIMAX_MAX_POINTS; i++) {
		u16 px = ((u16)ev[i * 4] << 8) | ev[i * 4 + 1];
		u16 py = ((u16)ev[i * 4 + 2] << 8) | ev[i * 4 + 3];

		if (px == HIMAX_INVALID_COORD || py == HIMAX_INVALID_COORD)
			continue;

		*x = px;
		*y = py;
		DIAG.raw_x = px;
		DIAG.raw_y = py;
		DIAG.touch_frames++;
		return 1;
	}

	DIAG.idle_frames++;
	return 0;
}

u32 himax_spi_tpd_product_id(void)
{
	return himax_spi_pid;
}
