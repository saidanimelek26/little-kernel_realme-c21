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
 * Firmware handshake: after the (zero flash) firmware has been loaded and
 * started it reports 0xec 0x01 here, see himax_hx83102d_reload_to_active().
 */
#define HIMAX_REG_FW_STATE	0x900000e4
#define HIMAX_FW_STATE0		0xec
#define HIMAX_FW_STATE1		0x01

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
static u8 himax_spi_tx[64] __attribute__((aligned(64)));
static u8 himax_spi_rx[64] __attribute__((aligned(64)));

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

/* True when the Himax firmware is running and past its reload. */
static int himax_fw_alive(void)
{
	u8 d[4];

	if (himax_ahb_read(HIMAX_REG_FW_STATE, d, sizeof(d)))
		return 0;

	return d[0] == HIMAX_FW_STATE0 && d[1] == HIMAX_FW_STATE1;
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
	u8 st[4];
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
	if (himax_ahb_read(HIMAX_REG_FW_STATE, st, sizeof(st)) == 0 &&
	    st[0] == HIMAX_FW_STATE0 && st[1] == HIMAX_FW_STATE1) {
		dprintf(CRITICAL, "tpd: spi%d %s fw alive\n",
			himax_spi_bus, stage);
		return 0;
	}

	if (himax_spi_dead)
		return -1;

	return 1;
}

/*
 * Restart the firmware so that it starts scanning, following
 * himax_sense_on_by_sys_rst() / himax_hx83102d_reload_to_active():
 *
 *   0x9000005c <- 0
 *   0x90000018 <- 0x55      (system reset)
 *   poll 0x900000e4 until 0xec 0x01
 */
static int himax_spi_sense_on(void)
{
	int i;

	if (himax_flash_write_burst(HIMAX_REG_SENSE_MAGIC, 0))
		return -1;

	if (himax_flash_write_burst(HIMAX_REG_SYS_RESET, HIMAX_SYS_RESET_VAL))
		return -1;

	for (i = 0; i < HIMAX_SPI_SENSE_TRIES; i++) {
		if (himax_fw_alive())
			return 0;
		mdelay(HIMAX_SPI_SENSE_MS);
	}

	return -1;
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
	 * (himax_sense_on(0): 0x9000005c <- 0, 0x90000018 <- 0x55, then
	 * wait until 0x900000e4 reads 0xec 0x01).
	 *
	 * This is a zero-flash part: the firmware is downloaded into SRAM by
	 * the kernel on every cold boot.  When it is not there nothing can
	 * report a point, and it is better to say so than to pretend the
	 * touch controller is usable.
	 */
	if (himax_spi_sense_on()) {
		dprintf(CRITICAL,
			"tpd: spi%d fw not running (zero-flash part without kernel firmware)\n",
			himax_spi_bus);
		return -1;
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
