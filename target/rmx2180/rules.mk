LOCAL_DIR := $(GET_LOCAL_DIR)

DEFINES += BUILD_LK
DEFINES += SKIP_MTK_PARTITION_HEADER_CHECK \
           MTK_GPT_SCHEME_SUPPORT

PLATFORM := mt6765

MODULES += \
	dev/keys \
	lib/ptable \
	dev/lcm \

DUMMY_AP := no

ifeq ($(DUMMY_AP), yes)
MEMBASE := 0x50000000 # SDRAM
else
MEMBASE := 0x48000000 # SDRAM
endif

ifeq ($(MTK_MLC_NAND_SUPPORT), yes)
DEFINES += MTK_MLC_NAND_SUPPORT
MEMSIZE := 0x00900000 # 9MB
else ifeq ($(MTK_TLC_NAND_SUPPORT), yes)
DEFINES += MTK_TLC_NAND_SUPPORT
MEMSIZE := 0x00900000 # 9MB
else
# RMX3201 uses eMMC, not MLC/TLC NAND.  Keep the full 9 MB LK
# window so the static heap after _end is large enough for
# seclib_image_buf_init() (stock LK ends at 0x48900000).
MEMSIZE := 0x00900000 # 9MB
endif


SCRATCH_ADDR     := 0x48B00000
SCRATCH_SIZE     := 0x08000000 # 128MB

HAVE_CACHE_PL310 := no
MTK_LM_MODE := yes
MTK_FASTBOOT_SUPPORT := yes
LK_PROFILING := yes
DEVICE_TREE_SUPPORT := yes
MTK_DLPT_SUPPORT := yes
MTK_USB2JTAG_SUPPORT := yes

MACH_TYPE := mt6765

DEFINES += \
	MEMBASE=$(MEMBASE)\
	SCRATCH_ADDR=$(SCRATCH_ADDR)\
	SCRATCH_SIZE=$(SCRATCH_SIZE)\
	MEMSIZE=$(MEMSIZE)

ifeq ($(DUMMY_AP), yes)
DEFINES += DUMMY_AP
endif

ifeq ($(HAVE_CACHE_PL310), yes)
DEFINES += HAVE_CACHE_PL310
endif

ifeq ($(MTK_LM_MODE), yes)
DEFINES += MTK_LM_MODE
endif

ifeq ($(DEVICE_TREE_SUPPORT), yes)
DEFINES += DEVICE_TREE_SUPPORT
endif

ifeq ($(MTK_FASTBOOT_SUPPORT), yes)
DEFINES += MTK_FASTBOOT_SUPPORT
endif

ifeq ($(LK_PROFILING), yes)
DEFINES += LK_PROFILING
endif

ifneq ($(filter user userdebug, $(TARGET_BUILD_VARIANT)),)
DEFINES += USER_BUILD
endif

ifeq ($(MTK_DLPT_SUPPORT), yes)
DEFINES += MTK_DLPT_SUPPORT
endif

INCLUDES += -I$(LOCAL_DIR)/include
INCLUDES += -I$(LOCAL_DIR)/include/target
INCLUDES += -I$(LOCAL_DIR)/inc

OBJS += \
        $(LOCAL_DIR)/init.o \
        $(LOCAL_DIR)/cust_display.o\
        $(LOCAL_DIR)/cust_leds.o\
        $(LOCAL_DIR)/power_off.o\
        $(LOCAL_DIR)/fastboot_oem_commands.o
