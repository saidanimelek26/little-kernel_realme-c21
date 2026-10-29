# Build config for Realme C21 / RMX3201 (project 20701).
# The filename stays rmx2180.mk because the GitHub workflow uses `make rmx2180`.
#
# Stock-LK security/AVB configuration.  This replaces the previous
# "FORCE DISABLE AVB COMPLETELY" block, which removed the stock
# sec_policy / set_boot_info_to_rpmb / km_client flow and forced the
# test-only unlock path in load_vfy_boot.c.

LOCAL_DIR := $(GET_LOCAL_DIR)
TARGET := rmx2180
MODULES += app/mt_boot \
           dev/lcm

MTK_EMMC_SUPPORT = yes
MTK_MMC_COMBO_DRV = yes
MTK_EMMC_SUPPORT_OTP = yes
MTK_SMI_SUPPORT = yes
MTK_KERNEL_POWER_OFF_CHARGING = yes
DEFINES += MTK_NEW_COMBO_EMMC_SUPPORT
DEFINES += MTK_GPT_SCHEME_SUPPORT

MTK_CHARGER_NEW_ARCH := yes
MTK_PUMP_EXPRESS_PLUS_SUPPORT := yes
MTK_MT6370_PMU_CHARGER_SUPPORT := yes
MTK_MT6370_PMU_BLED_SUPPORT := yes
MTK_CHARGER_INTERFACE := yes

# LCM Configuration
MTK_LCM_PHYSICAL_ROTATION = 0
CUSTOM_LK_LCM = "hx83102d_hdp_dsi_vdo_truly_truly_zal3251"
DEFINES += HX83102D_HDP_DSI_VDO_TRULY_TRULY_ZAL3251
DEFINES += MTK_ROUND_CORNER_SUPPORT

# Touch (Himax HX83102D on SPI2) support for the boot menu.
# Set to "no" to build a keys-only boot menu.
MTK_TPD_SUPPORT = yes

# The C21 touch controller is wired to SPI2 (0x11012000), not I2C.
# platform/mt6765 has no SPI driver of its own, so pull in the generic
# MTK SPI controller from platform/common/spi.
MTK_SPI_SUPPORT = yes

# Boot Logo (720x1600)
BOOT_LOGO := hdplus1600

# Backlight: the HX83102D panel takes a 12-bit DBV value (0..4095) in the
# DCS 0x51 brightness command.  Passing the generic LED_FULL (255) gives
# DBV 0x00FF (~6%) and a very dim screen, so mt65xx_backlight_on() programs
# this value through the LCM callback instead.
DEFINES += NOVALK_BACKLIGHT_DBV_MAX=4095

# Debug
DEBUG := 2
DEFINES += WITH_DEBUG_UART=1

# USB
CUSTOM_LK_USB_UNIQUE_SERIAL=yes
DEFINES += SERIAL_NUM_FROM_BARCODE=1

# SCP/SSPM
MTK_TINYSYS_SCP_SUPPORT = yes
MTK_TINYSYS_SSPM_SUPPORT = yes

# Modem Config
MTK_PROTOCOL1_RAT_CONFIG = C/Lf/Lt/W/T/G

# from stock
SYSTEM_AS_ROOT = yes
MTK_DTBO_FEATURE = yes
MTK_APPENDED_DTB_SUPPORT = no
MTK_BACKLIGHT_SMOOTH_SUPPORT = yes
MTK_LCM_DEVICE_TREE_SUPPORT = no
MTK_DISPLAY_CUTOUT_POLICY = 0

# ============================================================
# Stock security / verified boot / AVB
# ============================================================
# These must match the stock LK flow shown by the stock fastboot log:
#   [SEC_POLICY] sboot_state = 0x1, lock_state = 0x3
#   set_boot_info_to_rpmb: secureboot_state:1 lock_state:0 ...
#   km_client_read_rpmb_boot_info: last_bootmode:2
#
# MTK_SECURITY_SW_SUPPORT=yes is the important one: it compiles
# sec_policy.o, links libsec, and restores sec_query_device_lock(),
# get_lock_state(), set_boot_info_to_rpmb(), and the Keymaster RPMB
# boot-info path.  With it set to no, load_vfy_boot.c prints
# "security support off; reporting unlocked/orange" and the stock
# Keymaster/RPMB flow never runs.
MTK_VERIFIED_BOOT_SUPPORT = yes
MTK_AVB20_SUPPORT := yes
MTK_SECURITY_SW_SUPPORT = yes
MTK_BUILD_DEFAULT_UNLOCK := no
MTK_DM_VERITY_OFF = no

MTK_SEC_FASTBOOT_UNLOCK_SUPPORT := yes
MTK_SEC_USBDL = no
MTK_SECURITY_ANTI_ROLLBACK = no
MTK_EFUSE_WRITER_SUPPORT = no
MTK_SMC_ID_MGMT = yes
MTK_GOOGLE_TRUSTY_SUPPORT = no
SPM_FW_USE_PARTITION = yes

# ============================================================
# LK heap for seclib image verification
# ============================================================
# The heap size comes from target/rmx2180/rules.mk.  That file used
# to select 0x00200000 (2 MB) for eMMC, which made _heap_end
# 0x48200000 and left only 649836 bytes after _end.  seclib then
# failed in img_utils.c:58:
#   [SECLIB_IMG_VERIFY] malloc memory for heap failed!!
# It now uses the full 0x00900000 (9 MB) LK window, exactly like the
# stock LK (heap ends at 0x48900000).  This gives about 7.5 MB of
# static heap, more than the stock 5.4 MB, so no mblock heap is
# needed.

# Other
MTK_AB_OTA_UPDATER = no
DEVELOP_STAGE = SB
MTK_LK_ENABLE_DCM = yes
MTK_MT22_MODE := H1
MTK_ENABLE_GENIEZONE = yes
DEFINES += NO_POWER_OFF=y

# Boot menu: no timeout.  It waits until an option is confirmed.

# On-screen touch diagnostics in the boot menu (bring-up stage, firmware CRC
# attempts, frame counters, last raw point).  Set to 0 once touch is verified.
DEFINES += NOVALK_TOUCH_DIAG=1

# key
MT6765_USE_FCHR_ENB_KEY = yes
