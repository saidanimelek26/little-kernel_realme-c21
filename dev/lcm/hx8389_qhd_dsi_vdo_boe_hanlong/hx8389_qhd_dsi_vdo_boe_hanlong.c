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
#define FRAME_WIDTH             (540)
#define FRAME_HEIGHT            (960)

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

#define   LCM_DSI_CMD_MODE							(0)

static struct LCM_setting_table
{
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[128];
};

						
static struct LCM_setting_table lcm_initialization_setting[] = {

{0xB9,3,{0xFF,0x83,0x89}}, 
 //Set_power
{0xB1,20,{0x7F,0x14,0x14,0x32,0x92,0x90,0x10,0xFA,0x58,0x80,0x20,0x20,0xF8,0xAA,0xAA,0xA3,0x00,0x80,0x30,0x00}}, 
 
{0xB2,10,{0x80,0x50,0x0A,0x06,0x40,0x38,0x11,0x64,0x55,0x09}},  
{0xB4,11,{0x02,0x70,0x02,0x70,0x00,0x00,0x10,0x76,0x10,0x76,0x90}},  
{0xD3,35,{0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x32,0x10,0x08,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x17,0x00,0x09,0x09,0x13,0x00,0x00,0x00,0x0C,0x08,0x00,0x00,0x0A,0x00,0x01}},  
{0xD5,38,{0x18,0x18,0x18,0x18,0x18,0x18,0x01,0x00,0x03,0x02,0x19,0x19,0x19,0x19,0x21,0x20,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},  
{0xD6,38,{0x18,0x18,0x18,0x18,0x18,0x18,0x02,0x03,0x00,0x01,0x19,0x19,0x18,0x18,0x20,0x21,0x19,0x19,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},  
{0xC0,4,{0x30,0x15,0x00,0x03}},  
{0xC7,4,{0x00,0x80,0x00,0xC0}},  
{0xD2,1,{0x22}},  
{0xCC,1,{0x02}},  
{0xE0,42,{0x00,0x02,0x05,0x0F,0x10,0x1F,0x17,0x31,0x06,0x08,0x0A,0x14,0x10,0x15,0x17,0x16,0x16,0x09,0x13,0x14,0x17,0x00,0x02,0x05,0x0F,0x10,0x1F,0x17,0x31,0x06,0x08,0x0A,0x14,0x10,0x15,0x17,0x16,0x16,0x09,0x13,0x14,0x17}},  
{REGFLAG_DELAY, 5, {}}, 

{0xB6,2,{0x42,0x42}},  // vcom

{0xCD,1,{0x5A}},  
{0x11,1,{0x00}},             
{REGFLAG_DELAY, 120,{}},                                
{0x29,1,{0x00}},	           
{REGFLAG_DELAY, 50,{}},                                 
{0x35,1,{0x00}},             
{REGFLAG_DELAY, 100, {}},	   

};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
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
	params->dsi.mode = SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE; SYNC_PULSE_VDO_MODE SYNC_EVENT_VDO_MODE

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size=248;

	params->dsi.ssc_disable	= 1;
	params->dsi.cont_clock= 0;		
	params->dsi.clk_lp_per_line_enable = 1;	

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 12;
	params->dsi.vertical_frontporch = 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 44;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;	

	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;

	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;		

    params->dsi.PLL_CLOCK = 208;//220;//dsi clock customization: should config clock value directly
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
	return 1;
#if 0
	unsigned int id=0,id1=0,id2=0;
	unsigned char buffer[5];
	unsigned int array[3]; 
	int id_adc = 0;
	u8 hw_id = 0;

	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);	

#ifdef BUILD_LK
	DSI_clk_HS_mode(0);
#endif
//	array[0]=0x00063902;
//	array[1]=0x000177FF;
//	array[2]=0x00001000;
//	dsi_set_cmdq(array, 3, 1);
//	MDELAY(10); 

	array[0] = 0x00053700;// set return byte number
//	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1,  buffer, 5);
//	read_reg_v2(0xDA,  &buffer, 1);	
//	id = buffer[0];
	id = buffer[0]<<8 |buffer[1];
	id1 = buffer[2]<<8 |buffer[3];
	id2 = buffer[4];

	id_adc = adc_read_vol();

	mt_set_gpio_mode(GPIO_DISP_ID0_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_DISP_ID0_PIN, GPIO_DIR_IN);
	hw_id = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
	printf("yxch longli read id pin value = %d\n",hw_id);

#if defined(BUILD_LK)
	printf("lfz lk -- st7701s_fwvga_dsi_vdo_xinqi_hsd_4502 id_adc==1 0x%x , 0x%x , 0x%x \n",id,id1,id2);
#else
	printk("lfz kernel -- st7701s_fwvga_dsi_vdo_xinqi_hsd_4502 id_adc==1 0x%x , 0x%x , 0x%x \n",buffer[0],buffer[1],id);
#endif

	return ((id == 0x8802) && (hw_id == 1))?1:0;
#endif
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
	//printk("lcm_resume g_which_lcm=%d\n", g_which_lcm);	
	lcm_init();
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hx8389_qhd_dsi_vdo_boe_hanlong = {
	.name			= "hx8389_qhd_dsi_vdo_boe_hanlong",
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
