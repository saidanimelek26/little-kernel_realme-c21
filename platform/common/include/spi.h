#pragma once

#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_gpio.h>
#include <platform/sync_write.h>
#include <sys/types.h>
#include <debug.h>
#include <platform/mt_gpt.h>

#define MTK_SPI_ROUNDUP_DIV(a, b) (((a) + ((b)-1)) / (b))

/* #define SPI_EARLY_PORTING */
#define BUS_COUNT 8

enum {
	SPI_HZ = 109000000,
	MTK_FIFO_DEPTH = 32,
	MTK_PACKET_SIZE = 1024,
	/*
	 * Bounded so that a dead clock / gated controller fails fast instead
	 * of freezing the boot menu for ten seconds.  The longest transfer
	 * used here is 59 bytes, which takes ~32 us at 15 MHz.
	 */
	MTK_TXRX_TIMEOUT_US = 500*1000,
	MTK_ARBITRARY_VALUE = 0xdadadada
};

enum {
	MTK_SPI_IDLE = 0,
	MTK_SPI_PAUSE_IDLE = 1
};

enum {
	MTK_SPI_BUSY_STATUS = 0x01,
	MTK_SPI_PAUSE_FINISH_INT_STATUS = 3
};

/* SPI_CFG0_REG */
enum {
	SPI_CFG0_CS_HOLD_OFFSET = 0,
	SPI_CFG0_CS_SETUP_OFFSET = 16,
};

/*SPI_CFG1_REG*/
enum {
	SPI_CFG1_CS_IDLE_SHIFT = 0,
	SPI_CFG1_PACKET_LOOP_SHIFT = 8,
	SPI_CFG1_PACKET_LENGTH_SHIFT = 16,
	SPI_CFG1_GET_TICK_DLY_SHIFT = 29,

	SPI_CFG1_CS_IDLE_MASK = 0xff << SPI_CFG1_CS_IDLE_SHIFT,
	SPI_CFG1_PACKET_LOOP_MASK = 0xff << SPI_CFG1_PACKET_LOOP_SHIFT,
	SPI_CFG1_PACKET_LENGTH_MASK = 0x3ff << SPI_CFG1_PACKET_LENGTH_SHIFT,
	SPI_CFG1_GET_TICK_DLY_MASK = 0x7 << SPI_CFG1_GET_TICK_DLY_SHIFT,
};

/*SPI_CFG2_REG*/
enum {
	SPI_CFG2_SCK_HIGH_OFFSET = 0,
	SPI_CFG2_SCK_LOW_OFFSET = 16,
};

enum {
	SPI_CMD_ACT_SHIFT = 0,
	SPI_CMD_RESUME_SHIFT = 1,
	SPI_CMD_RST_SHIFT = 2,
	SPI_CMD_PAUSE_EN_SHIFT = 4,
	SPI_CMD_DEASSERT_SHIFT = 5,
	SPI_CMD_SAMPLE_SEL_SHIFT = 6,
	SPI_CMD_CS_POL_SHIFT = 7,
	SPI_CMD_CPHA_SHIFT = 8,
	SPI_CMD_CPOL_SHIFT = 9,
	SPI_CMD_RX_DMA_SHIFT = 10,
	SPI_CMD_TX_DMA_SHIFT = 11,
	SPI_CMD_TXMSBF_SHIFT = 12,
	SPI_CMD_RXMSBF_SHIFT = 13,
	SPI_CMD_RX_ENDIAN_SHIFT = 14,
	SPI_CMD_TX_ENDIAN_SHIFT = 15,
	SPI_CMD_FINISH_IE_SHIFT = 16,
	SPI_CMD_PAUSE_IE_SHIFT = 17,

	SPI_CMD_ACT_EN = 1 << SPI_CMD_ACT_SHIFT,
	SPI_CMD_RESUME_EN = 1 << SPI_CMD_RESUME_SHIFT,
	SPI_CMD_RST_EN = 1 << SPI_CMD_RST_SHIFT,
	SPI_CMD_PAUSE_EN = 1 << SPI_CMD_PAUSE_EN_SHIFT,
	SPI_CMD_DEASSERT_EN = 1 << SPI_CMD_DEASSERT_SHIFT,
	SPI_CMD_CPHA_EN = 1 << SPI_CMD_CPHA_SHIFT,
	SPI_CMD_CPOL_EN = 1 << SPI_CMD_CPOL_SHIFT,
	SPI_CMD_RX_DMA_EN = 1 << SPI_CMD_RX_DMA_SHIFT,
	SPI_CMD_TX_DMA_EN = 1 << SPI_CMD_TX_DMA_SHIFT,
	SPI_CMD_TXMSBF_EN = 1 << SPI_CMD_TXMSBF_SHIFT,
	SPI_CMD_RXMSBF_EN = 1 << SPI_CMD_RXMSBF_SHIFT,
	SPI_CMD_RX_ENDIAN_EN = 1 << SPI_CMD_RX_ENDIAN_SHIFT,
	SPI_CMD_TX_ENDIAN_EN = 1 << SPI_CMD_TX_ENDIAN_SHIFT,
	SPI_CMD_FINISH_IE_EN = 1 << SPI_CMD_FINISH_IE_SHIFT,
	SPI_CMD_PAUSE_IE_EN = 1 << SPI_CMD_PAUSE_IE_SHIFT,
};

/* SPI peripheral register map. */
struct mtk_spi_regs {
	unsigned int spi_cfg0_reg;
	unsigned int spi_cfg1_reg;
	unsigned int spi_tx_src_reg;
	unsigned int spi_rx_dst_reg;
	unsigned int spi_tx_data_reg;
	unsigned int spi_rx_data_reg;
	unsigned int spi_cmd_reg;
	unsigned int spi_status0_reg;
	unsigned int spi_status1_reg;
	unsigned int spi_pad_sel_reg;
	unsigned int spi_cfg2_reg;
	unsigned int spi_tx_src_reg_64;
	unsigned int spi_rx_dst_reg_64;
};

struct mtk_spi_bus {
	struct mtk_spi_regs *reg_addr;
	uint32_t state;
	uint32_t spi_bus_fdt_offset;
	uint32_t pad_select;
};

struct mtk_spi_bus_config {
#define    SPI_CPHA	0x01
#define    SPI_CPOL	0x02
	u32 spi_mode;
	u32 tick_delay;
	/*
	 * Bit order inside every byte.
	 *
	 * The MTK controller can shift each byte out / in LSB first (the
	 * reset default used by the LK driver) or MSB first, which is what
	 * every standard SPI slave -- the Himax touch controller included --
	 * expects.  Leaving this at 0 while talking to a normal SPI device
	 * silently bit-reverses every command byte, so the slave never
	 * decodes a valid opcode and never drives MISO.
	 *
	 * 0 = LSB first (legacy LK behaviour), 1 = MSB first.
	 */
	u32 msb_first;
};

struct spi_transfer {
	void		*tx_buf;
	void		*rx_buf;
	u32		len;
	u32		speed_hz;
	u32		cs_change;
	u32		tick_delay;
};

void mtk_spi_init(int bus_num, struct mtk_spi_bus_config *spi_config);
int init_spi_bus_from_dt(const char *compatible,
				struct mtk_spi_bus_config *config);

/*
 * Register a controller by its register base instead of by DT node.
 *
 * Some LK device trees (the Realme C21 one, for instance) carry no child
 * node for the touch controller, so there is nothing to look up even
 * though the controller itself is described.  This variant takes the base
 * address and the pad-select value straight from target knowledge.
 *
 * Returns the bus number (>= 0) or -1 on failure.
 */
int init_spi_bus_by_reg(u32 reg_base, u32 pad_select,
				struct mtk_spi_bus_config *config);

int spi_sync(int bus_num, struct spi_transfer *transfer);
