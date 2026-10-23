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
	
{0xE0,1,{0x00}},
{0xE1,1,{0x93}},
{0xE2,1,{0x65}},
{0xE3,1,{0xF8}},
{0x80,1,{0x03}},

{0xE0,1,{0x01}},
{0x00,1,{0x00}},
{0x01,1,{0x67}},
{0x03,1,{0x00}},
{0x04,1,{0x65}},

{0x0C,1,{0x74}},

{0x17,1,{0x00}},
{0x18,1,{0xBF}},
{0x19,1,{0x00}},
{0x1A,1,{0x00}},
{0x1B,1,{0xBF}},
{0x1C,1,{0x00}},

{0x24,1,{0xFE}},

{0x37,1,{0x19}},

{0x38,1,{0x05}},
{0x39,1,{0x00}},
{0x3A,1,{0x01}},
{0x3B,1,{0x01}},
{0x3C,1,{0x70}},
{0x3D,1,{0xFF}},
{0x3E,1,{0xFF}},
{0x3F,1,{0xFF}},

{0x40,1,{0x06}},
{0x41,1,{0xA0}},
{0x43,1,{0x1E}},
{0x44,1,{0x0F}},
{0x45,1,{0x28}},
{0x4B,1,{0x04}},

{0x55,1,{0x02}},
{0x56,1,{0x01}},
{0x57,1,{0xA9}},
{0x58,1,{0x0A}},
{0x59,1,{0x0A}},
{0x5A,1,{0x37}},
{0x5B,1,{0x19}},

{0x5D,1,{0x7E}},
{0x5E,1,{0x65}},
{0x5F,1,{0x53}},
{0x60,1,{0x47}},
{0x61,1,{0x43}},
{0x62,1,{0x34}},
{0x63,1,{0x38}},
{0x64,1,{0x23}},
{0x65,1,{0x3D}},
{0x66,1,{0x3C}},
{0x67,1,{0x3D}},
{0x68,1,{0x5D}},
{0x69,1,{0x4D}},
{0x6A,1,{0x56}},
{0x6B,1,{0x49}},
{0x6C,1,{0x44}},
{0x6D,1,{0x37}},
{0x6E,1,{0x23}},
{0x6F,1,{0x02}},
{0x70,1,{0x7E}},
{0x71,1,{0x65}},
{0x72,1,{0x53}},
{0x73,1,{0x47}},
{0x74,1,{0x43}},
{0x75,1,{0x34}},
{0x76,1,{0x38}},
{0x77,1,{0x23}},
{0x78,1,{0x3D}},
{0x79,1,{0x3C}},
{0x7A,1,{0x3D}},
{0x7B,1,{0x5D}},
{0x7C,1,{0x4D}},
{0x7D,1,{0x56}},
{0x7E,1,{0x49}},
{0x7F,1,{0x44}},
{0x80,1,{0x37}},
{0x81,1,{0x23}},
{0x82,1,{0x02}},

{0xE0,1,{0x02}},
{0x00,1,{0x47}},
{0x01,1,{0x47}},
{0x02,1,{0x45}},
{0x03,1,{0x45}},
{0x04,1,{0x4B}},
{0x05,1,{0x4B}},
{0x06,1,{0x49}},
{0x07,1,{0x49}},
{0x08,1,{0x41}},
{0x09,1,{0x1F}},
{0x0A,1,{0x1F}},
{0x0B,1,{0x1F}},
{0x0C,1,{0x1F}},
{0x0D,1,{0x1F}},
{0x0E,1,{0x1F}},
{0x0F,1,{0x5F}},
{0x10,1,{0x5F}},
{0x11,1,{0x57}},
{0x12,1,{0x77}},
{0x13,1,{0x35}},
{0x14,1,{0x1F}},
{0x15,1,{0x1F}},

{0x16,1,{0x46}},
{0x17,1,{0x46}},
{0x18,1,{0x44}},
{0x19,1,{0x44}},
{0x1A,1,{0x4A}},
{0x1B,1,{0x4A}},
{0x1C,1,{0x48}},
{0x1D,1,{0x48}},
{0x1E,1,{0x40}},
{0x1F,1,{0x1F}},
{0x20,1,{0x1F}},
{0x21,1,{0x1F}},
{0x22,1,{0x1F}},
{0x23,1,{0x1F}},
{0x24,1,{0x1F}},
{0x25,1,{0x5F}},
{0x26,1,{0x5F}},
{0x27,1,{0x57}},
{0x28,1,{0x77}},
{0x29,1,{0x35}},
{0x2A,1,{0x1F}},
{0x2B,1,{0x1F}},

{0x58,1,{0x40}},
{0x59,1,{0x00}},
{0x5A,1,{0x00}},
{0x5B,1,{0x10}},
{0x5C,1,{0x06}},
{0x5D,1,{0x40}},
{0x5E,1,{0x01}},
{0x5F,1,{0x02}},
{0x60,1,{0x30}},
{0x61,1,{0x01}},
{0x62,1,{0x02}},
{0x63,1,{0x03}},
{0x64,1,{0x6B}},
{0x65,1,{0x05}},
{0x66,1,{0x0C}},
{0x67,1,{0x73}},
{0x68,1,{0x09}},
{0x69,1,{0x03}},
{0x6A,1,{0x56}},
{0x6B,1,{0x08}},
{0x6C,1,{0x00}},
{0x6D,1,{0x04}},
{0x6E,1,{0x04}},
{0x6F,1,{0x88}},
{0x70,1,{0x00}},
{0x71,1,{0x00}},
{0x72,1,{0x06}},
{0x73,1,{0x7B}},
{0x74,1,{0x00}},
{0x75,1,{0xF8}},
{0x76,1,{0x00}},
{0x77,1,{0xD5}},
{0x78,1,{0x2E}},
{0x79,1,{0x12}},
{0x7A,1,{0x03}},
{0x7B,1,{0x00}},
{0x7C,1,{0x00}},
{0x7D,1,{0x03}},
{0x7E,1,{0x7B}},

{0xE0,1,{0x04}},
{0x00,1,{0x0E}},
{0x02,1,{0xB3}},
{0x09,1,{0x60}},
{0x0E,1,{0x2A}},
{0x36,1,{0x59}},

{0xE0,1,{0x00}},

	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 120,{}},
	
	{0x29,1,{0x00}},	
	{REGFLAG_DELAY, 50,{}},
{0x35,1,{0x00}},	
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
		params->dsi.vertical_frontporch					= 30;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 20;
		params->dsi.horizontal_backporch				= 20;
		params->dsi.horizontal_frontporch				= 40;

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		// params->dsi.compatibility_for_nvk = 0;
        params->dsi.ssc_disable = 1;

		params->dsi.esd_check_enable = 0;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
		params->dsi.lcm_esd_check_table[0].count        = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		
		//params->dsi.HS_TRAIL=20;	
        params->dsi.PLL_CLOCK = 211;


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
		printf("%s, LK   jd9365 debug:  jd9365 id = 0x%08x buffer[0]=0x%08x,buffer[1]=0x%08x,buffer[2]=0x%08x\n", __func__, id,buffer[0],buffer[1],buffer[2]);
	#else
		printk("%s, kernel   jd9365 debug:  jd9365 id = 0x%08x buffer[0]=0x%08x,buffer[1]=0x%08x,buffer[2]=0x%08x\n", __func__, id,buffer[0],buffer[1],buffer[2]);
	#endif
	return (id == 0x9365)?1:0;
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

LCM_DRIVER jd9365da_hdplus_vdo_boe_xinhangshen_lcm_drv = 
{
    .name			= "jd9365da_hdplus_vdo_boe_xinhangshen",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
};

