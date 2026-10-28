#ifndef __DEV_TPD_H
#define __DEV_TPD_H

#include <platform/mt_typedefs.h>

/*
 * NovaLK touch panel framework.
 *
 * Only the Himax HX83102D controller (Realme C21 / RMX3201) is
 * supported, over SPI2.  tpd_init() probes it and never blocks the
 * boot if it is not found: touch simply stays disabled and the boot menu
 * keeps working with the hardware keys and its timeout.
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

/* Human readable controller name for logs, e.g. "HX83112A". */
const char *tpd_name(void);

#endif /* __DEV_TPD_H */
