#ifndef __TPD_HIMAX_H
#define __TPD_HIMAX_H

#include <platform/mt_typedefs.h>

/*
 * Himax HX83112A low level driver for LK.
 *
 * Protocol taken from the mainline driver
 * drivers/input/touchscreen/himax_hx83112b.c:
 *
 *   - register 0x00..0x03 : 32 bit little endian AHB address
 *   - register 0x0c       : access direction (0 = read)
 *   - register 0x08..0x0b : 32 bit AHB data (read port)
 *   - register 0x30..0x67 : event stack (56 bytes, raw read)
 *
 * The vendor Android driver reaches the same window by writing the 32 bit
 * address as a plain 4 byte transfer and reading the payload right after
 * it.  Both variants are used, because some HX83112A firmwares only
 * answer one of them.
 *
 * The LK MT6765 I2C driver is limited to the 8 byte FIFO (DMA is forced
 * off in _config_mt_i2c()), therefore the 56 byte event frame is read
 * back in 4 byte pieces, which is also the size the mainline driver uses
 * for its non burst reads.
 */

int himax_tpd_probe(int bus, u8 addr, int rst_gpio, u32 *product_id);
int himax_tpd_start(int bus, u8 addr);

/* Product id reported by the controller that was found, 0 if unknown. */
u32 himax_tpd_product_id(void);

/*
 * Read and validate one event frame.
 *  1  -> touch, coordinates in *x / *y
 *  0  -> no touch / invalid frame
 * <0  -> I2C error
 */
int himax_tpd_read_event(int *x, int *y);

#endif /* __TPD_HIMAX_H */
