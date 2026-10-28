LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
	-I$(LK_TOP_DIR)/include \
	-I$(LK_TOP_DIR)/platform/$(PLATFORM)/include \
	-I$(LK_TOP_DIR)/platform/common/include

OBJS += \
	$(LOCAL_DIR)/tpd.o \
	$(LOCAL_DIR)/tpd_himax.o

# SPI transport for the HX83102D (Realme C21).  Needs the generic MTK SPI
# controller from platform/common/spi.
ifeq ($(MTK_SPI_SUPPORT),yes)
OBJS += $(LOCAL_DIR)/tpd_himax_spi.o
endif
