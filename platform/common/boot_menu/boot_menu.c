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

/*This file implements MTK boot menu.*/

#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <platform/mt_typedefs.h>
#include <platform/boot_mode.h>
#include <platform/mtk_key.h>
#include <platform/mt_gpt.h>
#include <string.h>
#include <sys/types.h>
#include <target/cust_key.h>
#include <video.h>

extern void cmdline_append(const char *append_string);
typedef void (*entry_cb)(void);

/*
 * How long the boot menu waits for a key press before it falls back to
 * NORMAL_BOOT.  Define BOOT_MENU_TIMEOUT_MS=0 for a target that must wait
 * forever, e.g. in the target rules.mk:
 *     DEFINES += BOOT_MENU_TIMEOUT_MS=0
 */
#ifndef BOOT_MENU_TIMEOUT_MS
#define BOOT_MENU_TIMEOUT_MS 10000
#endif

/* Give up waiting for a key release after this long, so a stuck/defective
 * key can never block the boot menu forever. */
#define BOOT_MENU_KEY_RELEASE_MS 5000

struct boot_menu {
	u8 boot_mode;
	const char *item_text;
	entry_cb callback;
};

//register the callback function
void ftrace_cb()
{
	cmdline_append("androidboot.boot_trace=1");
}

void kmemleak_cb()
{
	cmdline_append("kmemleak=on");
}

void initcall_cb()
{
	cmdline_append("initcall_debug=1 log_buf_len=4M");
}

struct boot_menu ui_entry[] = {
	{RECOVERY_BOOT,	"[Recovery    Mode]",            NULL},
	{FASTBOOT,      "[Fastboot    Mode]",            NULL},
	{NORMAL_BOOT,	"[Normal      Boot]",            NULL},
#if !defined(USER_LOAD) || defined(MTK_BUILD_ENHANCE_MENU)
	{NORMAL_BOOT,   "[Normal      Boot +ftrace]",    ftrace_cb},
	{NORMAL_BOOT,   "[Normal      kmemleak on]",     kmemleak_cb},
	{NORMAL_BOOT,   "[Normal      Boot +initcall]",  initcall_cb},
#endif
};

/* Preselected item: NORMAL_BOOT (also used by the timeout). */
#define BOOT_MENU_DEFAULT_ENTRY 2

/* Every entry is padded to this width so a long item text or a previous
 * "<<==" marker can never leave leftovers on the screen when redrawing. */
#define BOOT_MENU_ITEM_WIDTH 30

static void update_menu(unsigned int index) {
	const char* title_msg = "Select Boot Mode:\n[VOLUME_UP to select.  VOLUME_DOWN is OK.]\n\n";
	char item_line[BOOT_MENU_ITEM_WIDTH + 8];
	unsigned int i;

	video_set_cursor(video_get_rows() / 4, 0);
	video_printf(title_msg);

	for (i = 0; i < countof(ui_entry); i++) {
		snprintf(item_line, sizeof(item_line), "%-30s%s",
			 ui_entry[i].item_text,
			 (i == index) ? "<<==\n" : "\n");
		if (i == index)
			dprintf(INFO, "Switch to %s mode.\n",
				ui_entry[i].item_text);
		video_printf(item_line);
	}

#if BOOT_MENU_TIMEOUT_MS > 0
	video_printf("\nNo key press: boot \"%s\" in %d s\n",
			ui_entry[BOOT_MENU_DEFAULT_ENTRY].item_text,
			BOOT_MENU_TIMEOUT_MS / 1000);
#else
	video_printf("\nWaiting for a selection...\n");
#endif
}

static BOOL boot_menu_key_pressed(unsigned short key)
{
	return mtk_detect_key(key);
}

/*
 * A key that was held to power the phone on (VOLUME_UP opens this menu and
 * VOLUME_DOWN is the "OK" key) must be released before it is accepted as
 * input, otherwise the menu would move/select by itself.
 */
static void wait_for_key_release(unsigned short key)
{
	ulong begin = get_timer(0);

	while (boot_menu_key_pressed(key) &&
	       get_timer(begin) < BOOT_MENU_KEY_RELEASE_MS)
		mdelay(20);
}

void boot_menu_select() {
	unsigned int select = BOOT_MENU_DEFAULT_ENTRY;
#if BOOT_MENU_TIMEOUT_MS > 0
	ulong begin;
#endif

	dprintf(INFO, "\n[BOOTMENU] showing boot menu\n");

	wait_for_key_release(MT65XX_MENU_SELECT_KEY);
	wait_for_key_release(MT65XX_MENU_OK_KEY);

	video_clean_screen();
	update_menu(select);

#if BOOT_MENU_TIMEOUT_MS > 0
	begin = get_timer(0);
#endif

	while (1) {
		if (boot_menu_key_pressed(MT65XX_MENU_SELECT_KEY)) { //VOL_UP
			select = (select + 1) % countof(ui_entry);
			update_menu(select);
			wait_for_key_release(MT65XX_MENU_SELECT_KEY);
			mdelay(50);
		} else if (boot_menu_key_pressed(MT65XX_MENU_OK_KEY)) { //VOL_DOWN
			//use for OK;
			wait_for_key_release(MT65XX_MENU_OK_KEY);
			break;
		}
#if BOOT_MENU_TIMEOUT_MS > 0
		else if (get_timer(begin) >= BOOT_MENU_TIMEOUT_MS) {
			dprintf(INFO, "[BOOTMENU] timeout, default selection\n");
			select = BOOT_MENU_DEFAULT_ENTRY;
			break;
		}
#endif
		mdelay(20);
	}

	dprintf(INFO, "Boot mode:%s is selected!\n", ui_entry[select].item_text);
	g_boot_mode = ui_entry[select].boot_mode;

	if (ui_entry[select].callback)
		ui_entry[select].callback();

	video_clean_screen();

	return;
}
