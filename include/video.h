/*
** MPC823 Video Controller
** =======================
** (C) 2000 by Paolo Scaffardi (arsenio@tin.it)
** AIRVENT SAM s.p.a - RIMINI(ITALY)
**
*/

#ifndef _VIDEO_H_
#define _VIDEO_H_

/* Video functions */
int drv_video_init(void);

int	video_init	(void *videobase);
void	video_putc	(const char c);
void	video_puts	(const char *s);
void	video_printf	(const char *fmt, ...);

//copy from mtk u-boot-1.1.6 /include/video.h
void video_clean_screen(void);
void video_clear_image_by_color(const unsigned int data,
		unsigned int xpos, unsigned int ypos, unsigned int width, unsigned int height);
void video_set_cursor(int row, int col);
int  video_get_rows(void);
int  video_get_colums(void);

void video_set_fg_color(unsigned char r, unsigned char g, unsigned char b);
void video_set_bg_color(unsigned char r, unsigned char g, unsigned char b);
void video_set_default_color(void);

/*
 * NovaLK graphics helpers.
 *
 * These draw with absolute pixel coordinates on panels whose
 * MTK_LCM_PHYSICAL_ROTATION is "0" (the Realme C21 panel).  They do NOT
 * flush the panel: call video_refresh() once when the frame is complete.
 * Colors are 0x00RRGGBB.
 */
int  video_screen_width(void);
int  video_screen_height(void);
void video_refresh(void);

void video_fill_rect(unsigned int color, int x, int y, int width, int height);
void video_draw_text(int x, int y, int scale, unsigned int fg,
		     unsigned int bg, const char *s);
int  video_text_width(int scale, const char *s);

#endif
