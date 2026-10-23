#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/wait.h>
#endif

#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  										(480)	//442
#define FRAME_HEIGHT 										(800)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFF   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

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
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)         


struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
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
{0xDF, 3, {0x91,0x62,0xF3}},
{0xB7, 6, {0x10,0x12,0x5C,0x00,0x63,0x35}},
{0xB9, 4, {0x43,0x03,0x00,0x77}},
{0xBB, 11, {0x56,0x19,0x32,0x32,0x32,0xC0,0xD0,0x30,0x50,0x50,0x60}},
{0xBC, 2, {0x73,0x14}},
{0xC0, 1, {0x26}},
{0xC3, 14, {0x74,0x04,0x04,0x08,0x00,0x08,0x04,0x08,0x08,0x00,0x08,0x82,0x04,0x82}},
{0xC4, 5, {0x10,0x90,0x92,0x0E,0x09}},
{0xC8, 38, {0x7F,0x70,0x61,0x57,0x53,0x45,0x4A,0x33,0x4B,0x47,0x44,0x60,0x4C,0x51,0x42,0x40,0x33,0x23,0x18,0x7F,0x70,0x61,0x57,0x53,0x45,0x4A,0x33,0x4B,0x47,0x44,0x60,0x4C,0x51,0x42,0x40,0x33,0x23,0x18}},
{0xD0, 16, {0x1F,0x1F,0x1F,0x0B,0x09,0x07,0x05,0x1F,0x01,0x1F,0x1E,0x03,0x1E,0x1F,0x1F,0x1F}},
{0xD1, 16, {0x1F,0x1F,0x1F,0x0A,0x08,0x06,0x04,0x1F,0x00,0x1F,0x1E,0x02,0x1E,0x1F,0x1F,0x1F}},
{0xD2, 16, {0x1F,0x1F,0x1F,0x04,0x06,0x08,0x0A,0x1F,0x02,0x1E,0x1F,0x00,0x1E,0x1F,0x1F,0x1F}},
{0xD3, 16, {0x1F,0x1F,0x1F,0x05,0x07,0x09,0x0B,0x1F,0x03,0x1E,0x1F,0x01,0x1E,0x1F,0x1F,0x1F}},
{0xD4, 29, {0x30,0x00,0x00,0x00,0x00,0x05,0x30,0x00,0x03,0x03,0x40,0x03,0x40,0x05,0x00,0x00,0x00,0xCC,0x2D,0x73,0x03,0x03,0x40,0x08,0x25,0x00,0x63,0x03,0x00}},
{0xD5, 36, {0x30,0x10,0x80,0x18,0x0C,0x00,0x00,0x18,0x00,0x00,0x06,0x60,0x00,0x07,0x50,0x00,0x33,0xC0,0x00,0x60,0xC0,0x00,0x00,0x00,0x03,0x60,0x06,0x10,0x00,0x00,0x0F,0x4F,0x00,0x10,0x1F,0x3F}},
{0xDE, 1, {0x02}},
{0xC1, 1, {0x60}},
{0xC2, 7, {0x00,0x18,0x08,0x1E,0x25,0x7C,0xC7}},
{0xDE, 1, {0x00}},
{0xBE, 1, {0x4E}},

{0x11, 0, {0x00}},
{REGFLAG_DELAY,120,{}},
{0x29, 0, {0x00}},
{REGFLAG_DELAY,5,{}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static void init_lcm_registers(void)
{
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);	
}

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},
   // Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else

	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM				= LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Video mode setting		
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 2; 
	params->dsi.vertical_backporch					= 9;
	params->dsi.vertical_frontporch					= 16;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 20;
	params->dsi.horizontal_backporch				= 20;
	params->dsi.horizontal_frontporch				= 20;
	
//	params->dsi.horizontal_blanking_pixel			= 60;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
	params->dsi.compatibility_for_nvk = 0;

	// ESD
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;


	params->dsi.PLL_CLOCK =161;	
}

static void lcm_suspend(void)
{
	SET_RESET_PIN(1);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(20);
	SET_RESET_PIN(1);
	MDELAY(20);
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
}
   
extern void DSI_clk_HS_mode(char enter);
static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];  

	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);	
					   
	array[0]=0x00043902;
	array[1]=0xF26191BF;
	dsi_set_cmdq(array,2,1);
	
	array[0] = 0x00023700;// set return byte number
	dsi_set_cmdq(array, 1, 1);
	MDELAY(10); 

	read_reg_v2(0x04, buffer, 2);    
	id = buffer[0]<<8 |buffer[1];

#if defined(BUILD_LK)
	printf("<<<< lk -- JD9161 0x%x , 0x%x , 0x%x ,0x%x \n",buffer[0],buffer[1],buffer[2],id);
#else
	printk("<<<< kernel -- JD9161 0x%x , 0x%x , 0x%x \n",buffer[0],buffer[1],id);
#endif

	return (id == 0x9161)?1:0;
} 

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(5);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);
	init_lcm_registers();
}

static void lcm_resume(void)
{
	lcm_init();
}


// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER jd9161z_wvga_vdo_boe_ririjia_lcm_drv = 
{
    .name = "jd9161z_wvga_vdo_boe_ririjia",
    .set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
};
