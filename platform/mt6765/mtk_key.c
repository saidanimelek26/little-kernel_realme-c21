#include <printf.h>
#include <platform/mt_typedefs.h>
#include <platform/mtk_key.h>
#include <platform/boot_mode.h>
#include <platform/mt_pmic.h>
#include <platform/mt_gpio.h>
#include <platform/mt_pmic_wrap_init.h>
#include <platform/sync_write.h>
#include <platform/upmu_common.h>
#include <target/cust_key.h>
#include <libfdt.h>
#include <lk_builtin_dtb.h>

extern int pmic_detect_powerkey(void);
extern int pmic_detect_homekey(void);
extern kal_uint16 pmic_get_register_value(PMU_FLAGS_LIST_ENUM flagname);
extern kal_uint16 pmic_set_register_value(PMU_FLAGS_LIST_ENUM flagname,kal_uint32 val);

struct keypad_dts_data kpd_dts_data;
static int kpd_dts_init_done = 0;

static unsigned int kpd_fdt_getprop_u32(const void *fdt, int nodeoffset, const char *name)
{
	const void *data = NULL;
	int len = 0;

	data = fdt_getprop(fdt, nodeoffset, name, &len);
	if (len > 0 && data)
		return fdt32_to_cpu(*(unsigned int *)data);
	else
		return 0;
}

static void kpd_fdt_getprop_u32_array(const void *fdt, int nodeoffset,
						const char *name, char *out_value, int maxlen)
{
	const void *data = NULL;
	int len = 0;

	data = fdt_getprop(fdt, nodeoffset, name, &len);
	if (len > 0 && data)
		memcpy(out_value, data, len > maxlen ? maxlen : len);
	else
		memset(out_value, 0, len > maxlen ? maxlen: len);
}

void get_kpd_dts_info(void)
{
	int offset;

	if (kpd_dts_init_done)
		return;
	memset(&kpd_dts_data, 0, sizeof(kpd_dts_data));
	offset = fdt_path_offset(get_lk_overlayed_dtb(), "/kp@10010000");
	if (offset >= 0) {
		kpd_dts_data.kpd_sw_pwrkey = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-sw-pwrkey");
		kpd_dts_data.kpd_hw_pwrkey = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-pwrkey");
		kpd_dts_data.kpd_sw_rstkey = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-sw-rstkey");
		kpd_dts_data.kpd_hw_rstkey = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-rstkey");
		kpd_dts_data.kpd_use_extend_type = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-use-extend-type");
		kpd_dts_data.kpd_hw_recovery_key = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-recovery-key");
		kpd_dts_data.kpd_hw_factory_key = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-factory-key");
		dprintf(INFO, "kpd_sw_pwrkey = %d, kpd_hw_pwrkey = %d, kpd_sw_rstkey = %d, kpd_hw_rstkey = %d\n",
			kpd_dts_data.kpd_sw_pwrkey, kpd_dts_data.kpd_hw_pwrkey, kpd_dts_data.kpd_sw_rstkey, kpd_dts_data.kpd_hw_rstkey);
		dprintf(INFO, "kpd_use_extend_type = %d, kpd_hw_recovery_key = %d, kpd_hw_factory_key = %d\n",
			kpd_dts_data.kpd_use_extend_type, kpd_dts_data.kpd_hw_recovery_key, kpd_dts_data.kpd_hw_factory_key);
		kpd_dts_data.kpd_hw_map_num = kpd_fdt_getprop_u32(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-map-num");
		kpd_fdt_getprop_u32_array(get_lk_overlayed_dtb(), offset, "mediatek,kpd-hw-init-map", (char *)kpd_dts_data.kpd_hw_init_map, KPD_NUM_KEYS * 4);

		/*
		 * C21 / RMX3201 (20701) safety defaults.
		 *
		 * The stock DTBO entry 1 supplies these values.  If a wrong or
		 * incomplete dtbo is flashed, the node still exists but every
		 * property reads back 0.  In that case:
		 *  - kpd_hw_rstkey == 0 would make mtk_detect_key(0) (the
		 *    factory-key check in boot_mode_select) call
		 *    pmic_detect_homekey(), so holding volume-up at power-on
		 *    could enter FACTORY_BOOT;
		 *  - kpd_hw_pwrkey == 0 would stop power-key detection.
		 * Fill the known-good values instead of leaving zeros.
		 * The factory key is deliberately left at 0.
		 */
		{
			int applied = 0;

			if (kpd_dts_data.kpd_hw_pwrkey == 0) {
				kpd_dts_data.kpd_hw_pwrkey = 8;
				applied = 1;
			}
			if (kpd_dts_data.kpd_hw_rstkey == 0) {
				kpd_dts_data.kpd_hw_rstkey = 17;
				applied = 1;
			}
			if (kpd_dts_data.kpd_hw_recovery_key == 0) {
				kpd_dts_data.kpd_hw_recovery_key = 1;
				applied = 1;
			}

			if (applied)
				dprintf(CRITICAL, "kpd: C21 fallback in use (pwrkey=%d rstkey=%d recovery=%d factory=%d)\n",
					kpd_dts_data.kpd_hw_pwrkey, kpd_dts_data.kpd_hw_rstkey,
					kpd_dts_data.kpd_hw_recovery_key, kpd_dts_data.kpd_hw_factory_key);
		}

		kpd_dts_init_done = 1;
	} else {
		/* keypad dts not found: use the C21 / RMX3201 stock values */
		dprintf(CRITICAL, "keypad dts not found, using default setting\n");
		kpd_dts_data.kpd_hw_pwrkey = 8;
		kpd_dts_data.kpd_hw_rstkey = 17;
		kpd_dts_data.kpd_hw_recovery_key = 1;
		kpd_dts_data.kpd_hw_factory_key = 0;
	}
#ifdef MT65XX_FACTORY_KEY
	dprintf(CRITICAL, "MT65XX_FACTORY_KEY 0x%x\n",MT65XX_FACTORY_KEY);
#endif
#ifdef MT65XX_BOOT_MENU_KEY
	dprintf(CRITICAL, "MT65XX_BOOT_MENU_KEY 0x%x\n",MT65XX_BOOT_MENU_KEY);
#endif
#ifdef MT65XX_RECOVERY_KEY
	dprintf(CRITICAL, "MT65XX_RECOVERY_KEY 0x%x\n",MT65XX_RECOVERY_KEY);
#endif
}


void set_kpd_pmic_mode(void)
{
	return;
}

void disable_PMIC_kpd_clock(void)
{
	return;
}

void enable_PMIC_kpd_clock(void)
{
	return;
}

/* 0..2 cleared (pressed) matrix lines is a plausible key press; more than
 * that means the controller is not scanning a real matrix. */
static int kpd_mems_sane(void)
{
	int i;
	int pressed = 0;

	for (i = 0; i < KPD_NUM_MEMS; i++) {
		u32 bits = ~DRV_Reg32(KP_MEM1 + (i << 2)) & 0xffff;

		while (bits) {
			pressed++;
			bits &= bits - 1;
		}
	}

	return pressed <= 2;
}

/*
 * NovaLK: keypad controller bring-up.
 *
 * LK never programs the MT6765 keypad controller itself; it relies on
 * whatever state the preloader left behind.  When the controller is not
 * enabled (KP_EN = 0) the KP_MEM registers do not track the matrix, so
 * VOLUME_DOWN (matrix key 1) never reports a press.  Enable the block
 * without changing an existing scan selection.
 *
 * This is called late (right before the boot menu is drawn), i.e. after
 * the early boot-mode key checks have already been evaluated, so it
 * cannot change the recovery/fastboot decision made earlier in boot.
 */
void mtk_keypad_init(void)
{
	u32 sel, en, deb;
	int enabled_by_us = 0;

	sel = DRV_Reg32(KP_SEL);
	en = DRV_Reg32(KP_EN);
	deb = DRV_Reg32(KP_DEBOUNCE);

	dprintf(CRITICAL, "kpd: pre-init SEL=0x%x EN=0x%x DEB=0x%x\n",
		sel, en, deb);

	if (!sel)
		DRV_WriteReg32(KP_SEL, 0x000000ff);
	if (!deb)
		DRV_WriteReg32(KP_DEBOUNCE, 0x00000010);
	if (!(en & 0x1)) {
		DRV_WriteReg32(KP_EN, en | 0x1);
		enabled_by_us = 1;
	}

	mdelay(40);

	dprintf(CRITICAL,
		"kpd: post-init SEL=0x%x EN=0x%x STA=0x%x MEM=%x/%x/%x/%x/%x\n",
		DRV_Reg32(KP_SEL), DRV_Reg32(KP_EN), DRV_Reg32(KP_STA),
		DRV_Reg32(KP_MEM1), DRV_Reg32(KP_MEM2), DRV_Reg32(KP_MEM3),
		DRV_Reg32(KP_MEM4), DRV_Reg32(KP_MEM5));

	/*
	 * Safety net: an idle matrix reads back all ones in every memory
	 * register.  If we enabled a controller that was never configured,
	 * the registers can read random values instead; that would look like
	 * keys pressed all the time and make the menu unusable.  In that case
	 * undo our enable and stay on the PMIC keys.
	 */
	if (enabled_by_us && !kpd_mems_sane()) {
		dprintf(CRITICAL, "kpd: matrix not sane, disabling controller\n");
		DRV_WriteReg32(KP_EN, DRV_Reg32(KP_EN) & ~0x1);
	}
}

/* All KP_MEMx registers read back 0 when the keypad block is not
 * running.  A real "no key pressed" matrix reads back all ones (pull-up),
 * so all-zero means "controller dead", not "everything pressed". */
static int kpd_mems_all_zero(void)
{
	int i;

	for (i = 0; i < KPD_NUM_MEMS; i++) {
		if (DRV_Reg32(KP_MEM1 + (i << 2)) != 0)
			return 0;
	}

	return 1;
}

BOOL mtk_detect_key(unsigned short key) /* key: HW keycode */
{
#ifdef MACH_FPGA
	return FALSE;
#else

	unsigned short idx, bit, din;

	get_kpd_dts_info();
	if (key >= KPD_NUM_KEYS)
		return FALSE;

	idx = key / 16;
	bit = key % 16;

	/*
	 * The keypad matrix reports a pressed key as a cleared bit
	 * (the line is pulled low), so a 0 bit means "pressed".
	 */
	din = (DRV_Reg32(KP_MEM1 + (idx << 2)) >> bit) & 0x1;

	/* A disabled controller reads back 0 for every bit; do not turn
	 * that into "every key is pressed". */
	if (!din && kpd_mems_all_zero())
		din = 1;

	if (key == MTK_PMIC_PWR_KEY) {
		/* Power key */
		if (1 == pmic_detect_powerkey()) {
			dprintf(ALWAYS, "power key is pressed\n");
			return TRUE;
		}
		return FALSE;
	}

#ifdef MTK_PMIC_RST_KEY
	if (key == MTK_PMIC_RST_KEY) {
		/*
		 * MTK routes the "reset/home" key through the PMIC, but on
		 * the C21 VOLUME_UP is also a normal keypad matrix key.
		 * Accept either source so navigation works whether the key
		 * is wired to the PMIC or to the keypad matrix.
		 */
		if (1 == pmic_detect_homekey()) {
			dprintf(ALWAYS, "mtk detect key function pmic_detect_homekey pressed\n");
			return TRUE;
		}
	}
#endif

	if (!din) {
		dprintf(ALWAYS, "key %d is pressed\n", key);
		return TRUE;
	}
	return FALSE;
#endif
}

BOOL mtk_detect_pmic_just_rst(void)
{
	kal_uint32 just_rst = 0;

	dprintf(ALWAYS, "detecting pmic just reset\n");

	just_rst = pmic_get_register_value(PMIC_JUST_PWRKEY_RST);
	if (just_rst) {
		dprintf(ALWAYS, "Just recover from a reset\n");
		pmic_set_register_value(PMIC_CLR_JUST_RST, 0x01);
		return TRUE;
	}
	return FALSE;
}
