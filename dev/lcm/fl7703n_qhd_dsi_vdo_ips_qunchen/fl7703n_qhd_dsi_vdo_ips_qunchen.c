#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
    #include <platform/mt_gpio.h>
#else
    #include <mt-plat/mt_gpio.h>
	#include "../../../../misc/mediatek/auxadc/mt_auxadc.h"
#endif

// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------

#define AUXADC_LCM_VOLTAGE_CHANNEL     1
#define FRAME_WIDTH             (600)
#define FRAME_HEIGHT            (1280)

#define REGFLAG_DELAY             							0XFFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;
//static int g_which_lcm = -1;
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
//static unsigned int lcm_compare_id(void);
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


static struct LCM_setting_table
{
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};

						
static struct LCM_setting_table lcm_initialization_setting[] = {
{0xB9,3,{0xF1,0x12,0x87}},
{0xB2,3,{0x40,0x15,0x78}},
{0xB3,10,{0x10,0x10,0x28,0x28,0x03,0xFF,0x00,0x00,0x00,0x00}},
{0xB4,1,{0x80}},
{0xB5,2,{0x0A,0x0A}},
{0xB6,2,{0x0E,0x0E}},
{0xB8,4,{0x28,0x22,0xF0,0x63}},
{0xBA,27,{0x33,0x81,0x05,0xF9,0x0E,0x0E,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x25,0x00,0x91,0x0A,0x00,0x00,0x01,0x4F,0x01,0x00,0x00,0x37}},
{0xBC,1,{0x47}},
{0xBF,3,{0x02,0x11,0x00}},
{0xC0,9,{0x73,0x73,0x50,0x50,0xA0,0x00,0x12,0x53,0x00}},
{0xC1,17,{0x54,0x80,0x32,0x32,0x77,0xF4,0x77,0x77,0xCC,0xCC,0xFF,0xFF,0x11,0x11,0x00,0x00,0x32}},
{0xC7,12,{0x10,0x00,0x0A,0x00,0x00,0x00,0x00,0x00,0xED,0xC7,0x00,0xA5}},
{0xC8,4,{0x10,0x40,0x1E,0x03}},
{0xCC,1,{0x0B}},
{0xE0,34,{0x00,0x0A,0x10,0x2A,0x3D,0x3F,0x48,0x39,0x07,0x0D,0x0D,0x12,0x14,0x11,0x13,0x12,0x19,0x00,0x0A,0x10,0x2A,0x3D,0x3F,0x48,0x39,0x07,0x0D,0x0D,0x12,0x14,0x11,0x13,0x12,0x19}},
{0xE1,7,{0x11,0x11,0x91,0x00,0x00,0x00,0x00}},
{0xE3,14,{0x03,0x03,0x03,0x03,0x00,0x03,0x00,0x00,0x00,0x00,0xFF,0x84,0xC0,0x10}},
{0xE9,63,{0xC8,0x10,0x06,0x05,0x1,0x80,0x81,0x12,0x31,0x23,0x47,0x83,0x80,0x81,0x37,0x08,0x0C,0x00,0x18,0x00,0x00,0x00,0x0C,0x00,0x18,0x00,0x00,0x00,0x88,0x87,0x57,0x53,0x18,0x88,0x88,0x88,0x88,0x13,0x88,0x88,0x86,0x46,0x42,0x08,0x88,0x88,0x88,0x88,0x02,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xEA,61,{0x02,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x80,0x24,0x60,0x28,0x88,0x88,0x88,0x88,0x64,0x88,0x88,0x81,0x35,0x71,0x68,0x88,0x88,0x88,0x88,0x75,0x88,0x23,0x30,0x00,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x80,0x81,0x00,0x00,0x00,0x00}},
{0xEF,3,{0xFF,0xFF,0x01}},
	
{0x11,1,{0x00}},             
	{REGFLAG_DELAY, 100, {}},
{0x29,1,{0x00}},
//{0x35,1,{0x00}}, 	           
{REGFLAG_DELAY, 100,{}},                                 

};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Sleep Mode On
	{ 0x10, 0, {} },
	{ REGFLAG_DELAY, 120, {} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
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

static int adc_read_vol(void)
{
	int adc[1];
	int data[4] ={0,0,0,0};
	int sum = 0;
	int adc_vol=0;
	int num = 0;
	int ret =0;

	for(num=0;num<10;num++)
	{
		ret = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL, data, adc);
		sum+=(data[0]*100+data[1]);
	}
	adc_vol = sum/10;
#if defined(BUILD_LK)
	printf("lfz st7701s test adc_vol is %d\n",adc_vol);
#else
	printk("lfz st7701s test adc_vol is %d\n",adc_vol);
#endif
	
	if(adc_vol < 60)
	{       	
		printf("lfz st7701s_fwvga_dsi_vdo_jh_hsd_4502 \n");//38
		return 0;
	}
	else
	{
		printf("lfz st7701s_fwvga_dsi_vdo_xinqi_hsd_4502 \n");//115
		return 1;
	}
}

static void lcm_get_params(LCM_PARAMS * params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type  = LCM_TYPE_DSI;
	params->physical_width = 61;
    params->physical_height = 109;

	params->width = FRAME_WIDTH;
	params->height= FRAME_HEIGHT;

	// enable tearing-free
	//params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	//params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
	params->dsi.mode = SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; SYNC_PULSE_VDO_MODE SYNC_EVENT_VDO_MODE

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=256;

	params->dsi.ssc_disable	= 1;
	params->dsi.cont_clock= 0;		
	params->dsi.clk_lp_per_line_enable = 1;	

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 11;
	params->dsi.vertical_frontporch = 14;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 40;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;	

	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;

	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;		

    params->dsi.PLL_CLOCK = 212;//220;//dsi clock customization: should config clock value directly
}



static void lcm_suspend(void)
{
//	lcm_compare_id();
//	printk("lcm_suspend g_which_lcm=%d\n", g_which_lcm);
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
//	SET_RESET_PIN(1);	
//	MDELAY(10);	
//	SET_RESET_PIN(0);
}


static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
    unsigned char buffer[5]={0};
    unsigned int array[2]={0};  

    SET_RESET_PIN(1);
	MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

	array[0] = 0x00033700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x04, buffer, 3); 

	printf("fl7703n lcm_compare_id buffer:%d %d %d\n",buffer[0],buffer[1],buffer[2]);
	
	if((buffer[0] == 56) && (buffer[1] == 33))
		return 1;
	else
		return 0;

 //   return (id==LCM_ID_OTM8019A)?1:0;
		
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(100);
	printf("fl7703n_qhd_dsi_vdo_ips_qunchen lcm_init\n");
	init_lcm_registers();
}
static void lcm_resume(void)
{
	//printk("lcm_resume g_which_lcm=%d\n", g_which_lcm);	
	lcm_init();
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER fl7703n_qhd_dsi_vdo_ips_qunchen_lcm_drv = {
	.name			= "fl7703n_qhd_dsi_vdo_ips_qunchen",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params		= lcm_get_params,
	.init			= lcm_init,
	.suspend		= lcm_suspend,
	.resume			= lcm_resume,
	.compare_id		= lcm_compare_id,
};
