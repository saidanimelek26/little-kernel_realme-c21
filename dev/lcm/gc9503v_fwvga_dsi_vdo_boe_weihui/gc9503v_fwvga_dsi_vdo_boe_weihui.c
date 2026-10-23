//modify:huang 20140702 ;form fae


#ifdef BUILD_LK
#else
#include <linux/string.h>
#if defined(BUILD_LK)
#include <asm/arch/mt_gpio.h>
#else
//#include <mach/mt_gpio.h>
#endif
#endif
#include "lcm_drv.h"
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(960)

#define LCM_ID 												(0x9504)

#define REGFLAG_DELAY             							0xFFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER

#ifndef TRUE
#define   TRUE     1
#endif

#ifndef FALSE
#define   FALSE    0

#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define AUXADC_LCM_VOLTAGE_CHANNEL     12
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_GetOneChannelValue_Cali(int Channel, int*voltage);

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

//static unsigned char vcom_value= 0x6A;

static struct LCM_setting_table lcm_initialization_setting[] = {

{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x00}},

{0xF6, 2,{0x5A, 0x87}},

{0xC1, 1,{0x3F}},

{0xC2, 1,{0x0E}},

{0xC6, 1,{0xF8}},

{0xC9, 1,{0x10}},

{0xCD, 1,{0x25}},

{0xF8, 1,{0x8A}},

{0xAC, 1,{0x65}},

{0xA7, 1,{0x47}},

{0xA0, 1,{0xdd}}, ////0xff  20200402  james update

{0xFA, 4,{0x08, 0x08, 0x00, 0x04}},

{0xA3, 1,{0x2E}},

{0xFD, 3,{0x28, 0x3C, 0x00}},

{0x71, 1,{0x48}},

{0x72, 1,{0x48}},

{0x73, 2,{0x00, 0x44}},

{0x97, 1,{0xEE}},

{0x83, 1,{0x93}},

{0x9A, 1,{0x78}},

{0x9B, 1,{0x48}},

{0x82, 2,{0x50, 0x50}},

{0xB1, 1,{0x10}},

{0x7A, 2,{0x13, 0x1A}},

{0x7B, 2,{0x13, 0x1A}},

{0x6D, 32,{0x00, 0x00, 0x0A, 0x10, 0x08, 0x1F, 0x1F, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x01, 0x0F, 0x09, 0x00, 0x00}},

{0x64, 16,{0x18, 0x05, 0x03, 0xC4, 0x03, 0x03, 0x18, 0x04, 0x03, 0xC3, 0x03, 0x03,0x7A, 0x7A,0x7A, 0x7A}},

{0x67, 16,{0x18, 0x03, 0x03, 0xC2, 0x03, 0x03, 0x18, 0x02, 0x03, 0xC1, 0x03, 0x03,0x7A, 0x7A,0x7A, 0x7A}},

{0x60, 8,{0x18, 0x07, 0x7A, 0x7A, 0x18, 0x02, 0x7A, 0x7A}},

{0x63, 8,{0x18, 0x02, 0x7A, 0x7A, 0x18, 0x06, 0x7A, 0x7A}},

{0x69, 7,{0x14, 0x22, 0x14, 0x22, 0x44, 0x22, 0x08}},

{0xD1, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},

{0xD2, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},

{0xD3, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},

{0xD4, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},

{0xD5, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},

{0xD6, 52,{0x00, 0x00, 0x00, 0x10, 0x00, 0x26, 0x00, 0x3B, 0x00, 0x4B, 0x00, 0x69, 0x00, 0x85, 0x00, 0xB3, 0x00, 0xDB, 0x01, 0x1B, 0x01, 0x4F, 0x01, 0xA8, 0x01, 0xEF, 0x01, 0xF0, 0x02, 0x34, 0x02, 0x88, 0x02, 0xc5, 0x03, 0x1d, 0x03, 0x5c, 0x03, 0xA9, 0x03, 0xC8, 0x03, 0xE8, 0x03, 0xF0, 0x03, 0xF5, 0x03, 0xFE, 0x03, 0xFF}},


{0x11, 0,{0x00}},
{REGFLAG_DELAY, 100, {}},
{0x29, 0,{0x00}},

{REGFLAG_DELAY, 20, {}}, 
{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update) {
	unsigned int i;

	for(i = 0; i < count; i++) {

		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

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

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util) {
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}



static void lcm_get_params(LCM_PARAMS *params) {

	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode                 = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;

#if defined(LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM                = LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Highly depends on LCD driver capability.
	// Not support in MT6573

	//params->dsi.DSI_WMEM_CONTI=0x3C;
	//params->dsi.DSI_RMEM_CONTI=0x3E;


//	params->dsi.packet_size=256;

	// Video mode setting
	params->dsi.intermediat_buffer_num = 2;

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
    params->dsi.cont_clock =1;

		
		params->dsi.vertical_sync_active				= 2; //8
		params->dsi.vertical_backporch					= 15;  //24
		params->dsi.vertical_frontporch					= 8;  //12
		params->dsi.vertical_active_line = FRAME_HEIGHT;	

		params->dsi.horizontal_sync_active				= 10;//10
		params->dsi.horizontal_backporch				= 30;//20
		params->dsi.horizontal_frontporch				= 30;//20
		params->dsi.horizontal_active_pixel = FRAME_WIDTH;

  
	// Bit rate calculation
	params->dsi.PLL_CLOCK               = 200;//185
}
static unsigned int lcm_compare_id(void);

static void lcm_init(void) {
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);//10
	SET_RESET_PIN(1);
	MDELAY(120);//50

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}
static struct LCM_setting_table lcm_sleep_in_setting[] =
{
	{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0xc1, 1,{0x3f}},
 	{0x6c, 1,{0x60}},
	{REGFLAG_DELAY,20,{}},
	{0xb1, 1,{0x00}},	
	{0xFA, 4,{0x7f, 0x00, 0x00,0x00}},
	{REGFLAG_DELAY,20,{}},
	{0x6c, 1,{0x50}},
	{REGFLAG_DELAY,10,{}},
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 50, {}}, 
    {0x10, 1, {0x00}},    
    {REGFLAG_DELAY, 20, {}},
	{0xF0, 5,{0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0xC2, 1,{0xCE}},
	{0xC3, 1,{0xCD}},
	{0xC6, 1,{0xFC}},
	{0xC5, 1,{0x03}},
	{0xCD, 1,{0x64}},
	{0xC4, 1,{0xFF}},
	{0xC9, 1,{0xCD}},
	
	{0xF6, 2,{0x5A, 0x87}},
	{0xFD, 3,{0xAA, 0xAA, 0x0A}},
	{0xFE, 2,{0x6A, 0x0A}},
	{0x78, 2,{0x2A, 0xAA}},
	{0x92, 2,{0x17, 0x08}},
	{0x77, 2,{0xAA, 0x2A}},
	{0x76, 2,{0xAA, 0xAA}},
	
	{0x84, 1,{0x00}},
	{0x78, 2,{0x2B, 0xBA}},
	{0x89, 1,{0x73}},
	{0x88, 1,{0x3A}},
	{0x85, 1,{0xB0}},
	{0x76, 2,{0xEB, 0xAA}},
	{0x94, 1,{0x80}},
	{0x87, 3,{0x04, 0x07, 0x30}},
	{0x93, 1,{0x27}},
	{0xAF, 1,{0x02}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
static void lcm_suspend(void) {
	
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
    SET_RESET_PIN(1);
	MDELAY(50);//

}

#if !defined(BUILD_LK) && defined(SUPPORT_LCM_DEBUG)
#define USER_LCM_COLOR_NORMAL		(0)
#define USER_LCM_COLOR_INVERSE 	(1)
extern int user_lcm_color_status;
#endif



static void lcm_resume(void) {
	//vcom_value--;
	//lcm_initialization_setting[14].para_list[0]=vcom_value;
	//lcm_compare_id(); //for debug
	//lcm_initialization_setting[22].para_list[0]+=2;
	
	//lcm_initialization_setting[15].para_list[0]-=1;
	//lcm_initialization_setting[16].para_list[0]-=1;
	//lcm_initialization_setting[17].para_list[0]-=1;
	//lcm_initialization_setting[17].para_list[1]-=1;
	
	lcm_init();
}
//mike0320
/*
static struct LCM_setting_table lcm_compare_id_setting[] = {
	{0x01,  0, {0x00}},
	{REGFLAG_DELAY,	120,{}},	//
	{0x11,  0, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x11} },
	{0xD1,  1, {0x11} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
*/
static unsigned int lcm_read_adc_value(void)
{
        int adc[1];
		int voltage[1];
        int data[4] ={0,0,0,0};
        int sum = 0;
        int adc_vol=0;
        int num = 0;
        int ret =0;

        for(num=0;num<10;num++)
        {
                //ret = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL, data, adc);
                //sum+=(data[0]*100+data[1]);
				ret = IMM_GetOneChannelValue_Cali(AUXADC_LCM_VOLTAGE_CHANNEL, voltage);
				sum += voltage[0];
        }
        adc_vol = sum/10;
#if defined(BUILD_LK)
        printf("sunritel test adc_vol is %d\n",adc_vol);
#else
        printk("sunritel test adc_vol is %d\n",adc_vol);
#endif
        if(adc_vol < 300000)
        {
                printf("sunritel gc9503v_fwvga_dsi_vdo_boe_weihui,return 1 \n");//38
                return 1;
        }
        else
        {
                printf("sunritel gc9503v_fwvga_dsi_vdo_xinli_hongli ,return 0\n");//115
                return 0;
        }



}

static unsigned int lcm_compare_id(void) {
	unsigned int lcd_id = 0;
	unsigned int data_array[16];
	unsigned char buffer[3] = {0};
	unsigned int gpio_id0 = -1;

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(100);

	// read id return four byte
	data_array[0] = 0x00033700;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
#if 0
	read_reg_v2(0xDA, &buffer[0], 1);
	read_reg_v2(0xDB, &buffer[1], 1);
	read_reg_v2(0xDC, &buffer[2], 1);
#else
	read_reg_v2(0x04, buffer, 3);
#endif
#if defined(BUILD_LK)
	printf("xwliu gc9503v_cmi,lk id0=0x%.2x id1=0x%.2x id2=0x%.2x\n",buffer[0],buffer[1],buffer[2]);
#else
	printk("xwliu gc9503v_cmi,lk id0=0x%.2x id1=0x%.2x id2=0x%.2x\n",buffer[0],buffer[1],buffer[2]);
#endif
	lcd_id = ((buffer[1] & 0xff)<<8) | buffer[2];
#if defined(BUILD_LK)
	printf("xwliu gc9503v_cmi,lk lcd_id=0x%x\n",lcd_id);
#else
	printk("xwliu gc9503v_cmi,kernel lcd_id=0x%x\n",lcd_id);
#endif

	gpio_id0 = lcm_read_adc_value();

	return ((LCM_ID==lcd_id)&& (gpio_id0 == 1))?1:0;
}


LCM_DRIVER gc9503v_fwvga_dsi_vdo_boe_weihui_lcm_drv = {
	.name		= "gc9503v_fwvga_dsi_vdo_boe_weihui",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
};

