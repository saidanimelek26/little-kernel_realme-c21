#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/wait.h>
#endif

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------

#define FRAME_WIDTH				(720)
#define FRAME_HEIGHT			(1560)

#define REGFLAG_DELAY			0XFFE
#define REGFLAG_END_OF_TABLE	0xFFF   // END OF REGISTERS MARKER
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static  LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)		(lcm_util.set_reset_pin((v)))

#define UDELAY(n)				(lcm_util.udelay(n))
#define MDELAY(n)				(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

#define LCM_DSI_CMD_MODE		(0)

struct LCM_setting_table 
{
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};


void TC358768_DCS_write_1A_1P(unsigned char cmd, unsigned char para)
{
	unsigned int data_array[16];

	data_array[0] = (0x00023902);
	data_array[1] = (0x00000000 | (para << 8) | (cmd));
	dsi_set_cmdq(data_array, 2, 1);
}
				
static struct LCM_setting_table lcm_initialization_setting[] = {
{0xD5,3,{0x61,0x74,0x97}},
{0xD0,1,{0x10}},

{0x60,1,{0x03}},
{0x61,1,{0x3f}},
{0x62,1,{0xC3}},
{0x63,1, {0x10}},
{0x66,1,{0x40}},
{0x67,1, {0x0C}},
{0x68,1, {0x3B}},
{0x69,1,{0x02}},
{0x6A,1,{0x0C}},
{0x6B,1, {0x3E}},
{0xba,1,{0x01}},//4lane 01£¬3lane 00
{0xb6,1,{0x22}},
{0xa5,1,{0x9B}},
{0xa1,1,{0xFF}},
{0xa4,1,{0x99}},
{0xa0,1,{0xCC}},
{0xF1,1,{0x54}},
{0x93,1,{0x78}},//vcom
{0x9D,3,{0xB5,0xB5,0xB5}},//¶Ô±È¶È
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},

{0xD0,1,{0x11}},
{0x60,1,{0x06}},
{0x62,1,{0xC0}},
{0x63,1,{0x60}},
{0x64,1,{0x04}},
{0x66,1,{0x89}},
{0x67,1,{0x17}},
{0x68,1,{0x8F}},
{0x69,1,{0xC3}},
{0xB7,1,{0x07}},
{0xB8,2,{0xFF,0xFF}},
{0xB9,1,{0xFF}},
{0xBA,1,{0xFF}},
{0xBC,2,{0x00,0x0A}},
{0xB3,44,{0x35,0x3F,0x38,0x3F,0x36,0x3F,0x02,0x3F,0x08,0x0A,0x14,0x16,0x3F,0x3F,0x00,0x3F,0x06,0x3F,0x35,0x35,0x35,0x3F,0x35,0x3F,0x38,0x3F,0x36,0x3F,0x02,0x3F,0x09,0x0B,0x15,0x17,0x3F,0x3F,0x01,0x3F,0x07,0x3F,0x35,0x35,0x35,0x3F}},
{0xA0,8,{0x20,0x01,0x16,0x60,0x20,0x02,0x16,0x60}},
{0xA3,8,{0x20,0x03,0x16,0x60,0x20,0x04,0x16,0x60}},

{0xA1,8,{0x36,0x28,0x16,0x60,0x20,0x02,0x16,0x60}},
{0xA2,8,{0x20,0x01,0x16,0x60,0x36,0x28,0x16,0x60}},

{0xA8,12,{0x00,0x05,0x06,0x25,0x03,0x00,0x06,0x06,0x26,0x03,0x03,0x03}},
{0xA9,12,{0x00,0x07,0x06,0x27,0x03,0x00,0x08,0x06,0x28,0x03,0x03,0x03}},
{0xAE,12,{0x00,0x09,0x06,0x29,0x03,0x00,0x0A,0x06,0x2A,0x03,0x03,0x03}},
{0xAF,12,{0x00,0x0B,0x06,0x2B,0x03,0x00,0x0C,0x06,0x2C,0x03,0x03,0x03}},

{0xB0,7,{0x12,0x22,0x12,0x22,0x22,0x22,0x74}},
{0xA7,5,{0x22,0x16,0x60,0x16,0x60}},
{0xE6,1,{0xE7}},
{0xE7,3,{0x02,0x01,0x7F}},
{0xED,1,{0x03}},
{0xEE,1,{0x37}},
{0xF1,19,{0x7F,0x6A,0x59,0x4c,0x4b,0x3b,0x3f,0x2b,0x45,0x43,0x44,0x63,0x53,0x43,0x3B,0x3A,0x2D,0x1B,0x00}},
{0xF2,19,{0x7F,0x6A,0x59,0x4c,0x4b,0x3b,0x3f,0x2b,0x45,0x43,0x44,0x63,0x53,0x43,0x3B,0x3A,0x2D,0x1B,0x00}},

{0xC2,1,{0x00}},
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},
{0xff,1,{0xFF}},
{0xD0,1,{0x00}},
{0xff,1,{0xFF}},

{0x11,0, {0x00}},
{0xff,1, {0xFF}},
{REGFLAG_DELAY, 120, {}},
{0x29,0, {0x00}},
{0xff,1, {0xFF}},
{REGFLAG_DELAY, 120, {}},
};





static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	
    // Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};



//static struct LCM_setting_table lcm_backlight_level_setting[] = {
	//{0x51, 1, {0xFF}},
	//{REGFLAG_END_OF_TABLE, 0x00, {}}
//};


static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for(i = 0; i < count; i++)
	{
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd)
		{		
		case REGFLAG_DELAY :
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE :
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}	
}

static void init_lcm_registers(void)
{
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS * util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS * params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type	= LCM_TYPE_DSI;

	params->width	= FRAME_WIDTH;
	params->height	= FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode			= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity= LCM_POLARITY_RISING;
	params->dsi.mode			= SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; SYNC_PULSE_VDO_MODE SYNC_EVENT_VDO_MODE

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding		= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=256;

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count				= FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.vertical_sync_active	= 8;
	params->dsi.vertical_backporch		= 30;//8;
	params->dsi.vertical_frontporch		= 30;//8;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.horizontal_sync_active 	= 8;//8;
	params->dsi.horizontal_backporch	= 40;//60;
	params->dsi.horizontal_frontporch	= 40;//140;	
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;	

	// ESD
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;		

	params->dsi.PLL_CLOCK = 220;//dsi clock customization: should config clock value directly
}



static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	
//	SET_RESET_PIN(1);	
//	MDELAY(10);	
//	SET_RESET_PIN(0);
}


#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] =
	    (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] =
	    (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

extern void DSI_clk_HS_mode(char enter);
static unsigned int lcm_compare_id(void)
{	
	unsigned int id=0,id1=0;
	unsigned char buffer[5];
	unsigned int array[3]; 

	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);	
	
	//push_table(lcm_read_id_setting, sizeof(lcm_read_id_setting) / sizeof(struct LCM_setting_table), 1);
			
#ifdef BUILD_LK
	 DSI_clk_HS_mode(0);
#endif
//	array[0]=0x00063902;
//	array[1]=0x000177FF;
//	array[2]=0x00001000;
//	dsi_set_cmdq(array, 3, 1);
//	MDELAY(10); 


	array[0] = 0x00033700;// set return byte number
//	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

//	read_reg_v2(0x04,  buffer, 3);
	read_reg_v2(0x04,  buffer, 3);	
//	id = buffer[0];
	id = buffer[1]<<8 |buffer[2];
	id1 = buffer[2];

#if defined(BUILD_LK)
	printf("lfz lk -- gc9702 Tian Yi Fu 0x%x , 0x%x , 0x%x \n",buffer[0],buffer[1],buffer[2]);
#else
	printk("lfz kernel -- gc9702 Tian Yi Fu 0x%x , 0x%x , 0x%x \n",buffer[0],buffer[1],buffer[2]);
#endif

	// return 1;
	return (id == 0x9702)?1:0;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(30);
	SET_RESET_PIN(1);
	MDELAY(120);
	init_lcm_registers();
}

static void lcm_resume(void)
{
	lcm_init();
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER gc9702_hdplus_vdo_ips_ctc_hanlong_lcm_drv = 
{
	.name				= "gc9702_hdplus_vdo_ips_ctc_hanlong",
	.set_util_funcs		= lcm_set_util_funcs,
	.get_params			= lcm_get_params,
	.init				= lcm_init,
	.suspend			= lcm_suspend,
	.resume				= lcm_resume,
	.compare_id			= lcm_compare_id,	
#if (LCM_DSI_CMD_MODE)
	.set_backlight		= lcm_setbacklight,
	.update				= lcm_update,
#endif
};
