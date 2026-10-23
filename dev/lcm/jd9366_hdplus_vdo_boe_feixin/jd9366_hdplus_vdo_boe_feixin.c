#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/wait.h>
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  										(800)
#define FRAME_HEIGHT 										(1280)

#define   LCM_DSI_CMD_MODE							0

#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFFFD   // END OF REGISTERS MARKER

// static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define SET_3V3_PIN(v)	(lcm_util.set_gpio_lcd_enp_bias((v)))
#define SET_1V8_PIN(v)	(lcm_util.set_gpio_lcd_18v_bias((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V3(para_tbl,size,force_update)        lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[100];
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


static struct LCM_setting_table lcm_initialization_setting[] = {
	
{0xE0,2,{0xAB,0xBA}},
{0xE1,2,{0xBA,0xAB}},
{0xB0,1,{0x00}},
{0xB1,4,{0x10,0x01,0x47,0xFF}},
{0xB2,6,{0x0c,0x14,0x04,0x50,0x50,0x14}},
{0xB3,3,{0x56,0xd3,0x00}},
{0xB4,3,{0x42,0x30,0x04}},	/*0x60*/
{0xB5,1,{0x00}},
{0xB6,7,{0xb0,0x00,0x00,0x10,0x00,0x10,0x00}},
{0xB7,8,{0x0e,0x00,0xff,0x08,0x08,0xff,0xff,0x00}},
{0xB8,7,{0x16,0x12,0x29,0x49,0x48,0x00,0x00}},
{0xB9,38,{0x7a,0x65,0x55,0x49,0x46,0x38,0x3e,0x2b,0x47,0x48,0x49,0x66,0x51,0x55,0x44,0x41,0x32,0x20,0x06,0x7a,0x65,0x55,0x49,0x46,0x38,0x3e,0x2b,0x47,0x48,0x49,0x66,0x51,0x55,0x44,0x41,0x32,0x20,0x06}},
{0xBA,8,{0x00,0x00,0x00,0x44,0x24,0x00,0x00,0x00}},
{0xBB,3,{0x76,0x00,0x00}},
{0xBC,2,{0x00,0x00}},
{0xBD,5,{0xff,0x00,0x00,0x00,0x00}},
{0xBE,1,{0x00}},
{0xC0,16,{0x32,0x89,0x12,0x34,0xdd,0x77,0x44,0x44,0x10,0x04,0x90,0x04,0x3f,0x00,0x00,0x00}},
{0xC1,10,{0x97,0xd4,0x02,0x91,0x10,0x04,0x90,0x04,0x54,0x01}},
{0xC2,12,{0x3f,0x10,0x89,0x11,0x1f,0x11,0x22,0x33,0x44,0x82,0x18,0x03}},
{0xC3,22,{0xa4,0x64,0x05,0x13,0x13,0x1b,0x1b,0x11,0x11,0x19,0x19,0x0f,0x0f,0x17,0x17,0x0d,0x0D,0x15,0x15,0x23,0x22,0x07}},
{0xC4,22,{0x24,0x24,0x04,0x12,0x12,0x1a,0x1a,0x10,0x10,0x18,0x18,0x0e,0x0e,0x16,0x16,0x0c,0x0c,0x14,0x14,0x23,0x22,0x06}},
{0xC6,2,{0x3f,0x3f}},
{0xC7,22,{0x41,0x01,0x0d,0x11,0x09,0x15,0x19,0x4f,0x10,0xd7,0xcf,0x19,0x1b,0x1d,0x03,0x02,0x25,0x30,0x00,0x03,0xff,0x00}},
{0xC8,6,{0x62,0x00,0x32,0x40,0x54,0x16}},
{0xC9,5,{0xa1,0x22,0xff,0xc4,0x23}},
{0xCA,2,{0xCb,0x43}},
{0xCC,4,{0x2e,0x02,0x04,0x08}},
{0xCD,8,{0x0e,0x64,0x64,0x20,0x23,0x6b,0x06,0x83}},
{0xD0,3,{0x07,0x10,0x80}},
{0xD1,4,{0x00,0x0d,0xff,0x0f}},
{0xD2,4,{0xe3,0x2b,0x38,0x00}},
{0xD4,11,{0x00,0x01,0x00,0x0e,0x04,0x44,0x08,0x10,0x00,0x27,0x20}},
{0xD5,1,{0x00}},
{0xD6,2,{0x00,0x00}},
{0xD7,4,{0x00,0x00,0x00,0x00}},
{0xE3,2,{0x20,0x21}},
{0xE4,3,{0x08,0x55,0x03}},
{0xE6,8,{0x00,0x01,0xff,0xff,0xff,0xff,0xff,0xff}},
{0xE7,3,{0x00,0x00,0x00}},
{0xE8,7,{0xd5,0xff,0xff,0xff,0x00,0x00,0x00}},
{0xE9,1,{0xff}},
{0xF0,5,{0x12,0x03,0x20,0x00,0xff}},
{0xF1,26,{0xa6,0xc8,0xea,0xe6,0xe4,0xcc,0xe4,0xbe,0xf0,0xb2,0xaa,0xc7,0xff,0x66,0x98,0xe3,0x87,0xc8,0x99,0xc8,0x8c,0xbe,0x96,0x91,0x8f,0xff}},
{0xF3,1,{0x03}},
{0xF4,26,{0xff,0xfe,0xfc,0xfa,0xf8,0xf4,0xf0,0xe8,0xe0,0xd0,0xc0,0xa0,0x80,0x7f,0x5f,0x3f,0x2f,0x1f,0x17,0x0f,0x0b,0x07,0x05,0x03,0x01,0x00}},
{0xF5,26,{0xff,0xfe,0xfc,0xfa,0xf8,0xf4,0xf0,0xe8,0xe0,0xd0,0xc0,0xa0,0x80,0x7f,0x5f,0x3f,0x2f,0x1f,0x17,0x0f,0x0b,0x07,0x05,0x03,0x01,0x00}},
{0xF6,26,{0xff,0xfe,0xfc,0xfa,0xf8,0xf4,0xf0,0xe8,0xe0,0xd0,0xc0,0xa0,0x80,0x7f,0x5f,0x3f,0x2f,0x1f,0x17,0x0f,0x0b,0x07,0x05,0x03,0x01,0x00}},
{0xF7,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xF8,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xF9,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xFA,25,{0x00,0x84,0x12,0x21,0x48,0x48,0x21,0x12,0x84,0x69,0x69,0x5a,0xa5,0x96,0x96,0xa5,0x5a,0xb7,0xde,0xed,0x7b,0x7b,0xed,0xde,0xb7}},
{0xFB,23,{0x00,0x12,0x0f,0xff,0xff,0xff,0x00,0x38,0x40,0x08,0x70,0x0b,0x40,0x19,0x50,0x21,0xc0,0x27,0x60,0x2d,0x00,0x00,0x0f}},


	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 120,{}},
	
	{0x29,1,{0x00}},	
	{REGFLAG_DELAY, 50,{}},
//	{REGFLAG_END_OF_TABLE, 0x00, {} }	

};

static void init_lcm_registers(void)
{
   	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}
/*
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},

    // Sleep Mode On
	{0x10, 0, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

*/



// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{	
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		//params->physical_width = 71;
		//params->physical_height = 129;
		
        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		//params->dsi.word_count=720*3;	
		
		
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 12;
		params->dsi.vertical_frontporch					= 20;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 20;
		params->dsi.horizontal_backporch				= 80;
		params->dsi.horizontal_frontporch				= 80;

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		// params->dsi.compatibility_for_nvk = 0;
        params->dsi.ssc_disable = 1;

		params->dsi.esd_check_enable = 0;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
		params->dsi.lcm_esd_check_table[0].count        = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		
		//params->dsi.HS_TRAIL=20;	
        params->dsi.PLL_CLOCK = 237;


}


static unsigned int lcm_compare_id(void);

static void lcm_suspend(void)
{
	unsigned int data_array[16];

    data_array[0] = 0x00280500;
    dsi_set_cmdq(data_array, 1, 1);
	MDELAY(50);
		
    data_array[0]= 0x00100500;
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(120);
//	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);	

}


static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[3];
	unsigned int array[16]; 
	char  id_high=0;
	char  id_low=0;
	
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	
	SET_RESET_PIN(1);
	MDELAY(5); 
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);
		
	array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x04, buffer, 3);
	
	id_high = buffer[0]; 
	id_low = buffer[1];
	id = (id_high<<8)|id_low;
	
	#ifdef BUILD_LK
		printf("%s, LK   jd9366 debug:  jd9366 id = 0x%08x buffer[0]=0x%08x,buffer[1]=0x%08x,buffer[2]=0x%08x\n", __func__, id,buffer[0],buffer[1],buffer[2]);
	#else
		printk("%s, kernel   jd9366 debug:  jd9366 id = 0x%08x buffer[0]=0x%08x,buffer[1]=0x%08x,buffer[2]=0x%08x\n", __func__, id,buffer[0],buffer[1],buffer[2]);
	#endif
	return (id == 0x9366)?1:0;
}

static void lcm_init(void)
{
	mt_set_gpio_mode(GPIO_LCM_PWR2_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR2_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);	
		
    SET_RESET_PIN(1);
    MDELAY(10); //20130924  add 
    SET_RESET_PIN(0);
    MDELAY(50);//Must > 5ms   //6 20130924  changed
    SET_RESET_PIN(1);
    MDELAY(100);//Must > 50ms	//50 20130924  changed
   
   init_lcm_registers();
//	lcm_compare_id();
}


static void lcm_resume(void)
{
	lcm_init();
}

LCM_DRIVER jd9366_hdplus_vdo_boe_feixin_lcm_drv = 
{
    .name			= "jd9366_hdplus_vdo_boe_feixin",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
};

