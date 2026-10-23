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
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
//#define GC9503_TEST

#define FRAME_WIDTH  										(442)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER

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

//extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
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

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = {
		
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	
	{0xF6,2,{0x5A,0x87}},
	
	{0xC1,1,{0x3F}},
	
	{0xC2,1,{0x0E}},
	
	{0xC6,1,{0xF8}},
	
	{0xCD,1,{0x25}},
	
	{0xC9,1,{0x10}},
	
	{0xF8,1,{0x8A}},
	
	{0xAC,1,{0x65}},
	
	{0xA7,1,{0x47}},
	
	{0xA0,1,{0xdd}},
	
	{0x86,4,{0x99,0xA3,0xA3,0x51}},
	
	{0x87,3,{0x04,0x03,0x66}},
	
	{0xFA,4,{0x08,0x08,0x00,0x04}},
	
	{0x71,1,{0x48}},
	
	{0x72,1,{0x48}},	
	
	{0xA3,1,{0xa2}},
	
	{0xFD,3,{0x38,0x38,0x00}},
	
	{0x97,1,{0xee}},
	
	{0x83,1,{0x93}},
	

	{0x9A,1,{0x4e}},	//0x4e  0x51
	
	{0x9B,1,{0x22}},	//0x22  0x51
	
	{0x82,2,{0x00,0x00}},	//0x00 0x00   //0x26 0x26
	
	{0x80,1,{0x28}},
	
	{0xB1,1,{0x10}},
	
	{0x7A,2,{0x0F,0x13}},
	
	{0x7B,2,{0x0F,0x13}},
	
	{0x6D,32,{0x1E,0x1E,0x1E,0x07,0x05,0x0D,0x0B,0x0F,0x09,0x01,0x03,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x04,0x02,0x0A,0x10,0x0C,0x0E,0x06,0x08,0x1E,0x1E,0x1E}},
	
	{0x64,16,{0x28,0x06,0x03,0xC2,0x03,0x03,0x28,0x05,0x03,0xC3,0x03,0x03,0x79,0x79,0x79,0x79}},
	{0x65,16,{0x28,0x02,0x03,0xC6,0x03,0x03,0x28,0x01,0x03,0xC7,0x03,0x03,0x79,0x79,0x79,0x79}},
	{0x66,16,{0x20,0x00,0x03,0xC8,0x03,0x03,0x20,0x01,0x03,0xC9,0x03,0x03,0x79,0x79,0x79,0x79}},
	{0x67,16,{0x28,0x04,0x03,0xC4,0x03,0x03,0x28,0x03,0x03,0xC5,0x03,0x03,0x79,0x79,0x79,0x79}},
	{0x60,8,{0x38,0x09,0x79,0x79,0x38,0x08,0x79,0x79}},
	
	{0x61,8,{0x38,0x07,0x79,0x79,0x38,0x06,0x79,0x79}},
	
	{0x62,8,{0x33,0xBB,0x79,0x79,0x33,0xBC,0x79,0x79}},
	
	{0x63,8,{0x33,0xBD,0x79,0x79,0x33,0xBE,0x79,0x79}},
	
	{0x69,7,{0x12,0x22,0x12,0x22,0x44,0x22,0x08}},
	
	{0x6B,1,{0x07}},
	
	{0xD1,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},
	
	{0xD2,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},
	
	{0xD3,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},
	{0xD4,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},
	{0xD5,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},
	{0xD6,52,{0x00,0x00,0x00,0x1d,0x00,0x38,0x00,0x4e,0x00,0x61,0x00,0x86,0x00,0xA2,0x00,0xd4,0x00,0xf9,0x01,0x3e,0x01,0x74,0x01,0xC6,0x02,0x07,0x02,0x09,0x02,0x48,0x02,0x94,0x02,0xc7,0x02,0xF9,0x03,0x23,0x03,0x5a,0x03,0x83,0x03,0xbc,0x03,0xca,0x03,0xee,0x03,0xf0,0x03,0xFF}},

//	{0x36,1,{0x02}},
	{0x11,1,{0x00}},  // Sleep-Out
	{REGFLAG_DELAY, 100, {0}},
    {REGFLAG_DELAY, 100, {0}},
	{0x29,1,{0x00}},  // Display On
	{REGFLAG_DELAY, 10, {0}},	 
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};


#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 200, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
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
    {REGFLAG_DELAY, 120, {}},
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xC2,1,{0xcE}},
	{0xc3, 1,{0xcd}},
	{0xc6, 1,{0xfc}},
	{0xC5,1,{0x03}},
	{0xCD,1,{0x64}},
	{0xc4, 1,{0xff}},
	{0xC9,1,{0xcd}},
	{0xF6,2,{0x5A,0x87}},
	{0xFD,3,{0xaa,0xaa,0x0a}},
	{0xFe,2,{0x6a,0x0a}},
	{0x78,2,{0x2a,0xaa}},
	{0x92,2,{0x17,0x08}},
	{0x77,2,{0xaa,0x2a}},
	{0x76,2,{0xaa,0xaa}},
	{0x84, 1,{0x00}},
	{0x78,2,{0x2b,0xba}},
	{0x89, 1,{0x73}},
	{0x88, 1,{0x3a}},
	{0x85, 1,{0xb0}},
	{0x76,2,{0xeb,0xaa}},
	{0x94, 1,{0x80}},
	{0x87,3,{0x04,0x07,0x30}},
	{0x93, 1,{0x27}},
	{0xAf, 1,{0x02}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}	
	
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned int cmd;
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

#if 0
static void init_lcm_registers(void)
{	

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);


}
#endif


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}
#if 0
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
	printf("lfz gc9503v_hhx test adc_vol is %d\n",adc_vol);
#else
	printk("lfz gc9503v_hhx test adc_vol is %d\n",adc_vol);
#endif

	if(adc_vol < 30)
	{       	
		printf("lzy adc_vol < 30 \n");
		return 0;
	}
	else if((adc_vol >= 30) && (adc_vol < 60))
	{
		printf("lzy 30< adc_vol <= 50 \n");
		return 1;
	}
	else if((adc_vol >= 60) && (adc_vol < 100))
	{
		printf("lzy 50< adc_vol <= 100 \n");
		return 2;
	}
	else if((adc_vol >= 100) && (adc_vol < 130))
	{
		printf("lzy 100< adc_vol <= 130 \n");
		return 3;
	}
	else
	{
		printf("lzy adc_vol > 130 \n");
		return 4;
	}
}
#endif
static void lcm_get_params(LCM_PARAMS *params)
{
	//	unsigned int div2_real=0;
		//unsigned int cycle_time = 0;
	//	unsigned int ui = 0;
	//	unsigned int hs_trail_m, hs_trail_n;
		#define NS_TO_CYCLE(n, c)	((n) / c + (( (n) % c) ? 1 : 0))
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
 	#if (LCM_DSI_CMD_MODE)
		params->dbi.te_mode 			= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_FALLING;
 	#endif

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif
	
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
//		params->dsi.word_count=480*3;

	    params->dsi.vertical_sync_active				= 2;
	    params->dsi.vertical_backporch					= 15;
	    params->dsi.vertical_frontporch 				= 8;
	    params->dsi.vertical_active_line				= FRAME_HEIGHT;

	    params->dsi.horizontal_sync_active				= 10;
	    params->dsi.horizontal_backporch				= 30;
	    params->dsi.horizontal_frontporch				= 30;
	    params->dsi.horizontal_active_pixel 			= FRAME_WIDTH; 
	    params->dsi.PLL_CLOCK = 190; 

//////ESD  SETTING///
		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd			= 0x0A;
		params->dsi.lcm_esd_check_table[0].count		= 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		
//		params->dsi.lcm_esd_check_table[1].cmd = 0x0b;
//		params->dsi.lcm_esd_check_table[1].count = 1;
//		params->dsi.lcm_esd_check_table[1].para_list[0] = 0x00;

}

#if defined(GC9503_TEST)	
	unsigned char para_9a[1]={0xdc}; ///c0     dc   +3c
	unsigned char para_9b[1]={0xcf};///b3            cf
	unsigned char para_82[2]={0x8c,0x8c};///70    8c
#endif 

static void lcm_init(void)
{ 
//	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
//	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
//	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
    SET_RESET_PIN(1); 
    MDELAY(25); 
    SET_RESET_PIN(0); 
    MDELAY(100); 
    SET_RESET_PIN(1); 
    MDELAY(120); 
	
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

#if defined(GC9503_TEST)	
	para_9a[0]+=2;
	para_9b[0]-=2;
	
	para_82[0]+=2;
	para_82[1]+=2;
		
	dsi_set_cmdq_V2(0x9a, 1, para_9a, 1);
	dsi_set_cmdq_V2(0x9b, 1, para_9b, 1);
	dsi_set_cmdq_V2(0x82, 2, para_82, 1);
#endif
} 


static void lcm_suspend(void)
{

    push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	
	SET_RESET_PIN(1);
	MDELAY(100);
	
//	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
//	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
//	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ZERO);
	
}

static void lcm_resume(void)
{
    #ifdef BUILD_LK
		printf("%s, LK gc9503v debug \n", __func__);
    #else
		printk("%s, kernel gc9503v debug \n", __func__);
    #endif
	//lcm_initialization_setting[23].para_list[0]+=2;
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{

	int id=0;
	unsigned char buffer[4];
	unsigned int array[16];  
	char id_high=0;
	char id_low=0;
//	int id_adc = 0;
		
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);
	
	
	array[0] = 0x00033700; //
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x04, &buffer[0], 3);
	
	id_high=buffer[1];
	id_low=buffer[2];
	id=(id_high<<8)| id_low;
//	id_adc = adc_read_vol();
	
	//printf("gc9503 _fwvga_dsi_vdo_lcm_drv %s:0x%2x,0x%2x,0x%2x,0x%2x id=0x%x\n", __func__,buffer[0],buffer[1],buffer[2],buffer[3], id);
	//return 1;
	return (id == 0x9504 /*&& id_adc == 4*/) ? 1 : 0;
	//return 1;
}


LCM_DRIVER gc9503v_qhdminus_vdo_ivo_hanlong_lcm_drv = 
{
    .name			= "gc9503v_qhdminus_vdo_ivo_hanlong",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,	
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};


