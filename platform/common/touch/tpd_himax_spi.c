/*
 * Himax HX83102D touch controller, SPI transport (Realme C21 / RMX3201).
 *
 * Everything below is taken from the official Realme C21 kernel source
 * (realme_C21-AndroidR-kernel-source):
 *
 *   arch/arm64/boot/dts/mediatek/oppo6765_20701.dts
 *     touch is a child of &spi2 (0x11012000), chip select 0
 *     spi-max-frequency = <15000000>
 *     reset-gpio = <&pio 178 0>
 *     irq-gpio   = <&pio 0 0x2002>        (not used: the event stack is
 *                                          polled, so no EINT setup)
 *     pins: GPIO17 = SPI2_MO, GPIO18 = SPI2_CLK,
 *           GPIO19 = SPI2_MI, GPIO20 = SPI2_CSB  (all function 2)
 *
 *   drivers/input/touchscreen/mediatek/hxchipset_hx83102p/himax_platform.c
 *     SPI mode 3 (CPHA | CPOL), 8 bits per word
 *     tx_mlsb = rx_mlsb = 1, cs_pol = 0  ->  MSB first, CS active low
 *     write : one transfer, { 0xF2, reg, data... }
 *     read  : { 0xF3, reg, 0x00 } followed by N bytes; the payload are the
 *             N bytes clocked in after the three byte header
 *
 *   .../himax_ic_HX83102.c (hx83102_register_read)
 *     burst_enable(len > 4)
 *     write 0x00 <- LE32(address)
 *     write 0x0c <- 0x00
 *     read  0x08 -> payload
 *     burst_enable(0) if len > 4
 *     burst_enable(x): write 0x13 <- 0x31, write 0x0d <- (0x10 | x)
 *
 *   .../himax_ic_incell_core.c (generic read_event_stack)
 *     write 0x11 <- 0x00        (AHB auto read off)
 *     read  0x30 -> 56 byte event frame
 *
 * Event frame layout (himax_parse_report_points):
 *     byte 52      : number of points (0xff = none, else value & 0x0f)
 *     byte 53..54  : state info
 *     point i      : byte i*4 .. i*4+3, x = hi<<8 | lo, y = hi<<8 | lo
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
#define HIMAX_SPI_SPEED_HZ	15000000
#define HIMAX_SPI_MODE		(SPI_CPHA | SPI_CPOL)	/* SPI mode 3 */

#define HIMAX_SPI_RST_GPIO	GPIO178

/* After a reset pulse the firmware needs to boot before it answers. */
#define HIMAX_SPI_ID_TRIES	20
#define HIMAX_SPI_ID_RETRY_MS	25		/* ~500 ms total */

/* Reset timing, from hx83102_pin_reset(). */
#define HIMAX_SPI_RST_LOW_MS	5
#define HIMAX_SPI_RST_HIGH_MS	50

/* ------------------------------------------------------------------ */
/* Controller registers                                               */
/* ------------------------------------------------------------------ */

#define HIMAX_REG_AHB_ADDR	0x00
#define HIMAX_REG_AHB_RDATA	0x08
#define HIMAX_REG_AHB_DIR	0x0c
#define HIMAX_REG_AHB_CTRL	0x11	/* 0 = plain register access */
#define HIMAX_REG_BURST_CTRL	0x13
#define HIMAX_REG_BURST_MODE	0x0d
#define HIMAX_REG_EVENT_STACK	0x30

#define HIMAX_REG_ICID		0x900000d0
/*
 * Firmware handshake, see himax_hx83102d_reload_to_active(): write 0xec to
 * 0x90000048, wait a moment and read the register back.  A running firmware
 * answers 0x01ec, the boot ROM leaves something else (0x0000) there.
 */
#define HIMAX_REG_FW_RELOAD	0x90000048
#define HIMAX_FW_RELOAD_CMD	0xec
#define HIMAX_FW_STATE0		0xec
#define HIMAX_FW_STATE1		0x01

/*
 * State register of the boot ROM, see himax_mcu_sys_reset(): right after a
 * reset it reads 0x00 0x02, which means "no firmware, waiting for one".
 */
#define HIMAX_REG_ROM_STATE	0x900000e4
#define HIMAX_ROM_STATE0	0x00
#define HIMAX_ROM_STATE1	0x02

/* Firmware restart used by himax_sense_on_by_sys_rst(). */
#define HIMAX_REG_SENSE_MAGIC	0x9000005c
#define HIMAX_REG_SYS_RESET	0x90000018
#define HIMAX_SYS_RESET_VAL	0x55
#define HIMAX_SPI_SENSE_TRIES	50
#define HIMAX_SPI_SENSE_MS	5

/* SPI wire protocol opcodes. */
#define HIMAX_SPI_WRITE		0xf2
#define HIMAX_SPI_READ		0xf3

/* Event frame. */
#define HIMAX_EVENT_LEN		56
#define HIMAX_EVENT_POINTS	52
#define HIMAX_MAX_POINTS	10
#define HIMAX_INVALID_COORD	0xffff

#define HIMAX_PRODUCT_ID_FAMILY	0x83

/*
 * Transfer buffers.
 *
 * The SPI core switches to DMA above 32 bytes and cleans/invalidates the
 * cache around the buffers, so they are static, zeroed and cache line
 * aligned: no other data can share a line with them, and the rounded
 * length used by the DMA engine stays inside the buffer.
 */
#define HIMAX_SPI_BUF_SIZE	128

static u8 himax_spi_tx[HIMAX_SPI_BUF_SIZE] __attribute__((aligned(64)));
static u8 himax_spi_rx[HIMAX_SPI_BUF_SIZE] __attribute__((aligned(64)));

static int himax_spi_bus = -1;
static int himax_spi_ready;
static int himax_spi_dead;	/* a transfer failed: controller unusable */
static int himax_spi_no_dma;	/* DMA path failed, stick to FIFO transfers */
static u32 himax_spi_pid;
static int himax_spi_bad_frames;
static int himax_spi_frame_logged;

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
	if (ret)
		himax_spi_dead = 1;

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
	 * header, so the received stream is already aligned: rx[3] is the
	 * first payload byte because rx[0..2] carry the header echo.
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

/* AHB window read, exactly as hx83102_register_read() does it. */
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

/*
 * Burst write: one 8 byte transfer, { LE32(address), LE32(value) }.
 * This is himax_flash_write_burst() from the vendor driver, used to poke
 * the AHB registers that are not reachable through the plain 0xf2 path.
 */
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

/*
 * True when the Himax firmware is running and past its reload.
 *
 * himax_hx83102d_reload_to_active() pokes 0x90000048 with 0xec and reads the
 * register back: a live firmware answers 0x01ec, the boot ROM does not.
 * This has to be a write/read pair, a plain read of 0x900000e4 only reports
 * the ROM state and says nothing about the firmware.
 */
static int himax_fw_alive(void)
{
	u8 d[4];

	if (himax_flash_write_burst(HIMAX_REG_FW_RELOAD, HIMAX_FW_RELOAD_CMD))
		return 0;

	mdelay(1);

	if (himax_ahb_read(HIMAX_REG_FW_RELOAD, d, sizeof(d)))
		return 0;

	return d[0] == HIMAX_FW_STATE0 && d[1] == HIMAX_FW_STATE1;
}

/* True when the boot ROM is waiting for a firmware image. */
static int himax_rom_waiting(void)
{
	u8 d[4];

	if (himax_ahb_read(HIMAX_REG_ROM_STATE, d, sizeof(d)))
		return 0;

	return d[0] == HIMAX_ROM_STATE0 && d[1] == HIMAX_ROM_STATE1;
}

/* ------------------------------------------------------------------ */
/* Reset / pin mux                                                    */
/* ------------------------------------------------------------------ */

static void himax_spi_pads_init(void)
{
	/* ctp_ts_spi_mode: all four signals on function 2 (SPI2). */
	mt_set_gpio_mode(GPIO17, GPIO_MODE_02);	/* SPI2_MO  */
	mt_set_gpio_mode(GPIO18, GPIO_MODE_02);	/* SPI2_CLK */
	mt_set_gpio_mode(GPIO19, GPIO_MODE_02);	/* SPI2_MI  */
	mt_set_gpio_mode(GPIO20, GPIO_MODE_02);	/* SPI2_CSB */
}

static void himax_spi_reset_pulse(void)
{
	mt_set_gpio_mode(HIMAX_SPI_RST_GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(HIMAX_SPI_RST_GPIO, GPIO_DIR_OUT);

	mt_set_gpio_out(HIMAX_SPI_RST_GPIO, GPIO_OUT_ZERO);
	mdelay(HIMAX_SPI_RST_LOW_MS);
	mt_set_gpio_out(HIMAX_SPI_RST_GPIO, GPIO_OUT_ONE);
	mdelay(HIMAX_SPI_RST_HIGH_MS);
}

/* ------------------------------------------------------------------ */
/* Zero flash firmware download                                       */
/* ------------------------------------------------------------------ */

/*
 * The HX83102D is a "zero flash" part: it has no serial flash of its own,
 * the kernel pushes the firmware into its SRAM on every cold boot
 * (hx83102d_nf_zf_part_info()).  LK runs before that, so on a cold boot the
 * chip sits in its boot ROM and the event stack never fills: the touch
 * controller is detected, but it can not report a single point.
 *
 * The image below is /vendor/firmware/tp/20701/FW_HX83102D_NF_90HZ_TRULY_FAE.img
 * from the stock RMX3201 firmware, embedded at build time by
 * platform/common/touch/hx83102d_fw.S.
 *
 * Layout, as parsed by hx_parse_bin_cfg_data():
 *   0x00000 .. 0x0ffff   firmware code         (part 0: 40928 bytes)
 *   0x10000              part table            (part count at +12)
 *   0x10000 + 16 * i     part i                (sram, size, image offset)
 *   ...
 *
 * Every section is written and then checked by the CRC engine, exactly like
 * himax_sram_write_crc_check(): up to ten attempts.
 */

#define HX_ZF_PART_TABLE	0x10000
#define HX_ZF_MAX_PARTS		16
/*
 * Payload per burst write.  The SPI FIFO is 32 bytes deep and a framed
 * write costs two header bytes plus the four address bytes, so 24 bytes of
 * payload is the largest whole number of words that still fits.  Going
 * through the DMA engine would be faster, but its transfer length has to be
 * a multiple of four and a 0xf2 framed write never is.
 */
#define HX_ZF_CHUNK		24
#define HX_ZF_CFG_MAX		16384	/* config window is 9728 here */
#define HX_ZF_TRIES		10

/* CRC engine, himax_hw_check_CRC(). */
#define HX_ZF_CRC_FROM		0x80050020
#define HX_ZF_CRC_LEN		0x80050028
#define HX_ZF_CRC_CTRL		0x80050000
#define HX_ZF_CRC_VALUE		0x80050018
#define HX_ZF_CRC_MAGIC		0x00990000
#define HX_ZF_CRC_TRIES		100

/* Overlay reply handshake, hx83102d_nf_zf_part_info(). */
#define HX_ZF_OVL_REPLY_REG	0x10007ffc
#define HX_ZF_OVL_BORDER	0x66

/* Mode switch word cleared by himax_mcu_clean_sram_0f(). */
#define HX_ZF_MODE_SWITCH	0x10007294

/* Firmware blob, see hx83102d_fw.S. */
extern const u8 hx83102d_fw_img[];
extern const u8 hx83102d_fw_img_end[];

struct himax_zf_part {
	u32 sram;
	u32 size;
	u32 off;
	int ovl;		/* overlay index, -1 when not an overlay */
};

/* Assembled config window, written through DMA: keep it on its own lines. */
static u8 himax_zf_cfg[HX_ZF_CFG_MAX] __attribute__((aligned(64)));

/*
 * himax_mcu_sys_reset(): after this the boot ROM waits for an image and
 * reports 0x00 0x02 in 0x900000e4.  The vendor retries the pulse a few
 * times, so do the same; a running firmware simply never shows that state
 * and the reset still happened.
 */
static int himax_zf_enter_rom(void)
{
	u8 v;
	u8 d[4];
	int i;

	for (i = 0; i < 6; i++) {
		v = 0x27;
		if (himax_bus_write(0x31, &v, 1))
			return -1;

		v = 0x95;
		if (himax_bus_write(0x32, &v, 1))
			return -1;

		v = 0x00;
		if (himax_bus_write(0x31, &v, 1))
			return -1;

		mdelay(1);

		if (himax_ahb_read(HIMAX_REG_ROM_STATE, d, sizeof(d)))
			return -1;

		if (d[0] == HIMAX_ROM_STATE0 && d[1] == HIMAX_ROM_STATE1)
			break;
	}

	dprintf(CRITICAL, "tpd: spi%d rom state %02x %02x\n",
		himax_spi_bus, d[0], d[1]);

	return 0;
}

/*
 * himax_mcu_write_sram_0f(): { LE32(address), payload } bursts, the address
 * advancing by the chunk size.  The chunks stay inside the SPI FIFO so the
 * chip sees one well formed command per transfer.
 */
static int himax_zf_write(u32 addr, const u8 *data, u32 len)
{
	u32 chunk;
	u32 i;

	if (himax_burst_enable(1))
		return -1;

	while (len) {
		chunk = len > HX_ZF_CHUNK ? HX_ZF_CHUNK : len;

		himax_spi_tx[0] = HIMAX_SPI_WRITE;
		himax_spi_tx[1] = HIMAX_REG_AHB_ADDR;
		for (i = 0; i < 4; i++)
			himax_spi_tx[2 + i] = (u8)(addr >> (8 * i));
		memcpy(himax_spi_tx + 6, data, chunk);

		if (himax_spi_xfer(chunk + 6))
			return -1;

		addr += chunk;
		data += chunk;
		len -= chunk;
		udelay(10);
	}

	return 0;
}

/*
 * himax_hw_check_CRC(): let the chip CRC a SRAM window.
 * 0x80050020 <- start, 0x80050028 <- 0x0099 | words, poll 0x80050000 bit 0,
 * then read the result from 0x80050018.  0xffffffff means "no answer".
 */
static u32 himax_zf_crc_hw(u32 addr, u32 len)
{
	u8 d[4];
	int i;

	if (himax_flash_write_burst(HX_ZF_CRC_FROM, addr))
		return 0xffffffffu;

	if (himax_flash_write_burst(HX_ZF_CRC_LEN,
				    HX_ZF_CRC_MAGIC | ((len / 4) & 0xffff)))
		return 0xffffffffu;

	for (i = 0; i < HX_ZF_CRC_TRIES; i++) {
		if (himax_ahb_read(HX_ZF_CRC_CTRL, d, sizeof(d)))
			return 0xffffffffu;

		if (!(d[0] & 0x01)) {
			if (himax_ahb_read(HX_ZF_CRC_VALUE, d, sizeof(d)))
				return 0xffffffffu;

			return ((u32)d[0]) | ((u32)d[1] << 8) |
			       ((u32)d[2] << 16) | ((u32)d[3] << 24);
		}

		udelay(100);
	}

	return 0xffffffffu;
}

/* himax_mcu_Calculate_CRC_with_AP(): CRC-32C over little endian words. */
static u32 himax_zf_crc_sw(const u8 *buf, u32 len)
{
	u32 crc = 0xffffffffu;
	u32 i;
	int b;

	for (i = 0; i + 4 <= len; i += 4) {
		crc ^= ((u32)buf[i]) | ((u32)buf[i + 1] << 8) |
		       ((u32)buf[i + 2] << 16) | ((u32)buf[i + 3] << 24);

		for (b = 0; b < 32; b++)
			crc = (crc & 1) ? ((crc >> 1) ^ 0x82f63b78u)
					: (crc >> 1);
	}

	return crc;
}

/* Write one section until the CRC engine accepts it. */
static int himax_zf_load(u32 addr, const u8 *data, u32 len, u32 want)
{
	u32 crc = 0;
	int i;

	for (i = 0; i < HX_ZF_TRIES; i++) {
		if (himax_zf_write(addr, data, len))
			return -1;

		crc = himax_zf_crc_hw(addr, len);
		if (crc == want)
			return 0;

		dprintf(CRITICAL,
			"tpd: spi%d crc 0x%08x != 0x%08x at 0x%08x (try %d)\n",
			himax_spi_bus, crc, want, addr, i + 1);
	}

	return -1;
}

static int himax_zf_download(void)
{
	const u8 *img = hx83102d_fw_img;
	u32 img_len = (u32)(hx83102d_fw_img_end - hx83102d_fw_img);
	struct himax_zf_part part[HX_ZF_MAX_PARTS];
	int ovl[3] = { -1, -1, -1 };
	int nparts, i, imax = 0;
	u32 cfg_base = 0xffffffffu;
	u32 cfg_max = 0;
	u32 cfg_size;
	u32 crc;
	u8 d[4];

	if (img_len < HX_ZF_PART_TABLE + 16) {
		dprintf(CRITICAL, "tpd: fw image missing\n");
		return -1;
	}

	nparts = img[HX_ZF_PART_TABLE + 12];
	if (nparts < 2 || nparts > HX_ZF_MAX_PARTS) {
		dprintf(CRITICAL, "tpd: fw image part count %d\n", nparts);
		return -1;
	}

	for (i = 0; i < nparts; i++) {
		const u8 *e = img + HX_ZF_PART_TABLE + i * 16;

		part[i].sram = (u32)e[0] | ((u32)e[1] << 8) |
			       ((u32)e[2] << 16) | ((u32)e[3] << 24);
		part[i].size = (u32)e[4] | ((u32)e[5] << 8);
		part[i].off = (u32)e[8] | ((u32)e[9] << 8) |
			      ((u32)e[10] << 16);
		part[i].ovl = -1;

		/* Overlay sections carry 0x55 0x66 and their index. */
		if (e[15] == 0x55 && e[14] == 0x66) {
			part[i].ovl = e[13];
			if (e[13] < 3)
				ovl[e[13]] = i;
		}

		if (part[i].off + part[i].size > img_len) {
			dprintf(CRITICAL,
				"tpd: fw part %d runs past the image\n", i);
			return -1;
		}
	}

	/* The config sections are written as one contiguous window. */
	for (i = 1; i < nparts; i++) {
		if (part[i].ovl >= 0)
			continue;

		if (part[i].sram < cfg_base)
			cfg_base = part[i].sram;

		if (part[i].sram > cfg_max) {
			cfg_max = part[i].sram;
			imax = i;
		}
	}

	if (cfg_base == 0xffffffffu) {
		dprintf(CRITICAL, "tpd: fw image has no config sections\n");
		return -1;
	}

	cfg_size = (cfg_max - cfg_base) + part[imax].size;
	cfg_size += cfg_size % 16;

	if (cfg_size > sizeof(himax_zf_cfg)) {
		dprintf(CRITICAL, "tpd: fw config window %d too big\n",
			cfg_size);
		return -1;
	}

	memset(himax_zf_cfg, 0, cfg_size);

	for (i = 1; i < nparts; i++) {
		u32 at;

		if (part[i].ovl >= 0)
			continue;

		at = part[i].sram;
		if (at % 4)
			at -= at % 4;

		if (at < cfg_base || at - cfg_base + part[i].size > cfg_size) {
			dprintf(CRITICAL, "tpd: fw part %d outside window\n", i);
			return -1;
		}

		memcpy(himax_zf_cfg + (at - cfg_base),
		       img + part[i].off, part[i].size);
	}

	dprintf(CRITICAL,
		"tpd: spi%d fw image %d parts, code %d@0x%08x, config %d@0x%08x, overlay %d\n",
		himax_spi_bus, nparts,
		part[0].size, part[0].sram,
		cfg_size, cfg_base,
		ovl[2] >= 0 ? (int)part[ovl[2]].size : 0);

	if (himax_zf_enter_rom())
		return -1;

	/* Part 0: the firmware itself, its region CRC has to be zero. */
	if (himax_zf_load(part[0].sram, img + part[0].off, part[0].size, 0)) {
		dprintf(CRITICAL, "tpd: spi%d firmware section failed\n",
			himax_spi_bus);
		return -1;
	}

	/* Config window, checked against the software CRC of the image. */
	crc = himax_zf_crc_sw(himax_zf_cfg, cfg_size);
	if (himax_zf_load(cfg_base, himax_zf_cfg, cfg_size, crc)) {
		dprintf(CRITICAL, "tpd: spi%d config section failed\n",
			himax_spi_bus);
		return -1;
	}

	/* Border overlay, then the reply handshake it expects. */
	if (ovl[2] >= 0 && part[ovl[2]].size) {
		int o = ovl[2];

		if (himax_zf_load(part[o].sram, img + part[o].off,
				  part[o].size, 0)) {
			dprintf(CRITICAL, "tpd: spi%d overlay failed\n",
				himax_spi_bus);
			return -1;
		}

		if (himax_burst_enable(1))
			return -1;

		if (himax_flash_write_burst(HX_ZF_OVL_REPLY_REG,
					    HX_ZF_OVL_BORDER))
			return -1;

		for (o = 0; o < HX_ZF_TRIES; o++) {
			if (himax_ahb_read(HX_ZF_OVL_REPLY_REG, d,
					   sizeof(d)))
				return -1;

			if (d[0] == HX_ZF_OVL_BORDER)
				break;

			mdelay(1);
		}

		if (o == HX_ZF_TRIES) {
			dprintf(CRITICAL, "tpd: spi%d overlay reply %02x\n",
				himax_spi_bus, d[0]);
			return -1;
		}
	}

	/* himax_mcu_clean_sram_0f(zf_data_mode_switch, 4, 0). */
	memset(d, 0, sizeof(d));
	if (himax_zf_write(HX_ZF_MODE_SWITCH, d, sizeof(d)))
		return -1;

	dprintf(CRITICAL, "tpd: spi%d fw downloaded\n", himax_spi_bus);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Probe                                                              */
/* ------------------------------------------------------------------ */

/*
 * Read the driver IC ID from AHB 0x900000d0.
 *
 * The four bytes come back as { x, 0x2X, 0x10, 0x83 }; after dropping the
 * low byte the value reads 0x0083102X, exactly what the vendor
 * hx83102d_ic_id_read() logs as "83,10,2X".
 *
 * Return:
 *   0  a plausible Himax ID (0x83102a/2b/2d/2e)
 *   1  the chip answered with something else (0xffffff: MISO idle high,
 *      0x000000: bus stuck low)
 *  -1  the transfer itself failed
 */
static int himax_spi_read_id(u32 *id, u8 raw[4])
{
	u8 d[4];
	int i;

	if (himax_ahb_read(HIMAX_REG_ICID, d, sizeof(d)))
		return -1;

	for (i = 0; i < 4; i++)
		raw[i] = d[i];

	*id = ((u32)d[0]) | ((u32)d[1] << 8) |
	      ((u32)d[2] << 16) | ((u32)d[3] << 24);
	*id >>= 8;

	if (((*id >> 16) & 0xff) != HIMAX_PRODUCT_ID_FAMILY)
		return 1;
	if (((*id >> 8) & 0xff) != 0x10)
		return 1;

	return 0;
}

/*
 * Register echo test, straight out of himax_interface_on(): both 0x13 and
 * 0x0d are readable and echo what was written.  This proves that the chip
 * decoded our command bytes ("0xf3" must arrive as 0xf3, not as the bit
 * reversed 0xcf) and that MISO is really driven, so it works even on a
 * controller whose IC ID register is not implemented.
 */
static int himax_spi_reg_echo(void)
{
	u8 v;
	u8 r13 = 0;
	u8 r0d = 0;

	v = 0x31;
	if (himax_bus_write(HIMAX_REG_BURST_CTRL, &v, 1))
		return 0;

	v = 0x10;
	if (himax_bus_write(HIMAX_REG_BURST_MODE, &v, 1))
		return 0;

	if (himax_bus_read(HIMAX_REG_BURST_CTRL, &r13, 1))
		return 0;

	if (himax_bus_read(HIMAX_REG_BURST_MODE, &r0d, 1))
		return 0;

	return r13 == 0x31 && r0d == 0x10;
}

/*
 * One detection round.  Returns 0 when the controller answered (valid IC
 * ID, or a live register window), 1 when nothing answered and -1 on a bus
 * failure.
 */
static int himax_spi_detect(u32 *id, u8 raw[4], const char *stage)
{
	int ret;

	ret = himax_spi_read_id(id, raw);
	if (ret == 0) {
		dprintf(CRITICAL,
			"tpd: spi%d %s id 0x%06x (%02x %02x %02x %02x)\n",
			himax_spi_bus, stage, *id,
			raw[0], raw[1], raw[2], raw[3]);
		return 0;
	}

	if (ret < 0)
		return -1;

	/*
	 * Not every Himax part implements 0x900000d0 the same way; a plain
	 * register echo still tells us that the SPI link itself is healthy.
	 */
	if (himax_spi_reg_echo()) {
		dprintf(CRITICAL,
			"tpd: spi%d %s alive (reg echo), id %02x %02x %02x %02x\n",
			himax_spi_bus, stage,
			raw[0], raw[1], raw[2], raw[3]);
		return 0;
	}

	if (himax_spi_dead)
		return -1;

	/* Firmware already running: it answers the reload handshake. */
	if (himax_fw_alive()) {
		dprintf(CRITICAL, "tpd: spi%d %s fw alive\n",
			himax_spi_bus, stage);
		return 0;
	}

	if (himax_spi_dead)
		return -1;

	return 1;
}

/*
 * Poll the reload handshake until the firmware answers.
 */
static int himax_spi_wait_alive(void)
{
	int i;

	for (i = 0; i < HIMAX_SPI_SENSE_TRIES; i++) {
		if (himax_fw_alive())
			return 0;
		mdelay(HIMAX_SPI_SENSE_MS);
	}

	return -1;
}

/*
 * Restart the firmware so that it starts scanning, following
 * himax_sense_on_by_sys_rst() / himax_hx83102d_reload_to_active():
 *
 *   0x9000005c <- 0
 *   0x90000018 <- 0x55      (system reset)
 *   write 0x90000048 <- 0xec and read it back until it answers 0x01ec
 *
 * If the system reset leaves it silent, retry the way himax_sense_on(0)
 * does, through the 0x31/0x32 ROM reset.
 */
static int himax_spi_sense_on(void)
{
	/* himax_interface_on(): burst mode on, address auto increment off. */
	if (himax_burst_enable(0))
		return -1;

	if (himax_flash_write_burst(HIMAX_REG_SENSE_MAGIC, 0))
		return -1;

	if (himax_flash_write_burst(HIMAX_REG_SYS_RESET, HIMAX_SYS_RESET_VAL))
		return -1;

	if (himax_spi_wait_alive() == 0)
		return 0;

	if (himax_zf_enter_rom())
		return -1;

	return himax_spi_wait_alive();
}

int himax_spi_tpd_probe(u32 *product_id)
{
	struct mtk_spi_bus_config cfg;
	u8 raw[4] = { 0 };
	u32 id = 0;
	int ret;
	int i;

	himax_spi_ready = 0;
	himax_spi_bus = -1;
	himax_spi_dead = 0;

	himax_spi_pads_init();

	memset(&cfg, 0, sizeof(cfg));
	cfg.spi_mode = HIMAX_SPI_MODE;
	cfg.tick_delay = 0;
	/*
	 * The vendor SPI core is configured with tx_mlsb = rx_mlsb = 1,
	 * i.e. MSB first.  The LK controller defaults to LSB first, which
	 * bit-reverses every byte on the wire (0xf3 goes out as 0xcf) and
	 * leaves the chip unable to decode any command, so MISO just idles
	 * high and every read returns 0xffffff.
	 */
	cfg.msb_first = 1;

	himax_spi_bus = init_spi_bus_by_reg(HIMAX_SPI_REG_BASE,
					    HIMAX_SPI_PAD_SELECT, &cfg);
	if (himax_spi_bus < 0) {
		dprintf(CRITICAL, "tpd: spi2 registration failed\n");
		return -1;
	}

	/*
	 * The panel driver has already powered the TDDI up, so the
	 * controller normally answers without being reset.  Only pulse the
	 * reset line when it does not: that line is dedicated to the touch
	 * side (the LCM reset is GPIO45), but it still restarts the chip.
	 */
	ret = himax_spi_detect(&id, raw, "cs0");

	if (ret < 0) {
		dprintf(CRITICAL,
			"tpd: spi%d transfer failed, controller not clocked?\n",
			himax_spi_bus);
		return -1;
	}

	if (ret > 0) {
		dprintf(CRITICAL,
			"tpd: spi%d cs0 silent (id %02x %02x %02x %02x), reset pulse\n",
			himax_spi_bus, raw[0], raw[1], raw[2], raw[3]);

		himax_spi_reset_pulse();
		himax_spi_dead = 0;

		for (i = 0; i < HIMAX_SPI_ID_TRIES; i++) {
			ret = himax_spi_detect(&id, raw, "cs0 after reset");
			if (ret <= 0)
				break;
			if (himax_spi_dead)
				break;
			mdelay(HIMAX_SPI_ID_RETRY_MS);
		}

		if (ret > 0) {
			dprintf(CRITICAL,
				"tpd: spi%d cs0 never answered (last id %02x %02x %02x %02x)\n",
				himax_spi_bus, raw[0], raw[1], raw[2], raw[3]);
			return -1;
		}

		if (ret < 0) {
			dprintf(CRITICAL,
				"tpd: spi%d transfer failed after reset\n",
				himax_spi_bus);
			return -1;
		}
	}

	/*
	 * The event stack is only filled while the firmware runs in active
	 * mode, so enter that mode the same way the kernel does
	 * (himax_sense_on_by_sys_rst(): 0x9000005c <- 0, 0x90000018 <- 0x55,
	 * then the 0x90000048 reload handshake has to answer 0x01ec).
	 *
	 * This is a zero-flash part: the firmware lives in SRAM only and the
	 * kernel reloads it on every cold boot, after LK has run.  The ROM
	 * reports 0x00 0x02 in 0x900000e4 while it waits for an image; that
	 * is the cold boot case, and the embedded image is pushed into SRAM
	 * right away instead of waiting for a firmware that is not there.
	 */
	if (himax_rom_waiting() || himax_spi_sense_on()) {
		dprintf(CRITICAL,
			"tpd: spi%d fw not running, downloading image\n",
			himax_spi_bus);

		if (himax_zf_download()) {
			dprintf(CRITICAL,
				"tpd: spi%d firmware download failed\n",
				himax_spi_bus);
			return -1;
		}

		/* Boot what was just written and wait for the handshake. */
		if (himax_spi_sense_on()) {
			dprintf(CRITICAL,
				"tpd: spi%d fw still not running after download\n",
				himax_spi_bus);
			return -1;
		}
	}

	dprintf(CRITICAL, "tpd: spi%d fw active\n", himax_spi_bus);

	if (product_id)
		*product_id = id;

	himax_spi_pid = id;
	himax_spi_bad_frames = 0;
	himax_spi_frame_logged = 0;
	himax_spi_no_dma = 0;
	himax_spi_ready = 1;

	return 0;
}

/* ------------------------------------------------------------------ */
/* Event stack                                                        */
/* ------------------------------------------------------------------ */

/*
 * FIFO-only variant of the event read: two transfers, CS held between
 * them (cs_change = 0 makes the driver set PAUSE, the next call resumes).
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

	if (spi_sync(himax_spi_bus, &xfer))
		return -1;

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

	if (spi_sync(himax_spi_bus, &xfer))
		return -1;

	for (i = 0; i < 27; i++)
		ev[29 + i] = himax_spi_rx[i];

	return 0;
}

static int himax_spi_read_event_raw(u8 *ev)
{
	u8 v;
	int ret;

	/*
	 * Vendor order (himax_read_event_stack + himax_burst_enable(0)):
	 * no AHB burst, plain window access, then burst back on so that
	 * the next register read still works.
	 */
	if (himax_burst_enable(0))
		return -1;

	v = 0x00;	/* AHB auto read off */
	if (himax_bus_write(HIMAX_REG_AHB_CTRL, &v, 1))
		return -1;

	ret = -1;
	if (!himax_spi_no_dma) {
		ret = himax_bus_read(HIMAX_REG_EVENT_STACK, ev,
				     HIMAX_EVENT_LEN);
		if (ret) {
			himax_spi_no_dma = 1;
			himax_spi_dead = 0;	/* the bus itself answered before */
			dprintf(CRITICAL,
				"tpd: spi DMA event read failed, using FIFO frames\n");
		}
	}

	if (ret) {
		/*
		 * The 59 byte frame is the only transfer that needs DMA
		 * (the LK FIFO is 32 bytes deep).  Read the very same frame
		 * with two FIFO transfers that keep CS asserted in
		 * between, so the controller still sees one command
		 * followed by 56 clocked-in bytes.
		 */
		ret = himax_spi_read_event_paused(ev);
	}

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

int himax_spi_tpd_read_event(int *x, int *y)
{
	u8 ev[HIMAX_EVENT_LEN];
	int num_points;
	int i;

	if (!himax_spi_ready)
		return -1;

	if (himax_spi_read_event_raw(ev)) {
		himax_spi_bad_frames++;
		return -1;
	}

	num_points = ev[HIMAX_EVENT_POINTS];
	if (num_points == 0xff) {
		himax_spi_bad_frames = 0;
		return 0;
	}

	if (!himax_spi_checksum_ok(ev)) {
		if (himax_spi_bad_frames++ < 3)
			dprintf(CRITICAL,
				"tpd: bad frame checksum (cnt %d) %02x %02x %02x %02x\n",
				num_points, ev[0], ev[1], ev[2], ev[3]);
		return 0;
	}
	himax_spi_bad_frames = 0;

	if (!himax_spi_frame_logged) {
		himax_spi_frame_logged = 1;
		dprintf(CRITICAL,
			"tpd: first frame cnt %d x %d y %d (raw %02x %02x %02x %02x)\n",
			num_points & 0x0f,
			((int)ev[0] << 8) | ev[1],
			((int)ev[2] << 8) | ev[3],
			ev[0], ev[1], ev[2], ev[3]);
	}

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

u32 himax_spi_tpd_product_id(void)
{
	return himax_spi_pid;
}
