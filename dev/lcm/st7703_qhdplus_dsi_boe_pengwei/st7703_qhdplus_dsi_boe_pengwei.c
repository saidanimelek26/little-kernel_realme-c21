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
#define FRAME_WIDTH  (640)
#define FRAME_HEIGHT (960)

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
{0xB9, 3, {0xF1,0x12,0x83}},
{0xB2, 3, {0x78,0x23,0xF0}},
{0xB3, 10, {0x10,0x10,0x28,0x28,0x03,0xFF,0x00,0x00,0x00,0x00}},
{0xB4, 1, {0x80}},
{0xB5, 2, {0x0A,0x0A}}, 
{0xB6, 2, {0xBF,0xC9}},
{0xB8, 4, {0x25,0x22,0xF0,0x63}},
{0xBA, 27, {0x33,0x81,0x05,0xF9,0x0E,0x0E,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x25,0x00,0x90,0x0A,0x00,0x00,0x01,0x4F,0x01,0x00,0x00,0x37}},
{0xBC, 1, {0x46}},
{0xBF, 3, {0x02,0x11,0x00}},
{0xC0, 9, {0x73,0x73,0x50,0x50,0x00,0x00,0x08,0x70,0x00}},
{0xC1, 12, {0x65,0xC0,0x32,0x32,0x99,0xF1,0xCC,0xCC,0x77,0x77,0x33,0x33}},
{0xC6, 6, {0x82,0x00,0x3F,0xFF,0x00,0xE0}},
{0xCC, 1, {0x0B}},
{0xE0, 34, {0x00,0x29,0x2C,0x2F,0x3E,0x3F,0x53,0x3E,0x08,0x0D,0x0D,0x11,0x13,0x11,0x13,0x0E,0x18,0x00,0x29,0x2C,0x2F,0x3E,0x3F,0x53,0x3E,0x08,0x0D,0x0D,0x11,0x13,0x11,0x13,0x0E,0x18}},
{0xE3, 14, {0x07,0x07,0x0B,0x0B,0x03,0x0B,0x00,0x00,0x00,0x00,0xFF,0x00,0xC0,0x10}},
{0xE9, 63, {0xC1,0x10,0x0A,0x03,0xC4,0xA1,0x80,0x12,0x31,0x23,0x47,0x86,0xA1,0x80,0x47,0x08,0x30,0x00,0x80,0x00,0x00,0x00,0x30,0x00,0x80,0x00,0x00,0x00,0x88,0x8F,0xF8,0x84,0x44,0x66,0x00,0x22,0x88,0xFF,0x02,0x88,0x8F,0xF8,0x85,0x55,0x77,0x11,0x33,0x88,0xFF,0x13,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xEA, 61, {0x00,0x1A,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x8F,0xF8,0x88,0x85,0x33,0x11,0x77,0x55,0x88,0xFF,0x31,0x8F,0xF8,0x88,0x84,0x22,0x00,0x66,0x44,0x88,0xFF,0x20,0x23,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0xA1,0x80,0x00,0x00,0x00,0x00}},

	{0x11, 0, {} },
	{REGFLAG_DELAY, 250, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 50, {} },

};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
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
/*
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
	printf("lfz st7703 test adc_vol is %d\n",adc_vol);
#else
	printk("lfz st7703 test adc_vol is %d\n",adc_vol);
#endif

	if(adc_vol < 30)
	{       	
		printf("lfz st7703_fwvga_dsi_vdo_gt_ctc_h4504 \n");
		return 0;
	}
	else if((adc_vol >= 30) && (adc_vol < 50))
	{
		printf("lfz st7703_fwvga_dsi_vdo_zgd_hsd_h4504 \n");
		return 1;
	}
	else if((adc_vol >= 50) && (adc_vol < 100))
	{
		printf("lfz st7703_fwvga_dsi_vdo_gt_cmi_h4504 \n");
		return 2;
	}
	else if((adc_vol >= 100) && (adc_vol < 130))
	{
		printf("lfz st7703_fwvga_dsi_vdo_hy_cpt_h4504 \n");
		return 3;
	}
	else
	{
		printf("lfz st7703_fwvga_dsi_vdo_hy_cmi_h4504 \n");
		return 4;
	}
}
*/
static void lcm_get_params(LCM_PARAMS * params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type  = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height= FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
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

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
	params->dsi.vertical_active_line=FRAME_HEIGHT;
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 12;//8;
	params->dsi.vertical_frontporch = 16;//30;//8;
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 30;//8;
	params->dsi.horizontal_backporch = 130;//60;
	params->dsi.horizontal_frontporch = 130;//140;	
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;	

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;		

    params->dsi.PLL_CLOCK = 210;//220;//dsi clock customization: should config clock value directly
}



static void lcm_suspend(void)
{
//	lcm_compare_id();
//	printk("lcm_suspend g_which_lcm=%d\n", g_which_lcm);
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
	unsigned int id1=0,id2=0,id0=0;
	unsigned char buffer[5];
	unsigned int array[3]; 
	int ret;
	//int id_adc = 0;
	//u8 hw_id1 = 0;
	//u8 hw_id2 = 0;
	
#ifndef MACH_FPGA
	/*config rt5081 register 0xB2[7:6]=0x3, that is set db_delay=4ms.*/
	ret = PMU_REG_MASK(0xB2, (0x3 << 6), (0x3 << 6));

	/* set AVDD 5.4v, (4v+28*0.05v) */
	/*ret = RT5081_write_byte(0xB3, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB3, 28, (0x3F << 0));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB3);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB3);

	/* set AVEE */
	/*ret = RT5081_write_byte(0xB4, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB4, 28, (0x3F << 0));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB4);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB4);

	/* enable AVDD & AVEE */
	/* 0x12--default value; bit3--Vneg; bit6--Vpos; */
	/*ret = RT5081_write_byte(0xB1, 0x12 | (1<<3) | (1<<6));*/
	ret = PMU_REG_MASK(0xB1, (1<<3) | (1<<6), (1<<3) | (1<<6));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB1);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB1);

	MDELAY(15);

#endif	
	
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);	

#ifdef BUILD_LK
	DSI_clk_HS_mode(0);
#endif
//add by Y839,read hw id pin 
	//mt_set_gpio_mode( GPIO_DISP_ID0_PIN, GPIO_MODE_00);
	//mt_set_gpio_dir( GPIO_DISP_ID0_PIN, GPIO_DIR_IN);
	//hw_id1 = mt_get_gpio_in( GPIO_DISP_ID0_PIN);
	//mt_set_gpio_mode( GPIO_DISP_ID1_PIN, GPIO_MODE_00);
	//mt_set_gpio_dir( GPIO_DISP_ID1_PIN, GPIO_DIR_IN);
	//hw_id2 = mt_get_gpio_in( GPIO_DISP_ID1_PIN);
	//printf("wangliang hanlong read hw id pin value hw_id1 = %d ,hw_id2 =%d\n",hw_id1,hw_id2);

	/*if(hw_id0 == 1)
	{
		printf("Y839 ctc tn lcd ok ok ok\n");
		return 1;
	}else{
		printf("Y839 ctc tn lcd fail fail fail\n");
		return 0;
	}*/
	array[0] = 0x00053700;// set return byte number
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1,buffer, 3);
//	id = buffer[0];
	id0 = buffer[0];//should be 0x91
	id1 = buffer[1];//should be 0x61
	id2 = buffer[2];//should be 0x00
	
//		if (0 == hw_id2)
//		return 0;
	//id_adc = adc_read_vol();
#if defined(BUILD_LK)
	printf("wangliang lk -- st7703  ctc hanlong 0x%x , 0x%x , 0x%x \n",id0,id1,id2);
#else
	printk("luoyun kernel --st7703 hanlong  id= 0x%x , 0x%x , 0x%x \n",id0,id1,id2);
#endif

	return (0x8802 == (((id0&0x00FF)<<8)|(id1&0x00FF)))?1:0; //8802
	//return ((0x8802 == (((id0&0x00FF)<<8)|(id1&0x00FF)))&&((0 == hw_id1)&&(0==hw_id2)))?1:0; //8802
}

static void lcm_init(void)
{
	int ret;
#ifndef MACH_FPGA
	/*config rt5081 register 0xB2[7:6]=0x3, that is set db_delay=4ms.*/
	ret = PMU_REG_MASK(0xB2, (0x3 << 6), (0x3 << 6));

	/* set AVDD 5.7v, (4v+30*0.05v) */
	/*ret = RT5081_write_byte(0xB3, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB3, 34, (0x3F << 0));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB3);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB3);

	/* set AVEE */
	/*ret = RT5081_write_byte(0xB4, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB4, 34, (0x3F << 0));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB4);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB4);

	/* enable AVDD & AVEE */
	/* 0x12--default value; bit3--Vneg; bit6--Vpos; */
	/*ret = RT5081_write_byte(0xB1, 0x12 | (1<<3) | (1<<6));*/
	ret = PMU_REG_MASK(0xB1, (1<<3) | (1<<6), (1<<3) | (1<<6));
	if (ret < 0)
		printf("st7703----tps6132----cmd=%0x--i2c write error----\n", 0xB1);
	else
		printf("st7703----tps6132----cmd=%0x--i2c write success----\n", 0xB1);

	MDELAY(15);

#endif	
	
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);
	
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
LCM_DRIVER st7703_qhdplus_dsi_boe_pengwei_lcm_drv = {
	.name = "st7703_qhdplus_dsi_boe_pengwei",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};
