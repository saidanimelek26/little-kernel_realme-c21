#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include <platform/mt_gpio.h>
#include "lcm_drv.h"
#define LCM_ID_JD9161									0x9161

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif


// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


//static unsigned int lcm_esd_test = FALSE;      ///only for ESD test


//static kal_bool IsFirstBoot = KAL_TRUE;

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[120];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
{0xDF,3,{0x91,0x62,0xF3}},
//{0xB2,2,{0x00,0x92}},
//{0xB3,2,{0x00,0x92}},
{0xB7,6,{0x00,0xEc,0x2b,0x00,0x7b,0x21}},
{0xBB,11,{0x5A,0x1F,0xB2,0xB2,0xB2,0xC0,0xD0,0x30,0x50,0x50,0x50}},
{0xBC,2,{0x73,0x14}},
//{0xC0,1,{0x22}},
//{0xC1,1,{0x08}},
{0xC3,14,{0x74,0x04,0x08,0x0C,0x00,0x0C,0x0C,0x0C,0x0C,0x00,0x0C,0x82,0x08,0x82}},
{0xC4,5,{0x10,0xE0,0x92,0x0E,0x0C}},
{0xC8,38,{0x57,0x51,0x4B,0x47,0x4E,0x46,0x53,0x44,0x64,0x66,0x66,0x7F,0x6A,0x6B,0x59,0x50,0x41,0x2E,0x10,0x57,0x50,0x4B,0x46,0x4E,0x46,0x53,0x44,0x63,0x65,0x66,0x7F,0x6A,0x6B,0x59,0x50,0x41,0x2E,0x10}},
{0xD0,16,{0x1E,0x1E,0x04,0x06,0x00,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
{0xD1,16,{0x1E,0x1E,0x05,0x07,0x01,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
{0xD2,16,{0x1F,0x1E,0x07,0x05,0x01,0x1E,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
{0xD3,16,{0x1F,0x1E,0x06,0x04,0x00,0x1E,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
{0xD4,29,{0x10,0x00,0x00,0x03,0x60,0x08,0x10,0x00,0x00,0x06,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x0A,0x06,0x5C,0x08,0x25,0x00,0x63,0x01,0x00}},
{0xD5,40,{0x20,0x18,0x80,0x06,0x10,0x00,0x00,0x06,0x00,0x00,0x06,0x60,0x00,0x00,0x00,0x00,0x33,0x2F,0x00,0x60,0x1F,0x00,0x00,0x00,0x03,0x7B,0x06,0x10,0x00,0x00,0x0F,0x4F,0x00,0x10,0x1F,0x3F,0x00,0x00,0x00,0x00}},
{0xDE,1,{0x02}},
{0xC1,1,{0x71}},
//{0xC2,7,{0x00,0x18,0x08,0x1E,0x24,0x3C,0xC7}},
{0xDE,1,{0x00}},
//{0xBE,1,{0x4E}},
{0x35,0,{0x00}},

{0x11, 0, {0x00}},
{REGFLAG_DELAY, 120, {}},

{0x29, 0, {0x00}},
{REGFLAG_DELAY, 10, {}},

{REGFLAG_END_OF_TABLE, 0x00, {}}
};


#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
     {REGFLAG_DELAY, 20, {}},
    // Sleep Mode On
	{0x10, 0, {0x00}},
     {REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
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
				//MDELAY(10);//soso add or it will fail to send register
       	}
    }
	
}


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

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

		params->dsi.mode   = SYNC_EVENT_VDO_MODE;
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active=4;	//6;//4
		params->dsi.vertical_backporch=20;//10
		params->dsi.vertical_frontporch=20;	//10;
		params->dsi.vertical_active_line= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active=10;	//10;
		params->dsi.horizontal_backporch=30;	//30;
		params->dsi.horizontal_frontporch=30;	//30;
		params->dsi.horizontal_active_pixel= FRAME_WIDTH;

//    params->dsi.pll_div1=1;//1;		// div1=0,1,2,3;div1_real=1,2,4,4
//    params->dsi.pll_div2=0;//0;		// div2=0,1,2,3;div2_real=1,2,4,4
//    params->dsi.fbk_div =16;//17;		// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)		
       params->dsi.PLL_CLOCK=205;	//175;
}
//csh
//unsigned int    vcom_test = 0x005C00BE;

static void lcm_init(void)
{
    SET_RESET_PIN(1);
	MDELAY(50); //lankun add
    SET_RESET_PIN(0);
   // MDELAY(60);//Must > 5ms  //lankun close
    MDELAY(20);
    SET_RESET_PIN(1);
    //MDELAY(100);//Must > 50ms lankun close
    MDELAY(150);
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
//	lcm_compare_id();
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	    SET_RESET_PIN(1);
	MDELAY(10); //lankun add
    SET_RESET_PIN(0);
    MDELAY(50);//Must > 10ms
}

static void lcm_resume(void)
{
//	lcm_compare_id();
//vcom_test += 0x00020000;

//	lcm_initialization_setting[1].para_list[1]+=0x01;
	lcm_init();
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}
//#define XINBANGDA_JD9161Z_FWVGAP_DSI_VDO_BOE545_GPIO_ID
//#define PROJECT_LCM_GPIO_ID		124
static unsigned int lcm_compare_id(void)
{
		int array[4];
		char buffer[5];
		char id_high=0;
		char id_low=0;
		int id=0;
#ifdef XINBANGDA_JD9161Z_FWVGAP_DSI_VDO_BOE545_GPIO_ID
		unsigned int 	lcm_gpio_id = 0;
#endif	
		SET_RESET_PIN(1);
		MDELAY(10);
		  SET_RESET_PIN(0);
		MDELAY(20);
		  SET_RESET_PIN(1);
		MDELAY(120);
	
		array[0] = 0x00023700;// read id return two byte,version and id
		dsi_set_cmdq(array, 1, 1);
		read_reg_v2(0x04,buffer, 2);
	
		id_high = buffer[0]; 
		id_low = buffer[1];
		id = (id_high<<8)|id_low; 
		//printk("Linc JD9161 id = 0x%x \n", id); 
#ifdef XINBANGDA_JD9161Z_FWVGAP_DSI_VDO_BOE545_GPIO_ID
		mt_set_gpio_mode(PROJECT_LCM_GPIO_ID,GPIO_MODE_GPIO);
		mt_set_gpio_dir(PROJECT_LCM_GPIO_ID, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(PROJECT_LCM_GPIO_ID, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(PROJECT_LCM_GPIO_ID, GPIO_PULL_DOWN);
		MDELAY(150);
		lcm_gpio_id = mt_get_gpio_in(PROJECT_LCM_GPIO_ID);
		#if defined(BUILD_LK)
			printf("LK xingbangda_jd9161z_fwvgap_dsi_vdo_boe545.c lcm_compare_id : lcm_gpio_id = %d\n", lcm_gpio_id);
		#else
			printk("LK xingbangda_jd9161z_fwvgap_dsi_vdo_boe545.c lcm_compare_id : lcm_gpio_id = %d\n", lcm_gpio_id);
		#endif
		return ((id == LCM_ID_JD9161) && (lcm_gpio_id == 0))?1:0;
#else		 
		return (id == LCM_ID_JD9161) ?1:0;
#endif
}


#if 0
static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
	char  buffer[3];
	int   array[4];

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);
	if(buffer[0]==0x9C)
	{
		return FALSE;
	}
	else
	{			 
		return TRUE;
	}
 #endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	return TRUE;
}
#endif

LCM_DRIVER jd9161z_fwvga_vdo_boe_xingbangda =
{
    .name			= "jd9161z_fwvga_vdo_boe_xingbangda",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
//	.esd_check = lcm_esd_check,
//	.esd_recover = lcm_esd_recover,
};
