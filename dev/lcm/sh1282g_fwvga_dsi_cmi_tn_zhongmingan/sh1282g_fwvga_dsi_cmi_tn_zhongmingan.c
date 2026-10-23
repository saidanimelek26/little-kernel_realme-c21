#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
	#include <string.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										480
#define FRAME_HEIGHT 										854

#define REGFLAG_DELAY             							0xFFF
#define REGFLAG_END_OF_TABLE      							0xEEE   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE	0
#define SH1282G_LCM_ID       0x9504

//added for lcm detect ,read adc voltage

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);

//#define GC9504_TEST
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

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
{0xf0,5,{0x55,0xaa,0x52,0x08,0x00}},
{0xb0,5,{0x00,0x1E,0x14,0x1e,0x14}},

{0xb1,2,{0x78,0x00}},

{0xb4,1,{0x30}},
{0xb5,1,{0x6B}},
{0xb6,1,{0x14}},

{0xB7,5,{0x00,0x00,0x00,0x00,0x00}},

{0xB8,4,{0x00,0x10,0x10,0x10}},
{0xbc,1,{0x00}},
{0xC1,32,{0x00,0x00,0x03,0x07,0x09,0x0B,0x0D,0x00,0x16,0x17,0x00,0x13,0x00,0x00,0x15,0x00,0x00,0x14,0x00,0x00,0x12,0x00,0x17,0x16,0x00,0x0C,0x0A,0x08,0x06,0x02,0x00,0x00}},
{0xC2,5,{0x8A,0x33,0x14,0x89,0x14}},
{0xC3,42,{0x88,0x73,0x03,0x5D,0x0A,0x00,0x00,0x87,0x73,0x03,0x5E,0x0A,0x00,0x00,0x86,0x73,0x03,0x5F,0x0A,0x00,0x00,0x85,0x73,0x03,0x60,0x0A,0x00,0x00,0x84,0x73,0x03,0x61,0x0A,0x00,0x00,0x83,0x73,0x03,0x62,0x0A,0x00,0x00}},
{0xC4,14,{0x82,0x73,0x03,0x63,0x0A,0x00,0x00,0x81,0x73,0x03,0x64,0x0A,0x00,0x00}},
{0xC5,10,{0xAE,0x22,0x14,0xAD,0x14,0xAB,0x33,0x14,0xAA,0x14}},
{0xC6,12,{0x00,0x00,0x70,0x00,0x00,0x71,0x00,0x00,0x71,0x00,0x00,0x70}},
{0xf0,5,{0x55,0xaa,0x52,0x08,0x01}},
{0xb0,1,{0x0a}},
{0xb6,1,{0x54}},
{0xb1,1,{0x0a}},
{0xb7,1,{0x44}},
{0xb2,1,{0x00}},
{0xbf,1,{0x01}},
{0xb3,1,{0x10}},
{0xb9,1,{0x35}},
{0xba,1,{0x15}},
{0xbc,3,{0x00,0x7b,0x00}},
{0xbd,3,{0x00,0x7b,0x00}},
//{0xbe,2,{0x00,0x5d}},
{0xD1,52,{0x00,0x00,0x00,0x01,0x00,0x02,0x00,0x0B,0x00,0x18,0x00,0x3D,0x00,0x67,0x00,0xBA,0x00,0xF8,0x01,0x4B,0x01,0x81,0x01,0xCA,0x01,0xFF,0x02,0x00,0x02,0x2E,0x02,0x5C,0x02,0x75,0x02,0x95,0x02,0xA9,0x02,0xC1,0x02,0xD1,0x02,0xE6,0x02,0xF6,0x03,0x0E,0x03,0x49,0x03,0xF6}},
{0xD2,52,{0x00,0x00,0x00,0x01,0x00,0x02,0x00,0x0B,0x00,0x18,0x00,0x3D,0x00,0x67,0x00,0xBA,0x00,0xF8,0x01,0x4B,0x01,0x81,0x01,0xCA,0x01,0xFF,0x02,0x00,0x02,0x2E,0x02,0x5C,0x02,0x75,0x02,0x95,0x02,0xA9,0x02,0xC1,0x02,0xD1,0x02,0xE6,0x02,0xF6,0x03,0x0E,0x03,0x49,0x03,0xF6}},
{0x8F,6,{0x5A,0x96,0x3C,0xC3,0xA5,0x69}},
{0x8A,1,{0x13}},
{0xFF,4,{0xaa,0x55,0x25,0x01}},
{0xF9,16,{0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f}},
{0x85,6,{0x11,0x44,0x44,0x11,0x44,0x44}},
{0x3a,1,{0x70}},


{0x11, 1, {0x00}},       
{REGFLAG_DELAY, 120, {}},
//Display ON            
{0x29, 1, {0x00}},       
{REGFLAG_DELAY, 20, {}},
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
     
     {0x6C, 1,{0x60}},
	{REGFLAG_DELAY, 20, {}},
	{0xB1, 1,{0x00}},
	{0xFA, 4,{0x7F, 0x00, 0x00, 0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x6c,1,{0x50}}, 
	{REGFLAG_DELAY, 10, {}},

	{0x28, 0,{0x00}},	
	{REGFLAG_DELAY, 50, {}},  
	{0x10, 0,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	
         //??¦Ì?¡ä¨²??
	{0xF0,5,{0x55,0xaa,0x52,0x08,0x00}},
	{0xc2,1,{0xce}},
	{0xc3,1,{0xcd}},
	{0xc6,1,{0xfc}},
	{0xc5,1,{0x03}},
	{0xcd,1,{0x64}},
	{0xc4,1,{0xff}},///REG85 EN
	
	{0xc9,1,{0xcd}},
	{0xF6,2,{0x5a,0x87}},
	{0xFd,3,{0xaa,0xaa, 0x0a}},
	{0xFe,2,{0x6a,0x0a}},
	{0x78,2,{0x2a,0xaa}},
	{0x92,2,{0x17,0x08}},
	{0x77,2,{0xaa,0x2a}},
	{0x76,2,{0xaa,0xaa}},

         {0x84,1,{0x00}},
	{0x78,2,{0x2b,0xba}},
	{0x89,1,{0x73}},
	{0x88,1,{0x3A}},
	{0x85,1,{0xB0}},
	{0x76,2,{0xeb,0xaa}},
	{0x94,1,{0x80}},
	{0x87,3,{0x04,0x07,0x30}},					
	{0x93,1,{0x27}},
	{0xaf,1,{0x02}},
	

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
//				MDELAY(10);
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
    params->dsi.data_format.format	  = LCM_DSI_FORMAT_RGB888;
    
    params->dsi.packet_size=220;
    // Video mode setting		
    params->dsi.intermediat_buffer_num = 2;
    
    params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
   // params->dsi.cont_clock = 1;  //added by cheguosheng  // Continuous mode   or  not Continuous mode

	params->dsi.vertical_sync_active	= 10;// 3    2  4
	params->dsi.vertical_backporch		= 20;// 20   11  6
	params->dsi.vertical_frontporch		= 28; // 8 1  12 11
	params->dsi.vertical_active_line	= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active	= 10;// 10 50  20
	params->dsi.horizontal_backporch	= 90;//30  150
	params->dsi.horizontal_frontporch	= 30;//30 150
  
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
    params->dsi.PLL_CLOCK				= 206;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].cmd          = 0x0A;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;	
}


static unsigned int lcm_compare_id(void);

static void lcm_init(void)
{
    SET_RESET_PIN(1);
	MDELAY(10);//Must > 5ms
    SET_RESET_PIN(0);
    MDELAY(100);//Must > 5ms
    SET_RESET_PIN(1);
    MDELAY(120);//Must > 50ms
	#if defined(BUILD_LK)
		printf("sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 lcm_init \n");
	#else
		printk("sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 lcm_init \n");
	#endif

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{

	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);
     MDELAY(10);//Must > 5ms
 
}


static void lcm_resume(void)
{
    #ifdef BUILD_LK
		printf("%s, LK sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 debug \n", __func__);
    #else
		printk("%s, kernel sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 debug \n", __func__);
    #endif
        
	//lcm_compare_id();
	/*lcm_initialization_setting[15].para_list[0]+=2;
	lcm_initialization_setting[16].para_list[0]+=2;
	lcm_initialization_setting[17].para_list[0]+=2;
	lcm_initialization_setting[17].para_list[1]+=2;*/
lcm_init();
		//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

//added for lcm detect ,read adc voltage
static int adc_read_vol(int channel)
{
        int adc[1];
        int data[4] ={0,0,0,0};
        int sum = 0;
        int adc_vol=0;
        int num = 0;
        int ret =0;

        for(num=0;num<10;num++)
        {
                ret = IMM_GetOneChannelValue(channel, data, adc);
                sum+=(data[0]*100+data[1]);
        }
        adc_vol = sum/10;
#if defined(BUILD_LK)
        printf(" lk -22-- sunritel test channel= %d, adc_vol is %d\n",channel,adc_vol);
#else
        printk(" kernel -22-- sunritel test adc_vol is %d\n",adc_vol);
#endif

        if(adc_vol > 60)
        {
                printf("qicai adc_vol < 60 \n");
                return 1;
        }

        else
        {
                printf("hanlong adc_vol > 60 \n");
                return 0;
        }
}

static unsigned int lcm_compare_id(void)
{ 
    int array[4];
    char buffer[4];
    int id=0;
    
    unsigned int id_auxadc = 0;
	struct LCM_setting_table switch_table_page[] = {
		{ 0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x01} }
	};
	
	//int id1_adc = 2;
	int id2_adc = 2;
    
    SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(120);
    push_table(switch_table_page, sizeof(switch_table_page) / sizeof(struct LCM_setting_table), 1);
    ////// for id start
    array[0] = 0x00033700;
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xc6, buffer, 2);
    
    id = (buffer[0] << 8) | buffer[1];

    #if defined(BUILD_LK)
    printf("%s,lk sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #endif
    ////// for id end
	//id1_adc = adc_read_vol(1);
	id2_adc = adc_read_vol(12);

  	printf("11 id2_adc=%d--- \n",id2_adc);

    #ifdef BUILD_LK
    printf("%s,lk sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #endif
    ////// for id_auxadc end
    
    //if((0x9503 == id || 0x9504 == id) && (id_auxadc == 1))
	if((0x1282 == id) && (id2_adc == 1))
    {
        #ifdef BUILD_LK
        printf("%s,lk sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120,read lcd id success,return 1\n", __func__);
        #else
        printk("%s,kernel sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120,read lcd id success,return 1\n", __func__);
        #endif
        
        return 1;
    }

    
    #ifdef BUILD_LK
    printf("%s,lk sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120 id,read lcd id fail,return 0\n", __func__);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_cmi_zhongmingan_taiyi_sp120,read lcd id fail,return 0\n", __func__);
    #endif
    
    return 0;
}


LCM_DRIVER sh1282g_fwvga_dsi_cmi_tn_zhongmingan_lcm_drv =
{
    .name			= "sh1282g_fwvga_dsi_cmi_tn_zhongmingan",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
