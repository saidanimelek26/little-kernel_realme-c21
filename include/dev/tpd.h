#ifndef __DEV_TPD_H
#define __DEV_TPD_H

#include <platform/mt_typedefs.h>

/*
 * NovaLK touch panel framework.
 *
 * Only the Himax HX83102D controller (Realme C21 / RMX3201) is
 * supported, over SPI2.  tpd_init() probes it, downloads the zero-flash
 * firmware into its SRAM and never blocks the boot if anything fails:
 * touch simply stays disabled and the boot menu keeps working with the
 * hardware keys and its timeout.
 */

#define TPD_MAX_POINTS 10

/* 0 if a controller was found and configured, < 0 otherwise. */
int tpd_init(void);

/* 1 if a controller is ready, 0 otherwise. */
int tpd_available(void);

/*
 * Poll the controller.
 *   1  -> finger down, *x / *y hold panel coordinates
 *   0  -> no touch (or a frame that did not pass validation)
 *  <0  -> touch not available
 */
int tpd_read(int *x, int *y);

/* Human readable controller name for logs, e.g. "HX83102D". */
const char *tpd_name(void);

/* ------------------------------------------------------------------ */
/* Diagnostics (shown on screen by the boot menu, no UART needed)     */
/* ------------------------------------------------------------------ */

/* Last bring-up stage that was entered. */
enum tpd_stage {
	TPD_STAGE_NONE = 0,
	TPD_STAGE_PADS,		/* pin mux + SPI2 clock gate */
	TPD_STAGE_SPI,		/* SPI controller registration */
	TPD_STAGE_RESET,	/* hardware reset pulse */
	TPD_STAGE_LINK,		/* register echo / ID read */
	TPD_STAGE_PARSE,	/* firmware partition table */
	TPD_STAGE_SENSE_OFF,	/* MCU halt before download */
	TPD_STAGE_FW_MAIN,	/* 64K main code block */
	TPD_STAGE_FW_CFG,	/* configuration blocks */
	TPD_STAGE_FW_OVL,	/* overlay section + handshake */
	TPD_STAGE_FW_START,	/* reload disable + sense on */
	TPD_STAGE_READY		/* running, polling events */
};

/* Reason of the failure, TPD_ERR_NONE while everything is fine. */
enum tpd_err {
	TPD_ERR_NONE = 0,
	TPD_ERR_NO_SPI,		/* could not register the SPI controller */
	TPD_ERR_XFER,		/* an SPI transfer timed out / failed */
	TPD_ERR_NO_CHIP,	/* bus alive but the chip does not answer */
	TPD_ERR_FW_PARSE,	/* firmware image is malformed */
	TPD_ERR_SENSE_OFF,	/* MCU would not enter safe mode */
	TPD_ERR_FW_CRC,		/* SRAM write never matched the HW CRC */
	TPD_ERR_NOT_FOUND	/* neither SPI nor I2C controller found */
};

struct tpd_diag {
	int stage;		/* enum tpd_stage */
	int err;		/* enum tpd_err */

	/* bring-up */
	u8  id_raw[4];		/* raw bytes of AHB 0x900000d0 */
	u32 id;			/* decoded IC id, 0x83102x expected */
	int link_ok;		/* register echo test passed */
	int sense_off_ok;
	int nparts;		/* partitions found in the firmware image */
	int crc_main_tries;	/* attempts the main block needed (0 = n/a) */
	int crc_cfg_tries;
	int crc_ovl_tries;
	int dma_fallback;	/* DMA writes were replaced by FIFO chains */
	int reload_dis_ok;	/* flash reload disable read back correctly */
	int handshake_ok;	/* firmware answered 0xEC 0x01 */
	int attempts;		/* full bring-up attempts */

	/* runtime */
	int int_level;		/* IRQ line (GPIO0), 0 = controller has data */
	u32 frames;		/* event frames read */
	u32 idle_frames;	/* frames reporting "no touch" */
	u32 touch_frames;	/* frames with a valid finger */
	u32 bad_frames;		/* checksum / sanity failures */
	u32 zero_frames;	/* all-zero or ESD pattern frames */
	u32 xfer_errors;
	int rx_flips;		/* DMA <-> FIFO event read switches */
	int raw_x, raw_y;	/* last point as reported by the chip */
	int x, y;		/* last point after mapping */
	u8  last_cnt;		/* last point-count byte (0xff = none) */
};

const struct tpd_diag *tpd_get_diag(void);

/*
 * Compose a short human readable diagnostic line.
 *   line 0 : bring-up summary ("TP READY HX83102D" / "TP FAIL: ...")
 *   line 1 : bring-up details (id, CRC attempts, handshake)
 *   line 2 : live data (INT level, frame counters, last point)
 * Returns the number of characters written.
 */
int tpd_diag_line(int line, char *buf, int len);

#endif /* __DEV_TPD_H */
