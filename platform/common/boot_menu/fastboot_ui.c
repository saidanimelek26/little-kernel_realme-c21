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
 *     [RECOVERY] [SYSTEM]  <- small reboot buttons
 */

#include <string.h>
#include <printf.h>
#include <sys/types.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_leds.h>
#include <platform/boot_mode.h>
#include <video.h>
#include <boot_menu.h>
#include <platform/mt_rtc.h>
#include <platform/mtk_wdt.h>
#include <platform/timer.h>
#include <dev/udc.h>
#ifdef MTK_TPD_SUPPORT
#include <kernel/thread.h>
#include <dev/tpd.h>
#include <target/cust_tpd.h>
#endif

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

	/* Serial from efuse / product info; hide the LK placeholder. */
	if (sn_buf[0] && strcmp(sn_buf, "0123456789ABCDEF") != 0)
		ui_info_add("SERIAL", sn_buf, NOVA_TEXT);

	/*
	 * Installed RAM, summed from the DRAM ranks the preloader detected
	 * and handed over in the boot arguments (a measured value).
	 */
	if (g_boot_arg) {
		u64 total = 0;
		u32 i;

		for (i = 0; i < g_boot_arg->dram_rank_num && i < 4; i++)
			total += g_boot_arg->dram_rank_size[i];

		if (total) {
			u32 mb = (u32)(total >> 20);

			if (mb >= 1024 && (mb % 1024) == 0)
				snprintf(buf, sizeof(buf), "%u GB", mb / 1024);
			else
				snprintf(buf, sizeof(buf), "%u MB", mb);
			ui_info_add("RAM", buf, NOVA_TEXT);
		}

		/* Preloader version string as reported by the preloader. */
		ui_copy_str(buf, sizeof(buf), g_boot_arg->pl_version,
			    sizeof(g_boot_arg->pl_version));
		if (buf[0])
			ui_info_add("PRELOADER", buf, NOVA_TEXT);
	}

	/* Compile time stamp of this bootloader. */
	snprintf(buf, sizeof(buf), "%s", __DATE__);
	ui_info_add("LK BUILD", buf, NOVA_TEXT);

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

/* ------------------------------------------------------------------ */
/* Reboot buttons                                                     */
/* ------------------------------------------------------------------ */

#define UI_BTN_H	72
#define UI_BTN_W	250
#define UI_BTN_GAP	24

enum { BTN_RECOVERY = 0, BTN_SYSTEM = 1, BTN_COUNT = 2 };

static const char *const g_btn_label[BTN_COUNT] = { "RECOVERY", "SYSTEM" };
static int g_btn_x[BTN_COUNT], g_btn_y;

static void ui_paint_button(int idx, int pressed)
{
	u32 fill = pressed ? NOVA_SURFACE_SEL : NOVA_SURFACE;
	u32 frame = pressed ? NOVA_ACCENT : NOVA_BORDER;
	int x = g_btn_x[idx];
	int y = g_btn_y;
	int tw = video_text_width(UI_TEXT_SCALE, g_btn_label[idx]);

	ui_card(x, y, UI_BTN_W, UI_BTN_H, fill, frame);
	video_draw_text(x + (UI_BTN_W - tw) / 2,
			y + (UI_BTN_H - UI_FONT_H * UI_TEXT_SCALE) / 2,
			UI_TEXT_SCALE, pressed ? NOVA_ACCENT : NOVA_TEXT, fill,
			g_btn_label[idx]);
}

#ifdef MTK_TPD_SUPPORT
static int ui_btn_hit(int x, int y)
{
	int i;

	for (i = 0; i < BTN_COUNT; i++) {
		if (x >= g_btn_x[i] && x < g_btn_x[i] + UI_BTN_W &&
		    y >= g_btn_y && y < g_btn_y + UI_BTN_H)
			return i;
	}

	return -1;
}

/* Same sequence the fastboot "reboot" / "reboot-recovery" commands use. */
static void ui_reboot(int idx)
{
	if (idx == BTN_RECOVERY)
		Set_RTC_Recovery_Mode(1);

	/* Disconnect USB first so the host does not hang on a dead device. */
	udc_stop();
	mdelay(500);
	mtk_arch_reset(1);
}

/*
 * The fastboot screen has no event loop of its own, so a small thread polls
 * the touch controller.  If the boot menu already brought touch up this only
 * reuses it; when fastboot was entered with the keys it brings it up here.
 */
static int ui_touch_thread(void *arg)
{
	int latched = 1;	/* ignore a finger already on the glass */
	int tx, ty;

	(void)arg;

	if (tpd_init() != 0)
		return 0;

	for (;;) {
		if (tpd_read(&tx, &ty) == 1) {
			if (!latched) {
				int sx = tx * video_screen_width() /
					 TPD_PANEL_WIDTH;
				int sy = ty * video_screen_height() /
					 TPD_PANEL_HEIGHT;
				int hit = ui_btn_hit(sx, sy);

				latched = 1;
				if (hit >= 0) {
					ui_paint_button(hit, 1);
					video_refresh();
					ui_reboot(hit);
				}
			}
		} else {
			latched = 0;
		}

		thread_sleep(20);
	}

	return 0;
}

static void ui_touch_start(void)
{
	static int started;
	thread_t *t;

	if (started)
		return;

	t = thread_create("novalk_touch", ui_touch_thread, NULL,
			 DEFAULT_PRIORITY, 16 * 1024);
	if (!t)
		return;

	started = 1;
	thread_resume(t);
}
#endif

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
		  UI_STATUS_H + 64 + 2 + 28 + line_h + 28 + UI_BTN_H;

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
	y += line_h + 28;

	/* Two small reboot buttons side by side: recovery and system. */
	g_btn_y = y;
	g_btn_x[BTN_RECOVERY] = w / 2 - UI_BTN_GAP / 2 - UI_BTN_W;
	g_btn_x[BTN_SYSTEM] = w / 2 + UI_BTN_GAP / 2;
	ui_paint_button(BTN_RECOVERY, 0);
	ui_paint_button(BTN_SYSTEM, 0);

	video_refresh();

	g_ui_shown = 1;

#ifdef MTK_TPD_SUPPORT
	ui_touch_start();
#endif
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
