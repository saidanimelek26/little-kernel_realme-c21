/*
 * NovaLK Fastboot screen (Realme C21 / RMX3201).
 *
 * The boot menu draws the three option cards with dev/video/mtk_cfb.c
 * primitives; this file reuses the exact same palette, font scales and
 * spacing rules so that entering fastboot keeps the NovaLK identity
 * instead of dropping to a black screen (or the stock MTK
 * "Select Boot Mode:" list).
 *
 * Layout (720 x 1600 reference):
 *
 *        Nova LK                    <- accent + text, scale 5
 *   Developed by Melek Saidani      <- dim + accent, scale 2
 *      FASTBOOT MODE                <- accent, scale 2
 *   REALME C21  -  RMX3201          <- dim, scale 2
 *          ----                     <- accent bar
 *
 *   +--------------------------+
 *   |        FASTBOOT          |    <- hero card, scale 4
 *   | USB / bootloader iface   |
 *   +--------------------------+
 *
 *   +--------------------------+
 *   | DEVICE INFO              |    <- only values actually read from the
 *   | STATE        UNLOCKED    |       device are listed; a value that
 *   | ANDROID      11 (2021-09)|       cannot be read leaves no row
 *   | SERIAL       ...         |       behind and the card shrinks
 *   | ...                      |
 *   +--------------------------+
 *
 *   +--------------------------+
 *   | (o)  WAITING FOR USB     |    <- status card, repainted when the
 *   |      plug the cable ...  |       UDC reports online/offline
 *   +--------------------------+
 *   ----------------------------
 *        fastboot devices        <- accent
 *     POWER + VOL-  ->  reboot   <- dim
 */

#include <string.h>
#include <printf.h>
#include <sys/types.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_leds.h>
#include <platform/boot_mode.h>
#include <video.h>
#include <boot_menu.h>

/* Font scales, same values as the boot menu. */
#define UI_TITLE_SCALE	5	/* 40 x 80 */
#define UI_HERO_SCALE	4	/* 32 x 64 */
#define UI_TEXT_SCALE	2	/* 16 x 32 */

/* NovaLK palette, 0x00RRGGBB (identical to boot_menu.c). */
#define NOVA_BG		0x0A0F1A
#define NOVA_SURFACE	0x141E33
#define NOVA_SURFACE_SEL 0x0F3A52
#define NOVA_BORDER	0x26385A
#define NOVA_ACCENT	0x2ED3FF
#define NOVA_TEXT	0xF2F7FF
#define NOVA_TEXT_DIM	0x8FA6C4

#define UI_FONT_H	16

/* Fixed block heights. */
#define UI_HERO_H	210
#define UI_STATUS_H	190
#define UI_CARD_MAX_W	620
#define UI_CARD_MARGIN	40

/* Device info card metrics. */
#define UI_INFO_TITLE_H	(UI_FONT_H * UI_TEXT_SCALE)
#define UI_INFO_TOP	20
#define UI_INFO_TITLE_GAP 8
#define UI_INFO_ROW_H	38
#define UI_INFO_BOTTOM	20
#define UI_INFO_LABEL_W	190
#define UI_INFO_MAX_ROWS 8

static int g_ui_shown;
static int g_ui_usb;

static int g_status_x, g_status_y, g_status_w;

/* Values read from the device at show time (never hardcoded). */
struct ui_info_row {
	const char *label;
	char value[44];
	u32 color;
};

static struct ui_info_row g_info[UI_INFO_MAX_ROWS];
static int g_info_count;
static int g_info_x, g_info_y, g_info_w, g_info_h;

/*
 * Sources outside this file.  These are the very same values the fastboot
 * protocol publishes, so the screen and "fastboot getvar all" never
 * disagree.
 */
extern char sn_buf[];			/* set by set_serial_num() before us */
extern int get_bat_volt(int times);	/* mV */
extern unsigned get_unlocked_status(void);
extern ssize_t partition_read(const char *part_name, off_t offset,
			      u8 *data, size_t size);

static void ui_text_center(int cx, int y, int scale, u32 fg, u32 bg,
			   const char *s)
{
	video_draw_text(cx - video_text_width(scale, s) / 2, y, scale,
			fg, bg, s);
}

static void ui_card(int x, int y, int w, int h, u32 fill, u32 border)
{
	video_fill_rect(fill, x, y, w, h);

	if (!border)
		return;

	/* 2 px hairline frame, the same weight the boot menu uses for its
	 * selected card. */
	video_fill_rect(border, x, y, w, 2);
	video_fill_rect(border, x, y + h - 2, w, 2);
	video_fill_rect(border, x, y, 2, h);
	video_fill_rect(border, x + w - 2, y, 2, h);
}

/* ------------------------------------------------------------------ */
/* Real device information                                            */
/* ------------------------------------------------------------------ */

static void ui_info_add(const char *label, const char *value, u32 color)
{
	if (g_info_count >= UI_INFO_MAX_ROWS)
		return;

	if (!value || !value[0])
		return;

	g_info[g_info_count].label = label;
	g_info[g_info_count].color = color;
	snprintf(g_info[g_info_count].value,
		 sizeof(g_info[g_info_count].value), "%s", value);
	g_info_count++;
}

/*
 * Android release, decoded from the boot image header that sits on the
 * device ("os_version", the same value the kernel build encodes).
 * Nothing is shown when the header cannot be read or does not parse.
 */
static int ui_android_version(char *out, int out_len)
{
	static u8 hdr[4096] __attribute__((aligned(64)));
	u32 hdr_ver, os;
	u32 rel, major, minor;
	int year, month;
	ssize_t len;

	out[0] = '\0';

	len = partition_read("boot", (off_t)0, hdr, sizeof(hdr));
	if (len != (ssize_t)sizeof(hdr))
		return -1;

	if (memcmp(hdr, "ANDROID!", 8) != 0)
		return -1;

	hdr_ver = (u32)hdr[0x28] | ((u32)hdr[0x29] << 8) |
		  ((u32)hdr[0x2a] << 16) | ((u32)hdr[0x2b] << 24);

	/* Header v3 moved os_version next to the kernel/ramdisk sizes. */
	if (hdr_ver >= 3) {
		os = (u32)hdr[0x10] | ((u32)hdr[0x11] << 8) |
		     ((u32)hdr[0x12] << 16) | ((u32)hdr[0x13] << 24);
	} else {
		os = (u32)hdr[0x2c] | ((u32)hdr[0x2d] << 8) |
		     ((u32)hdr[0x2e] << 16) | ((u32)hdr[0x2f] << 24);
	}

	if (!os)
		return -1;

	/* A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0]. */
	rel = (os >> 25) & 0x7f;
	major = (os >> 18) & 0x7f;
	minor = (os >> 11) & 0x7f;
	year = (int)((os >> 4) & 0x7f) + 2000;
	month = (int)(os & 0x0f);

	if (rel < 4 || rel > 30)
		return -1;

	if (major && year >= 2000 && month >= 1 && month <= 12)
		snprintf(out, out_len, "%u.%u.%u (%d-%02d)",
			 rel, major, minor, year, month);
	else if (major)
		snprintf(out, out_len, "%u.%u.%u", rel, major, minor);
	else if (year >= 2000 && month >= 1 && month <= 12)
		snprintf(out, out_len, "%u (%d-%02d)", rel, year, month);
	else
		snprintf(out, out_len, "%u", rel);

	return 0;
}

/* Copy a fixed size, possibly unterminated, bootloader string. */
static void ui_copy_str(char *out, int out_len, const char *in, int in_len)
{
	int i;

	for (i = 0; i < in_len && i < out_len - 1; i++) {
		char c = in[i];

		if (!c)
			break;
		if (c < 0x20 || c > 0x7e)
			c = ' ';
		out[i] = c;
	}

	while (i > 0 && out[i - 1] == ' ')
		i--;

	out[i] = '\0';
}

static void ui_info_collect(void)
{
	char buf[48];
	unsigned unlocked;
	u32 hw;

	g_info_count = 0;

	/* Bootloader lock state: same value fastboot reports as "unlocked". */
	unlocked = get_unlocked_status();
	if (unlocked <= 1)
		ui_info_add("STATE", unlocked ? "UNLOCKED" : "LOCKED",
			    unlocked ? NOVA_ACCENT : NOVA_TEXT);

	if (ui_android_version(buf, sizeof(buf)) == 0)
		ui_info_add("ANDROID", buf, NOVA_TEXT);

	/* Serial from efuse / product info; hide the LK placeholder. */
	if (sn_buf[0] && strcmp(sn_buf, "0123456789ABCDEF") != 0)
		ui_info_add("SERIAL", sn_buf, NOVA_TEXT);

	/* Preloader version is the bootloader's own build string. */
	if (g_boot_arg) {
		ui_copy_str(buf, sizeof(buf), g_boot_arg->pl_version,
			    sizeof(g_boot_arg->pl_version));
		if (buf[0])
			ui_info_add("BOOTLOADER", buf, NOVA_TEXT);
	}

	hw = mt_get_chip_hw_ver();
	if (hw) {
		snprintf(buf, sizeof(buf), "0x%04X", hw);
		ui_info_add("HW REV", buf, NOVA_TEXT);
	}

	{
		int mv = get_bat_volt(1);

		if (mv > 0) {
			snprintf(buf, sizeof(buf), "%d mV", mv);
			ui_info_add("BATTERY", buf, NOVA_TEXT);
		}
	}
}

static void ui_paint_info(void)
{
	int i;

	if (!g_info_count)
		return;

	ui_card(g_info_x, g_info_y, g_info_w, g_info_h, NOVA_SURFACE,
		NOVA_BORDER);

	video_draw_text(g_info_x + 24, g_info_y + UI_INFO_TOP, UI_TEXT_SCALE,
			NOVA_ACCENT, NOVA_SURFACE, "DEVICE INFO");

	for (i = 0; i < g_info_count; i++) {
		int ry = g_info_y + UI_INFO_TOP + UI_INFO_TITLE_H +
			 UI_INFO_TITLE_GAP + i * UI_INFO_ROW_H;

		video_draw_text(g_info_x + 24, ry, UI_TEXT_SCALE,
				NOVA_TEXT_DIM, NOVA_SURFACE,
				g_info[i].label);
		video_draw_text(g_info_x + 24 + UI_INFO_LABEL_W, ry,
				UI_TEXT_SCALE, g_info[i].color, NOVA_SURFACE,
				g_info[i].value);
	}
}

/* Status card: painted at show time and repainted on UDC events. */
static void ui_paint_status(int online)
{
	int x = g_status_x;
	int y = g_status_y;
	int w = g_status_w;
	u32 fill = online ? NOVA_SURFACE_SEL : NOVA_SURFACE;
	u32 frame = online ? NOVA_ACCENT : NOVA_BORDER;
	int dot = 20;
	int dot_x = x + 44;
	int dot_y = y + 52 + (UI_FONT_H * UI_TEXT_SCALE - dot) / 2;
	int text_x = dot_x + dot + 24;

	ui_card(x, y, w, UI_STATUS_H, fill, frame);

	if (online) {
		video_fill_rect(NOVA_ACCENT, dot_x, dot_y, dot, dot);
	} else {
		/* Hollow dot: idle, waiting for the host. */
		video_fill_rect(frame, dot_x, dot_y, dot, dot);
		video_fill_rect(fill, dot_x + 4, dot_y + 4, dot - 8, dot - 8);
	}

	video_draw_text(text_x, y + 52, UI_TEXT_SCALE,
			online ? NOVA_TEXT : NOVA_TEXT_DIM, fill,
			online ? "USB CONNECTED" : "WAITING FOR USB");
	video_draw_text(text_x, y + 52 + UI_FONT_H * UI_TEXT_SCALE + 16,
			UI_TEXT_SCALE, NOVA_TEXT_DIM, fill,
			online ? "ready for fastboot commands"
			       : "plug the cable into a computer");
}

void novalk_fastboot_ui_show(void)
{
	int w, h, card_w, card_x;
	int title_h = UI_FONT_H * UI_TITLE_SCALE;
	int line_h = UI_FONT_H * UI_TEXT_SCALE;
	int hero_h = UI_HERO_H;
	int info_h;
	int content, top, y, hero_y, x, nova_w, lk_w;

	w = video_screen_width();
	h = video_screen_height();
	if (w <= 0 || h <= 0)
		return;

	/* Make sure the frame is visible even when fastboot was entered
	 * straight from a key press and the LCM left the backlight off. */
	mt65xx_backlight_on();

	card_w = w - 2 * UI_CARD_MARGIN;
	if (card_w > UI_CARD_MAX_W)
		card_w = UI_CARD_MAX_W;
	card_x = (w - card_w) / 2;

	ui_info_collect();
	info_h = 0;
	if (g_info_count)
		info_h = UI_INFO_TOP + UI_INFO_TITLE_H + UI_INFO_TITLE_GAP +
			 g_info_count * UI_INFO_ROW_H + UI_INFO_BOTTOM;

	content = title_h + 18 + line_h + 10 +
		  line_h + 8 + line_h + 26 + 4 + 80 +
		  hero_h + 36 +
		  (info_h ? info_h + 28 : 0) +
		  UI_STATUS_H + 64 + 2 + 28 + line_h + 12 + line_h;

	top = (h - content) / 2;
	if (top < 40)
		top = 40;

	video_fill_rect(NOVA_BG, 0, 0, w, h);

	/* ---------------- brand ---------------- */
	y = top;

	nova_w = video_text_width(UI_TITLE_SCALE, "Nova");
	lk_w = video_text_width(UI_TITLE_SCALE, "LK");
	x = (w - (nova_w + lk_w)) / 2;

	video_draw_text(x, y, UI_TITLE_SCALE, NOVA_ACCENT, NOVA_BG, "Nova");
	video_draw_text(x + nova_w, y, UI_TITLE_SCALE, NOVA_TEXT, NOVA_BG,
			"LK");

	y += title_h + 18;

	/* "Developed by" in dim, the name in accent. */
	{
		int prefix_w = video_text_width(UI_TEXT_SCALE,
						NOVALK_CREDIT_PREFIX);
		int name_w = video_text_width(UI_TEXT_SCALE,
					      NOVALK_CREDIT_NAME);
		int cx = (w - (prefix_w + name_w)) / 2;

		video_draw_text(cx, y, UI_TEXT_SCALE, NOVA_TEXT_DIM, NOVA_BG,
				NOVALK_CREDIT_PREFIX);
		video_draw_text(cx + prefix_w, y, UI_TEXT_SCALE, NOVA_ACCENT,
				NOVA_BG, NOVALK_CREDIT_NAME);
	}

	y += line_h + 10;

	ui_text_center(w / 2, y, UI_TEXT_SCALE, NOVA_ACCENT, NOVA_BG,
		       "FASTBOOT MODE");
	y += line_h + 8;

	ui_text_center(w / 2, y, UI_TEXT_SCALE, NOVA_TEXT_DIM, NOVA_BG,
		       "REALME C21  -  RMX3201");
	y += line_h + 26;

	video_fill_rect(NOVA_ACCENT, w / 2 - 60, y, 120, 4);
	y += 4 + 80;

	/* ---------------- hero card ---------------- */
	hero_y = y;
	ui_card(card_x, hero_y, card_w, hero_h, NOVA_SURFACE, 0);
	ui_text_center(w / 2, hero_y + 52, UI_HERO_SCALE, NOVA_TEXT,
		       NOVA_SURFACE, "FASTBOOT");
	ui_text_center(w / 2, hero_y + 52 + UI_FONT_H * UI_HERO_SCALE + 30,
		       UI_TEXT_SCALE, NOVA_TEXT_DIM, NOVA_SURFACE,
		       "USB / bootloader interface");
	y += hero_h + 36;

	/* ---------------- device info card ---------------- */
	if (info_h) {
		g_info_x = card_x;
		g_info_y = y;
		g_info_w = card_w;
		g_info_h = info_h;
		ui_paint_info();
		y += info_h + 28;
	}

	/* ---------------- USB status card ---------------- */
	g_status_x = card_x;
	g_status_y = y;
	g_status_w = card_w;
	ui_paint_status(g_ui_usb);
	y += UI_STATUS_H + 64;

	/* ---------------- footer ---------------- */
	video_fill_rect(NOVA_BORDER, card_x, y, card_w, 2);
	y += 28;

	ui_text_center(w / 2, y, UI_TEXT_SCALE, NOVA_ACCENT, NOVA_BG,
		       "fastboot devices");
	y += line_h + 12;

	ui_text_center(w / 2, y, UI_TEXT_SCALE, NOVA_TEXT_DIM, NOVA_BG,
		       "POWER + VOL-  ->  reboot");

	video_refresh();

	g_ui_shown = 1;
}

/*
 * Called from the fastboot UDC notify callback.  Only the status card is
 * repainted, so a connect/disconnect never flashes the whole screen.
 */
void novalk_fastboot_ui_usb(int online)
{
	if (!g_ui_shown)
		return;

	online = online ? 1 : 0;
	if (online == g_ui_usb)
		return;

	g_ui_usb = online;
	ui_paint_status(online);
	video_refresh();
}
