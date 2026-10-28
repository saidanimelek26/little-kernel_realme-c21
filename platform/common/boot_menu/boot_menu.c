/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2019. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.

*
* The following software/firmware and/or related documentation ("MediaTek Software")
* have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
* applicable license agreements with MediaTek Inc.
*/

/*
 * NovaLK boot menu.
 *
 * Three options only, drawn with the absolute-pixel primitives added to
 * dev/video/mtk_cfb.c (video_fill_rect / video_draw_text / video_refresh):
 *
 *   1. Reboot to Fastboot
 *   2. Boot to System      <- default, selected by the timeout
 *   3. Reboot to Recovery
 *
 * Input:
 *   - touch: tap directly on a card (Himax HX83112A, see dev/tpd.h)
 *   - keys : VOLUME_UP / VOLUME_DOWN move, POWER confirms
 *   - no input for BOOT_MENU_TIMEOUT_MS: boot NORMAL_BOOT
 *
 * The menu never changes the boot decision made earlier by
 * boot_mode_select(); it only sets g_boot_mode when the user confirms.
 */

#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <platform/mt_typedefs.h>
#include <platform/boot_mode.h>
#include <platform/mtk_key.h>
#include <string.h>
#include <sys/types.h>
#include <target/cust_key.h>
#include <video.h>
#ifdef MTK_TPD_SUPPORT
#include <dev/tpd.h>
#endif
#include <platform/mt_gpt.h>
#include <platform/timer.h>

#ifndef MTK_PMIC_PWR_KEY
#define MTK_PMIC_PWR_KEY 8
#endif

/*
 * How long the boot menu waits for an input before it boots NORMAL_BOOT.
 * Define BOOT_MENU_TIMEOUT_MS=0 to wait forever.
 */
#ifndef BOOT_MENU_TIMEOUT_MS
#define BOOT_MENU_TIMEOUT_MS 10000
#endif

/* Font scales (the LK font is 8x16 pixels). */
#define TITLE_SCALE	5	/* 40 x 80  */
#define LABEL_SCALE	3	/* 24 x 48  */
#define DESC_SCALE	2	/* 16 x 32  */
#define HINT_SCALE	2	/* 16 x 32  */

/* NovaLK palette, 0x00RRGGBB. */
#define NOVA_BG		0x0A0F1A
#define NOVA_SURFACE	0x141E33
#define NOVA_SURFACE_SEL 0x0F3A52
#define NOVA_BORDER	0x26385A
#define NOVA_ACCENT	0x2ED3FF
#define NOVA_TEXT	0xF2F7FF
#define NOVA_TEXT_DIM	0x8FA6C4

struct boot_menu_entry {
	u8 boot_mode;
	const char *label;
	const char *desc;
};

static const struct boot_menu_entry ui_entry[] = {
	{FASTBOOT,	"Reboot to Fastboot",	"USB / bootloader"},
	{NORMAL_BOOT,	"Boot to System",	"Start Android normally"},
	{RECOVERY_BOOT,	"Reboot to Recovery",	"Android recovery mode"},
};

#define BOOT_MENU_ENTRIES	((int)countof(ui_entry))

/* Preselected item: NORMAL_BOOT (also used by the timeout). */
#define BOOT_MENU_DEFAULT_ENTRY	1

/* Computed layout (pixels). */
static int g_w, g_h;
static int g_card_x, g_card_w, g_card_h, g_card_gap, g_cards_top;
static int g_title_y, g_subtitle_y, g_device_y, g_accent_y;
static int g_footer_y;
#ifdef MTK_TPD_SUPPORT
static int g_touch_ok;
#endif

static void novalk_draw_text_center(int cx, int y, int scale, u32 fg, u32 bg,
				    const char *s)
{
	int w = video_text_width(scale, s);

	video_draw_text(cx - w / 2, y, scale, fg, bg, s);
}

static void novalk_layout(void)
{
	int title_h = 16 * TITLE_SCALE;
	int sub_h = 16 * DESC_SCALE;
	int cards_total;
	int footer_h;
	int content;
	int top;

	g_w = video_screen_width();
	g_h = video_screen_height();

	g_card_w = g_w - 2 * 40;
	if (g_card_w > 620)
		g_card_w = 620;
	g_card_x = (g_w - g_card_w) / 2;

	g_card_h = 170;
	g_card_gap = 34;
	cards_total = BOOT_MENU_ENTRIES * g_card_h +
		      (BOOT_MENU_ENTRIES - 1) * g_card_gap;

	footer_h = 3 * (16 * HINT_SCALE + 4);

	content = title_h + 16 + sub_h + 10 + sub_h + 24 + 4 + 70 +
		  cards_total + 90 + footer_h;

	top = (g_h - content) / 2;
	if (top < 24)
		top = 24;

	g_title_y = top;
	g_subtitle_y = g_title_y + title_h + 16;
	g_device_y = g_subtitle_y + sub_h + 10;
	g_accent_y = g_device_y + sub_h + 24;
	g_cards_top = g_accent_y + 4 + 70;
	g_footer_y = g_cards_top + cards_total + 90;
}

static void novalk_paint_background(void)
{
	video_fill_rect(NOVA_BG, 0, 0, g_w, g_h);
}

static void novalk_paint_header(void)
{
	int nova_w, lk_w, x;

	/* Clear only the header band so the cards are not repainted. */
	video_fill_rect(NOVA_BG, 0, 0, g_w, g_cards_top - 24);

	nova_w = video_text_width(TITLE_SCALE, "Nova");
	lk_w = video_text_width(TITLE_SCALE, "LK");
	x = (g_w - (nova_w + lk_w)) / 2;

	video_draw_text(x, g_title_y, TITLE_SCALE, NOVA_ACCENT, NOVA_BG, "Nova");
	video_draw_text(x + nova_w, g_title_y, TITLE_SCALE, NOVA_TEXT, NOVA_BG, "LK");

	novalk_draw_text_center(g_w / 2, g_subtitle_y, DESC_SCALE,
				NOVA_TEXT, NOVA_BG, "BOOT MENU");
	novalk_draw_text_center(g_w / 2, g_device_y, DESC_SCALE,
				NOVA_TEXT_DIM, NOVA_BG, "REALME C21  -  RMX3201");

	video_fill_rect(NOVA_ACCENT, g_w / 2 - 60, g_accent_y, 120, 4);
}

static void novalk_paint_card(int index, int selected)
{
	int x = g_card_x;
	int y = g_cards_top + index * (g_card_h + g_card_gap);
	int w = g_card_w;
	int h = g_card_h;
	u32 fill = selected ? NOVA_SURFACE_SEL : NOVA_SURFACE;
	char num[4];
	int text_x = x + 100;
	int block_h = 16 * LABEL_SCALE + 16 + 16 * DESC_SCALE;
	int text_y = y + (h - block_h) / 2;

	video_fill_rect(fill, x, y, w, h);

	if (selected) {
		video_fill_rect(NOVA_ACCENT, x, y, w, 3);
		video_fill_rect(NOVA_ACCENT, x, y + h - 3, w, 3);
		video_fill_rect(NOVA_ACCENT, x, y, 3, h);
		video_fill_rect(NOVA_ACCENT, x + w - 3, y, 3, h);
	}

	/* Number chip. */
	video_fill_rect(selected ? NOVA_ACCENT : NOVA_BORDER,
			x + 30, y + h / 2 - 22, 44, 44);
	snprintf(num, sizeof(num), "%d", index + 1);
	video_draw_text(x + 30 + (44 - 8 * DESC_SCALE) / 2,
			y + h / 2 - (16 * DESC_SCALE) / 2, DESC_SCALE,
			selected ? NOVA_SURFACE_SEL : NOVA_TEXT, 
			selected ? NOVA_ACCENT : NOVA_BORDER, num);

	video_draw_text(text_x, text_y, LABEL_SCALE, NOVA_TEXT, fill,
			ui_entry[index].label);
	video_draw_text(text_x, text_y + 16 * LABEL_SCALE + 16, DESC_SCALE,
			selected ? NOVA_ACCENT : NOVA_TEXT_DIM, fill,
			ui_entry[index].desc);
}

static void novalk_paint_cards(int select)
{
	int i;

	for (i = 0; i < BOOT_MENU_ENTRIES; i++)
		novalk_paint_card(i, i == select);
}

static void novalk_paint_footer(int seconds_left)
{
	int timer_y = g_footer_y + 40;
#if BOOT_MENU_TIMEOUT_MS > 0
	char line[64];
#else
	(void)seconds_left;
#endif

	video_fill_rect(NOVA_BG, 0, g_footer_y - 16, g_w,
			g_h - (g_footer_y - 16));

	novalk_draw_text_center(g_w / 2, g_footer_y, HINT_SCALE,
				NOVA_TEXT_DIM, NOVA_BG,
				"VOL+/-  move      POWER  select");

#ifdef MTK_TPD_SUPPORT
	if (g_touch_ok) {
		novalk_draw_text_center(g_w / 2, g_footer_y + 40, HINT_SCALE,
					NOVA_TEXT_DIM, NOVA_BG,
					"or tap an option");
		timer_y = g_footer_y + 80;
	}
#endif

	if (BOOT_MENU_TIMEOUT_MS > 0) {
		snprintf(line, sizeof(line), "Auto-boot in %d s", seconds_left);
		novalk_draw_text_center(g_w / 2, timer_y, HINT_SCALE,
					NOVA_ACCENT, NOVA_BG, line);
	} else {
		novalk_draw_text_center(g_w / 2, timer_y, HINT_SCALE,
					NOVA_ACCENT, NOVA_BG,
					"Waiting for your selection");
	}
}

static BOOL boot_menu_key_pressed(unsigned short key)
{
	return mtk_detect_key(key);
}

/* Index of the card under (x, y), or -1. */
static int novalk_hit_test(int x, int y)
{
	int i;

	for (i = 0; i < BOOT_MENU_ENTRIES; i++) {
		int top = g_cards_top + i * (g_card_h + g_card_gap);

		if (x >= g_card_x && x < g_card_x + g_card_w &&
		    y >= top && y < top + g_card_h)
			return i;
	}

	return -1;
}

void boot_menu_select(void)
{
	int select = BOOT_MENU_DEFAULT_ENTRY;
	int seconds_left = -1;
	int need_cards = 1;
	int need_footer = 1;
	int prev_up, prev_down, prev_pwr;
#ifdef MTK_TPD_SUPPORT
	int touch_latched = 0;
#endif
	ulong begin = 0;
	ulong last_timer = 0;

	dprintf(INFO, "\n[BOOTMENU] NovaLK boot menu\n");

	/* Enable the keypad controller (LK never does it) before polling. */
#ifdef MTK_KPD_CTRL_SUPPORT
	mtk_keypad_init();
#endif

#ifdef MTK_TPD_SUPPORT
	if (tpd_init() == 0) {
		g_touch_ok = 1;
		dprintf(INFO, "[BOOTMENU] touch: %s\n", tpd_name());
	} else {
		g_touch_ok = 0;
		dprintf(INFO, "[BOOTMENU] touch: not available\n");
	}
#endif

	novalk_layout();

	dprintf(INFO, "[BOOTMENU] screen %dx%d, cards y=%d h=%d\n",
		g_w, g_h, g_cards_top, g_card_h);

	/* A finger/key held since power-on must not be taken as an input. */
	prev_up = boot_menu_key_pressed(MT65XX_MENU_SELECT_KEY);
	prev_down = boot_menu_key_pressed(MT65XX_MENU_OK_KEY);
	prev_pwr = boot_menu_key_pressed(MTK_PMIC_PWR_KEY);

#ifdef MTK_TPD_SUPPORT
	if (g_touch_ok) {
		int tx, ty;
		ulong start = get_timer(0);

		while (tpd_read(&tx, &ty) == 1 && get_timer(start) < 1000)
			mdelay(20);
	}
#endif

	novalk_paint_background();
	novalk_paint_header();
	novalk_paint_cards(select);
	novalk_paint_footer(BOOT_MENU_TIMEOUT_MS / 1000);
	video_refresh();

	seconds_left = BOOT_MENU_TIMEOUT_MS / 1000;

	begin = get_timer(0);
	last_timer = begin;

	while (1) {
		int up, down, pwr;

		/* ---------------- keys ---------------- */
		up = boot_menu_key_pressed(MT65XX_MENU_SELECT_KEY);
		if (up && !prev_up) {
			select = (select + BOOT_MENU_ENTRIES - 1) %
				 BOOT_MENU_ENTRIES;
			dprintf(INFO, "[BOOTMENU] key up -> %d\n", select);
			need_cards = 1;
		}
		prev_up = up;

		down = boot_menu_key_pressed(MT65XX_MENU_OK_KEY);
		if (down && !prev_down) {
			select = (select + 1) % BOOT_MENU_ENTRIES;
			dprintf(INFO, "[BOOTMENU] key down -> %d\n", select);
			need_cards = 1;
		}
		prev_down = down;

		pwr = boot_menu_key_pressed(MTK_PMIC_PWR_KEY);
		if (pwr && !prev_pwr) {
			dprintf(INFO, "[BOOTMENU] power key, confirm\n");
			break;
		}
		prev_pwr = pwr;

		/* ---------------- touch ---------------- */
#ifdef MTK_TPD_SUPPORT
		if (g_touch_ok) {
			int tx = 0, ty = 0;
			int pressed = tpd_read(&tx, &ty);

			if (pressed == 1) {
				if (!touch_latched) {
					int hit = novalk_hit_test(tx, ty);

					touch_latched = 1;
					if (hit >= 0) {
						select = hit;
						dprintf(INFO,
							"[BOOTMENU] tap %d,%d -> %d\n",
							tx, ty, hit);
						break;
					}
				}
			} else {
				touch_latched = 0;
			}
		}
#endif

		/* ---------------- timeout ---------------- */
#if BOOT_MENU_TIMEOUT_MS > 0
		if (get_timer(begin) >= BOOT_MENU_TIMEOUT_MS) {
			dprintf(INFO, "[BOOTMENU] timeout, default selection\n");
			select = BOOT_MENU_DEFAULT_ENTRY;
			break;
		}

		if (get_timer(last_timer) >= 1000) {
			ulong elapsed = get_timer(begin);
			int left = (int)((BOOT_MENU_TIMEOUT_MS - elapsed + 999) /
					 1000);

			last_timer = get_timer(0);
			if (left < 0)
				left = 0;
			if (left != seconds_left) {
				seconds_left = left;
				need_footer = 1;
			}
		}
#endif

		if (need_cards || need_footer) {
			if (need_cards)
				novalk_paint_cards(select);
			if (need_footer)
				novalk_paint_footer(seconds_left);
			video_refresh();
			need_cards = 0;
			need_footer = 0;
		}

		mdelay(10);
	}

	dprintf(INFO, "[BOOTMENU] selected: %s\n", ui_entry[select].label);

	g_boot_mode = ui_entry[select].boot_mode;

	video_clean_screen();

	return;
}
