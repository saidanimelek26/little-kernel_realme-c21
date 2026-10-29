#include <string.h>
#include <printf.h>
#include <platform/mt_typedefs.h>
#include <platform/timer.h>
#include <libfdt.h>
#include <lk_builtin_dtb.h>
#include <dev/tpd.h>
#include <target/cust_tpd.h>
#include "tpd_himax.h"

#define TPD_MAX_BUS	6
#define TPD_I2C_ADDR	0x48

/* Filled by the transports, shown by the boot menu (see dev/tpd.h). */
struct tpd_diag g_tpd_diag;

static int tpd_ready;
static int tpd_bus = -1;
static u32 tpd_product_id;
static char tpd_label[16] = "touch";

/* Transport the controller was found on. */
#define TPD_TRANSPORT_NONE	0
#define TPD_TRANSPORT_SPI	1
#define TPD_TRANSPORT_I2C	2

static int tpd_transport = TPD_TRANSPORT_NONE;

/*
 * MT6765 I2C controller base addresses, in controller index order.
 * The vendor DT describes the touch controller as a child of one of these
 * nodes, so the parent "reg" tells us which LK bus to use.
 */
static const u32 tpd_i2c_bases[TPD_MAX_BUS + 1] = {
	0x11007000,	/* I2C0 */
	0x11008000,	/* I2C1 */
	0x11009000,	/* I2C2 */
	0x1100f000,	/* I2C3 */
	0x11011000,	/* I2C4 */
	0x11016000,	/* I2C5 */
	0x1100d000,	/* I2C6 */
};

static int tpd_bus_from_parent(void *fdt, int node)
{
	int parent;
	const void *reg;
	int len = 0;
	u32 base = 0;
	int i;

	parent = fdt_parent_offset(fdt, node);
	if (parent < 0)
		return -1;

	reg = fdt_getprop(fdt, parent, "reg", &len);
	if (reg && len >= (int)sizeof(u32))
		base = fdt32_to_cpu(*(const u32 *)reg);

	for (i = 0; i <= TPD_MAX_BUS; i++) {
		if (tpd_i2c_bases[i] == base)
			return i;
	}

	return -1;
}

static int tpd_gpio_from_prop(void *fdt, int node, const char *name)
{
	const void *data;
	int len = 0;
	const u32 *cells;

	data = fdt_getprop(fdt, node, name, &len);
	if (!data || len < 8)
		return -1;

	cells = (const u32 *)data;
	return (int)fdt32_to_cpu(cells[1]);
}

static int tpd_dt_lookup(int *bus, u8 *addr, int *rst_gpio)
{
	void *fdt = get_lk_overlayed_dtb();
	int node = -1;

	if (!fdt)
		return -1;

	while ((node = fdt_next_node(fdt, node, NULL)) >= 0) {
		const char *compat;
		const void *reg;
		int len = 0;

		compat = fdt_getprop(fdt, node, "compatible", NULL);
		if (!compat)
			continue;

		if (!strstr(compat, "himax") &&
		    !strstr(compat, "hx83112") &&
		    !strstr(compat, "hx83102"))
			continue;

		reg = fdt_getprop(fdt, node, "reg", &len);
		if (!reg || len < (int)sizeof(u32))
			continue;

		*addr = (u8)(fdt32_to_cpu(*(const u32 *)reg) & 0xff);
		*bus = tpd_bus_from_parent(fdt, node);

		*rst_gpio = tpd_gpio_from_prop(fdt, node, "himax,rst-gpio");
		if (*rst_gpio < 0)
			*rst_gpio = tpd_gpio_from_prop(fdt, node, "himax,reset-gpio");
		if (*rst_gpio < 0)
			*rst_gpio = tpd_gpio_from_prop(fdt, node, "reset-gpio");
		if (*rst_gpio < 0)
			*rst_gpio = tpd_gpio_from_prop(fdt, node, "reset-gpios");

		dprintf(CRITICAL, "tpd: DT '%s' bus %d addr 0x%x rst %d\n",
			compat, *bus, *addr, *rst_gpio);

		return 0;
	}

	return -1;
}

static void tpd_set_name(u32 product_id)
{
	if ((product_id & 0xffffff) == 0x83102d)
		strcpy(tpd_label, "HX83102D");
	else if ((product_id & 0xffff00) == 0x831100)
		strcpy(tpd_label, "HX83112A");
	else if ((product_id & 0xffff00) == 0x831000)
		strcpy(tpd_label, "HX83102");
	else if ((product_id >> 16) == 0x83)
		strcpy(tpd_label, "Himax");
	else if (product_id)
		strcpy(tpd_label, "touch");
	else
		strcpy(tpd_label, "Himax");
}

int tpd_init(void)
{
	int dt_bus = TPD_I2C_BUS_DEFAULT;
	int rst_gpio = TPD_RST_GPIO_DEFAULT;
	u8 addr = TPD_I2C_ADDR_DEFAULT;
	u32 product_id = 0;
	int bus;

	if (tpd_ready)
		return 0;

	/*
	 * 0. The C21 touch controller is wired to SPI2, not I2C.  This is
	 *    the path that works on the device: the I2C bus scan below never
	 *    succeeds because nothing ACKs at 0x48.
	 */
#ifdef MTK_SPI_SUPPORT
	if (himax_spi_tpd_probe(&product_id) == 0) {
		tpd_transport = TPD_TRANSPORT_SPI;
		tpd_bus = -1;
		tpd_product_id = product_id;
		tpd_ready = 1;
		tpd_set_name(tpd_product_id);
		dprintf(CRITICAL, "tpd: %s ready on spi2 cs0 (id 0x%x)\n",
			tpd_label, tpd_product_id);
		return 0;
	}
#endif

#ifdef MTK_SPI_SUPPORT
	/*
	 * The SPI link answered but the controller could not be brought up
	 * (firmware download failed): an I2C scan cannot help and would only
	 * overwrite the failure reason shown on screen.
	 */
	if (g_tpd_diag.link_ok) {
		dprintf(CRITICAL, "tpd: spi2 chip found but bring-up failed\n");
		return -1;
	}
#endif

	/*
	 * 1. Use the device tree when it describes the controller: that
	 *    gives us the exact bus, slave address and reset GPIO.
	 */
	if (tpd_dt_lookup(&dt_bus, &addr, &rst_gpio) == 0) {
		dprintf(CRITICAL, "tpd: DT says i2c%d addr 0x%x rst %d\n",
			dt_bus, addr, rst_gpio);

		if (dt_bus >= 0 && !himax_tpd_probe(dt_bus, addr, rst_gpio,
						    &product_id)) {
			tpd_bus = dt_bus;
			tpd_product_id = product_id;
			tpd_transport = TPD_TRANSPORT_I2C;
			tpd_ready = 1;
			goto done;
		}
	}

	/*
	 * 2. Otherwise scan every I2C controller for the standard Himax
	 *    address.  The built-in LK DTB only carries a generic
	 *    "goodix,touch" node without reg/reset GPIO, so this is the
	 *    path used on the C21.
	 */
	for (bus = 0; bus <= TPD_MAX_BUS; bus++) {
		if (!himax_tpd_probe(bus, addr, rst_gpio, &product_id)) {
			tpd_bus = bus;
			tpd_product_id = product_id;
			tpd_transport = TPD_TRANSPORT_I2C;
			tpd_ready = 1;
			goto done;
		}

		/* Give the next controller time to settle after a failed
		 * transfer / possible reset. */
		mdelay(2);
	}

	if (g_tpd_diag.err == TPD_ERR_NONE)
		g_tpd_diag.err = TPD_ERR_NOT_FOUND;
	dprintf(CRITICAL, "tpd: no touch controller found, keys only\n");
	return -1;

done:
	tpd_set_name(tpd_product_id);
	dprintf(CRITICAL, "tpd: %s ready on i2c%d addr 0x%x (id 0x%x)\n",
		tpd_label, tpd_bus, addr, tpd_product_id);
	return 0;
}

int tpd_available(void)
{
	return tpd_ready;
}

const char *tpd_name(void)
{
	return tpd_label;
}

int tpd_read(int *x, int *y)
{
	int px = 0, py = 0;
	int ret;

	if (!tpd_ready)
		return -1;

	ret = 0;
	switch (tpd_transport) {
	case TPD_TRANSPORT_SPI:
		ret = himax_spi_tpd_read_event(&px, &py);
		break;
	case TPD_TRANSPORT_I2C:
		ret = himax_tpd_read_event(&px, &py);
		break;
	default:
		return -1;
	}

	if (ret <= 0)
		return ret;

#if TPD_SWAP_XY
	{
		int tmp = px;

		px = py;
		py = tmp;
	}
#endif
#if TPD_INVERT_X
	px = TPD_PANEL_WIDTH - 1 - px;
#endif
#if TPD_INVERT_Y
	py = TPD_PANEL_HEIGHT - 1 - py;
#endif

	if (px < 0 || px >= TPD_PANEL_WIDTH || py < 0 || py >= TPD_PANEL_HEIGHT) {
		dprintf(CRITICAL, "tpd: out of range point %d,%d\n", px, py);
		return 0;
	}

	*x = px;
	*y = py;
	g_tpd_diag.x = px;
	g_tpd_diag.y = py;

	return 1;
}

/* ------------------------------------------------------------------ */
/* On-screen diagnostics                                              */
/* ------------------------------------------------------------------ */

const struct tpd_diag *tpd_get_diag(void)
{
	return &g_tpd_diag;
}

static const char *tpd_stage_name(int stage)
{
	switch (stage) {
	case TPD_STAGE_PADS:		return "pads/clock";
	case TPD_STAGE_SPI:		return "spi init";
	case TPD_STAGE_RESET:		return "reset";
	case TPD_STAGE_LINK:		return "link";
	case TPD_STAGE_PARSE:		return "fw parse";
	case TPD_STAGE_SENSE_OFF:	return "sense off";
	case TPD_STAGE_FW_MAIN:		return "fw main";
	case TPD_STAGE_FW_CFG:		return "fw config";
	case TPD_STAGE_FW_OVL:		return "fw overlay";
	case TPD_STAGE_FW_START:	return "fw start";
	case TPD_STAGE_READY:		return "ready";
	default:			return "-";
	}
}

static const char *tpd_err_name(int err)
{
	switch (err) {
	case TPD_ERR_NO_SPI:	return "no spi ctrl";
	case TPD_ERR_XFER:	return "spi xfer fail";
	case TPD_ERR_NO_CHIP:	return "chip silent";
	case TPD_ERR_FW_PARSE:	return "bad fw image";
	case TPD_ERR_SENSE_OFF:	return "sense off";
	case TPD_ERR_FW_CRC:	return "fw crc mismatch";
	case TPD_ERR_NOT_FOUND:	return "not found";
	default:		return "ok";
	}
}

int tpd_diag_line(int line, char *buf, int len)
{
	const struct tpd_diag *d = &g_tpd_diag;

	switch (line) {
	case 0:
		if (tpd_ready)
			return snprintf(buf, len, "TP READY %s", tpd_label);
		return snprintf(buf, len, "TP FAIL at %s: %s",
				tpd_stage_name(d->stage), tpd_err_name(d->err));
	case 1:
		return snprintf(buf, len,
			"id %06x link %d off %d parts %d crc %d/%d/%d dma-fb %d hs %d rl %d try %d",
			d->id, d->link_ok, d->sense_off_ok, d->nparts,
			d->crc_main_tries, d->crc_cfg_tries, d->crc_ovl_tries,
			d->dma_fallback, d->handshake_ok, d->reload_dis_ok,
			d->attempts);
	default:
		return snprintf(buf, len,
			"int %d fr %u idle %u tch %u bad %u zero %u err %u flip %d cnt %02x raw %d,%d",
			d->int_level, d->frames, d->idle_frames, d->touch_frames,
			d->bad_frames, d->zero_frames, d->xfer_errors, d->rx_flips,
			d->last_cnt, d->raw_x, d->raw_y);
	}
}
