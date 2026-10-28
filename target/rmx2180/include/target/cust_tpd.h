#ifndef __CUST_TPD_H__
#define __CUST_TPD_H__

/*
 * NovaLK touch panel configuration for Realme C21 / RMX3201 (MT6765).
 *
 * The touch controller of this device is a Himax HX83102D TDDI wired to
 * SPI2 (0x11012000, chip select 0), reset on GPIO178, IRQ on GPIO0.  This
 * was confirmed by the LK log (nothing ACKs on I2C at 0x48) and by the
 * official Realme C21 kernel DT (oppo6765_20701.dts: the tp node is a
 * child of &spi2).  The SPI transport is implemented in
 * platform/common/touch/tpd_himax_spi.c.
 *
 * The I2C probe below it is only a fallback for a differently wired
 * board; on this device it never succeeds.
 *
 * Values below are only used for that fallback and for coordinate
 * mapping.
 */

#define TPD_PANEL_WIDTH       720
#define TPD_PANEL_HEIGHT      1600

/* Fallback probe parameters (-1 = scan every bus / do not drive reset). */
#define TPD_I2C_BUS_DEFAULT   (-1)
#define TPD_I2C_ADDR_DEFAULT  0x48
#define TPD_RST_GPIO_DEFAULT  (-1)

/* Optional coordinate fixups, in case the panel reports swapped axes. */
#define TPD_SWAP_XY           0
#define TPD_INVERT_X          0
#define TPD_INVERT_Y          0

#endif /* __CUST_TPD_H__ */
