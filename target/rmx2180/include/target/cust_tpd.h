#ifndef __CUST_TPD_H__
#define __CUST_TPD_H__

/*
 * NovaLK touch panel configuration for Realme C21 / RMX3201 (MT6765).
 *
 * The touch controller of this device is a Himax HX83112A
 * ("himax,hx83112a_nf", reported by Device Info HW from the running
 * Android kernel).  It sits at the usual Himax 7-bit I2C address 0x48.
 *
 * The I2C bus number and the reset GPIO are described by the vendor DTBO,
 * which is not part of this repository.  tpd.c therefore:
 *   1. looks for a "himax"/"hx83112" node in the overlayed DTB (built-in
 *      main DTB + flashed DTBO) and uses its reg/reset-gpio/bus, then
 *   2. if that fails, scans I2C0..I2C6 for a HX83112A product ID.
 *
 * Values below are only used for the fallback path and for coordinate
 * mapping.  Set TPD_RST_GPIO to the real reset GPIO number once known if
 * the automatic probe cannot reach the chip (the panel may be held in
 * reset by the preloader).
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
