#ifndef __CUST_KEY_H__
#define __CUST_KEY_H__

#ifndef USE_DTB_NO_DWS
#include <cust_kpd.h>
#else
extern struct keypad_dts_data kpd_dts_data;
#define MT65XX_FACTORY_KEY    (kpd_dts_data.kpd_hw_factory_key)
#define MTK_PMIC_PWR_KEY      (kpd_dts_data.kpd_hw_pwrkey)
#define MTK_PMIC_RST_KEY      (kpd_dts_data.kpd_hw_rstkey)
#endif

#define MT65XX_META_KEY		42	/* KEY_2 */
/* MTK_PMIC_RST_KEY is defined in cust_kpd.h */
#define MT_CAMERA_KEY 		9

/*
 * C21 / RMX3201 / project 20701 key layout (stock DTBO entry 1):
 *
 *   hw key  8  -> PMIC power key
 *   hw key 17  -> PMIC home/reset key, reported as KEY_VOLUMEUP
 *                 (mediatek,kpd-hw-rstkey = 17)
 *   hw key  1  -> keypad matrix bit 1, KEY_VOLUMEDOWN
 *                 (the only non-zero entry of mediatek,kpd-hw-init-map)
 *   mediatek,kpd-hw-recovery-key = 1
 *   mediatek,kpd-hw-factory-key is not present in the stock DTBO (= 0)
 *
 * So: VOLUME UP enters and navigates the boot menu, VOLUME DOWN confirms.
 */
#define MT65XX_BOOT_MENU_KEY       17     /* VOLUME UP / PMIC home key */
#define MT65XX_MENU_SELECT_KEY     MT65XX_BOOT_MENU_KEY

/*
 * Do NOT use MT65XX_FACTORY_KEY here.  It is 0 on this device (the stock
 * DTBO does not export mediatek,kpd-hw-factory-key) and it is also used by
 * boot_mode_select() to detect FACTORY_BOOT at power-on.  Keeping it at 0
 * means holding volume-down at boot can never enter factory mode.
 *
 * The menu OK key is the raw hardware keypad bit used by volume-down.
 */
#define MT65XX_MENU_OK_KEY         1      /* VOLUME DOWN (kpd matrix bit 1) */

#endif /* __CUST_KEY_H__ */
