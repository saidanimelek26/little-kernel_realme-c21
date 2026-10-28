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
 *   | (o)  WAITING FOR USB     |    <- status card, repainted when the
 *   |      plug the cable ...  |       UDC reports online/offline
 *   +--------------------------+
 *   ----------------------------
 *        fastboot devices        <- accent
 *     POWER + VOL-  ->  reboot   <- dim
 */

#include <string.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_leds.h>
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

static int g_ui_shown;
static int g_ui_usb;

static int g_status_x, g_status_y, g_status_w;

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

	content = title_h + 18 + line_h + 8 + line_h + 26 + 4 + 80 +
		  hero_h + 36 + UI_STATUS_H + 64 + 2 + 28 + line_h + 12 +
		  line_h;

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
