#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"
#include "mt_gpio.h"
//#include "mach/gpio_const.h"

//#define GPIO_DISP_ID0_PIN GPIO93
// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (960)

#define REGFLAG_DELAY             							0XFFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

#define   LCM_DSI_CMD_MODE							(0)

struct LCM_setting_table 
{
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};

						
static struct LCM_setting_table lcm_initialization_setting[] = {
//=========================================  initial code  =========================================//
//----------CMD2 enable-----------------
{0x00,1,{0x00}},
{0xFF,3,{0x80,0x19,0x01}},
{0x00,1,{0x80}},
{0xFF,2,{0x80,0x19}},

//-----------resolution adjustment------------
{0x00,1,{0x90}},
{0xB3,1,{0x02}},//00h=480*800,01h=480*864,02h=480*854,03h=480*800,04h=480*720,05h=480*480,06h=480*360
{0x00,1,{0x92}},
{0xB3,1,{0x40}},

{0x00,1,{0xA2}},
{0xB3,4,{0x01,0xE0,0x03,0xC0}},//480x960
{0x00,1,{0xA7}},
{0xB3,1,{0x04}},//enable auto resolution

//-------------------Tcon & RTN setting----------------------------
{0x00,1,{0x80}},
{0xC0,6,{0x00,0x59,0x00,0x15,0x15,0x00}},

//---------------shift1,2,3 setting-------------------------
{0x00,1,{0x90}},
{0xC0,6,{0x00,0x10,0x00,0x00,0x00,0x04}},

//----------------reg_oscref_video_hs_shift-----------------
{0x00,1,{0xA0}},
{0xC1,1,{0xe8}},


//*******************GOA Settings********************************

{0x00,1,{0x80}},
{0xCE,12,{0x87,0x03,0x00,0x86,0x03,0x00,0x85,0x03,0x00,0x84,0x03,0x00}},

//{0x00,1,{0x90}},
//{0xCE,12,{0x33,0xC0,0x00,0x33,0xC1,0x00,0x33,0xC2,0x00,0x33,0xC3,0x00}},

{0x00,1,{0x90}},
{0xCE,12,{0x33,0xBC,0x00,0x33,0xBD,0x00,0x33,0xBE,0x00,0x33,0xBF,0x00}},


{0x00,1,{0xA0}},
{0xCE,14,{0x38,0x05,0x03,0xC0,0x00,0xA0,0x02,0x38,0x04,0x03,0xC1,0x00,0xA0,0x02}},

{0x00,1,{0xB0}},
{0xCE,14,{0x38,0x03,0x03,0xC2,0x00,0xA0,0x02,0x38,0x02,0x03,0xC3,0x00,0xA0,0x02}},

{0x00,1,{0xC0}},
{0xCE,14,{0x38,0x01,0x03,0xC4,0x00,0xA0,0x02,0x38,0x00,0x03,0xC5,0x00,0xA0,0x02}},

{0x00,1,{0xD0}},
{0xCE,14,{0x30,0x00,0x03,0xC6,0x00,0xA0,0x02,0x30,0x01,0x03,0xC7,0x00,0xA0,0x02}},

{0x00,1,{0xC7}},
{0xCF,1,{0x00}},

//--------------------Power on/off sequence-------------------------
{0x00,1,{0xC0}},
{0xCB,15,{0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00}},

{0x00,1,{0xD0}},
{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01}},

{0x00,1,{0xE0}},
{0xCB,9,{0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00}},

//--------------------------U2D------------------------------------
{0x00,1,{0x80}},
{0xCC,10,{0x00,0x00,0x00,0x03,0x01,0x09,0x0b,0x0d,0x0f,0x05}},

{0x00,1,{0x90}},
{0xCC,1,{0x07}},

{0x00,1,{0xA0}},
{0xCC,15,{0x08,0x06,0x10,0x0e,0x0c,0x0a,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

//--------------------------D2U------------------------------------
{0x00,1,{0xB0}},
{0xCC,10,{0x00,0x00,0x00,0x06,0x08,0x0C,0x0A,0x10,0x0E,0x04}},

{0x00,1,{0xC0}},
{0xCC,1,{0x02}},

{0x00,1,{0xD0}},
{0xCC,15,{0x01,0x03,0x0D,0x0F,0x09,0x0B,0x07,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

//*********************************************************************************

//----------------------------------------------------------
{0x00,1,{0x80}},
{0xc1,2,{0x03,0x44}},

//---------------power control----------------
{0x00,1,{0x90}},
{0xC5,2,{0x8E,0x97}},//VDDA pump ration=2*VDD, LVD of VDDA=GVDD+0.4, DLVD of VDDA=GVDD+0.8, VGH=15V, VGL=-10V

{0x00,1,{0x82}},
{0xC5,1,{0xb1}},	//VDDA clamp voltage=6.5V

//---------------GVDD & GVDDN output voltage----------------
{0x00,1,{0x00}},
{0xD8,2,{0x6F,0x7F}},	//99
//--------------- VCOM output voltage----------------
{0x00,1,{0x00}},
{0xD9,1,{0x01}},//57 0x0f

//-------------------LVDSVDD&VDD18 setting---------------------------------------
{0x00,1,{0xb1}},
{0xC5,1,{0xb9}},

//---------------VGH & VGL pump setting------------------------------
{0x00,1,{0x92}},
{0xC5,1,{0x01}},//VGH=6*VDD, VGL=-6*VDD, ??PUMP4_EN_ASDM_HV=0(C580[4]=0)???]?w?~?|??? 

{0x00,1,{0x94}},
{0xC5,2,{0x66,0x66}},

//----------------Pre-Charge-----------------------------------
{0x00,1,{0x89}},
{0xC4,1,{0x08}},
//---------------- Gamma code Setting ----------------//
//{0x00,1,{0x00}},
//{0xE1,20,{0x0a,0x14,0x1d,0x2a,0x35,0x43,0x45,0x65,0x5f,0x77,0x8c,0x7a,0x81,0x58,0x50,0x40,0x2e,0x1d,0x14,0x05}},
														// 255  253  251  248  244  239  231  203  175  143  112   80   52   24   16   11    7    4    2    0
//{0x00,1,{0x00}},
//{0xE2,20,{0x0a,0x14,0x1d,0x2a,0x35,0x43,0x45,0x65,0x5f,0x77,0x8c,0x7a,0x81,0x58,0x50,0x40,0x2e,0x1d,0x14,0x05}},
//-------------------------------------------------------------
//----------0920---------
{0x00,1,{0x00}},                                                                               //10
{0xE1,20,{0x00,0x1c,0x2f,0x43,0x56,0x69,0x6e,0x99,0x87,0x9f,0x62,0x45,0x57,0x37,0x3a,0x2c,0x25,0x1b,0x10,0x00}},
//             255 , 253, 251,248,  244, 239, 231, 203, 175,143,  112, 80,  52,   24,  16,  11,  7,   4,  2,    0   
{0x00,1,{0x00}},
{0xE2,20,{0x00,0x1c,0x2f,0x43,0x56,0x69,0x6e,0x9a,0x88,0x9c,0x66,0x4e,0x5c,0x40,0x3a,0x2c,0x25,0x1b,0x10,0x00}},
//              255 , 253, 251,248,  244, 239, 231, 203, 175,143,  112, 80,  52,   24,  16,  11,  7,   4,  2,    0  
/////////////////////////////////////////////////
{0x00,1,{0xA2}}, 
{0xC0,3,{0x00,0x00,0x00}},





{0x00,1,{0x90}},
{0xC0,6,{0x00,0x10,0x00,0x00,0x00,0x04}},
{0x00,1,{0x80}},
{0xC4,1,{0x30}},
{0x00,1,{0x80}},
{0xC1,3,{0x03,0x44,0x84}},
{0x00,1,{0x98}},
{0xC0,1,{0x00}},
{0x00,1,{0xA9}},
{0xC0,1,{0x0A}},
{0x00,1,{0xB0}},
{0xC1,6,{0x20,0x00,0x00,0x00,0x02,0x01}},
{0x00,1,{0xE1}},
{0xC0,2,{0x40,0x30}},
{0x00,1,{0xA0}},
{0xC1,1,{0xE8}},
{0x00,1,{0x90}},
{0xB6,2,{0xB4,0x5A}},
{0x00,1,{0x83}},
{0xC5,1,{0x01}},
{0x00,1,{0xC4}},
{0xC5,2,{0x00,0x00}},
{0x00,1,{0xBA}},
{0xF5,2,{0x00,0x00}},
{0x00,1,{0x00}},
{0xFB,1,{0x01}},
{0x00,1,{0xB4}},
{0xC0,1,{0x00}},
{0x00,1,{0x80}},
{0xFF,2,{0xFF,0xFF}},
{0x00,1,{0x00}},
{0xFF,3,{0xFF,0xFF,0xFF}},

 {0x11,0,{0x00}},
 {REGFLAG_DELAY,120,{}},
 {0x29,0,{0x00}},
 {REGFLAG_DELAY,20,{}},
 {REGFLAG_END_OF_TABLE, 0x00, {} }	
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},
    // Sleep Mode On
	{0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {0xFF,6,{0x77,0x01,0x00,0x00,0x11,0x80}},
    {0xFF,5,{0x77,0x01,0x00,0x00,0x91}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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

	params->type  = LCM_TYPE_DSI;

	params->physical_width = 71;
	params->physical_height = 143;

	params->width = FRAME_WIDTH;
	params->height= FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	params->dsi.mode = SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; SYNC_PULSE_VDO_MODE SYNC_EVENT_VDO_MODE

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=256;

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
    
    params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
    params->dsi.vertical_active_line=FRAME_HEIGHT;
    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 45;//8;
    params->dsi.vertical_frontporch = 48;//8;
    params->dsi.vertical_active_line = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active = 8;//8;
    params->dsi.horizontal_backporch = 80;//60;
    params->dsi.horizontal_frontporch = 80;//140;	
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;	
/*
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;		
*/
    params->dsi.PLL_CLOCK = 240;//dsi clock customization: should config clock value directly

    params->dsi.ssc_disable = 0;  // ssc disable control (1: disable, 0: enable, default: 0)
    params->dsi.ssc_range = 4;  // ssc range control (1:min, 8:max, default: 4)
}



static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);	
	MDELAY(10);	
	SET_RESET_PIN(0);
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
	unsigned int id=0,id1=0,id2=0;
	unsigned char buffer[5];
	unsigned int array[3]; 
	unsigned int gpio_id0=0;

	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);	

#ifdef BUILD_LK
	DSI_clk_HS_mode(0);
#endif
	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDA, buffer, 1);
	id = buffer[0]; //we only need ID

//gpio_id0 = mt_get_gpio_in(GPIO_DISP_ID0_PIN);

#if defined(BUILD_LK)
	printf("lk -- otm8019a hanlong 0x%x\n", id);
	printf("lk -- otm8019a hanlong gpio_id0 = %d\n", gpio_id0);
#else
	printk("kernel -- otm8019a hanlong 0x%x\n",id);
	printk("kernel -- otm8019a hanlong gpio_id0 = %d\n", gpio_id0);
#endif


/*	if ((id == 0x40) && (gpio_id0 == 0))
		return 1;
	else
		return 0;*/
	return (id == 0x40)?1:0;

}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(100);
	
	init_lcm_registers();
}
static void lcm_resume(void)
{
	lcm_init();
}

//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER otm8019a_fwplus_dsi_vdo_ivo_hanlong_lcm_drv = {
	.name			= "otm8019a_fwplus_dsi_vdo_ivo_hanlong",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params		= lcm_get_params,
	.init			= lcm_init,
	.suspend		= lcm_suspend,
	.resume			= lcm_resume,
	.compare_id		= lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};
