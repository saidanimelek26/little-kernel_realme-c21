/*
 * Himax HX83102D firmware image, Realme C21 (RMX3201).
 *
 * The HX83102D is a zero flash part: it keeps its firmware in SRAM only and
 * the kernel reloads it on every cold boot.  LK runs before that, so the
 * image has to travel inside the bootloader for the touch controller to be
 * usable in the boot menu.
 *
 * Source: stock RMX3201 firmware,
 *   /vendor/firmware/tp/20701/FW_HX83102D_NF_90HZ_TRULY_FAE.img
 * 131072 bytes, 13 part table entries, TRULY panel (the panel this device
 * reports as HX83102D_TRULY_TRULY).
 *
 * .incbin resolves relative to the build root, which is where the assembler
 * is started from.
 */

	.section .rodata

	.global hx83102d_fw_img
	.global hx83102d_fw_img_end

	.align 4
hx83102d_fw_img:
	.incbin "platform/common/touch/fw/hx83102d_truly.bin"
hx83102d_fw_img_end:
	.align 4
