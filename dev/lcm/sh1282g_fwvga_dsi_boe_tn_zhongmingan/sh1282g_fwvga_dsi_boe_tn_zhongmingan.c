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
         {0xF0, 5,{0x55,0xaa,0x52,0x08,0x00}},
		 {0xB0, 5,{0x00,0x0C,0x14,0x1e,0x14}},
		 {0xB1, 2,{0x78,0x00}},
		 {0xB4, 1,{0x00}},
		 {0xB5, 1,{0x6B}},
		 {0xB6, 1,{0x08}},
		 {0xB7, 4,{0x23,0x00,0x23,0x00}},
		 {0xB8, 4,{0x00,0x10,0x10,0x10}},
         {0xBA, 1,{0x01}},
		 {0xBB, 1,{0x33}},
		 {0xBC, 1,{0x00}},
		 {0xC1, 32,{0x00,0x01,0x06,0x08,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x03,0x09,0x07,0x01,0x00}},
		 {0xC2, 5,{0x85,0x11,0x00,0x84,0x00}},
		 {0xC3, 28,{0x86,0x31,0x03,0x5b,0x00,0x00,0x00,0x85,0x31,0x03,0x5c,0x00,0x00,0x00,0x84,0x31,0x03,0x5d,0x00,0x00,0x00,0x83,0x31,0x03,0x5e,0x00,0x00,0x00}},
		 {0x8f, 6,{0x5a,0x96,0x3c,0xc3,0xa5,0x69}},

		 {0x8A, 1,{0x13}},
		 {0x85, 6,{0x11,0x44,0x44,0x11,0x44,0x44}},
		 {0xF0, 5,{0x55,0xAA,0x52,0x08,0x01}},
		 {0xB0, 1,{0x08}},
		 {0xB1, 1,{0x08}},
		 {0xB2, 1,{0x00}},
		 {0xB3, 1,{0x10}},
		 {0xB6, 1,{0x23}},
		 {0xB7, 1,{0x23}},
		 {0xB8, 1,{0x24}},
	     {0xB9, 1,{0x36}},
		 {0xBA, 1,{0x27}},
		 {0xBC, 3,{0x00,0x90,0x00}},
		 {0xBD, 3,{0x00,0x90,0x00}},
		 //{0xBE, 2,{0x00,0x80}}, //set vcom
		 {0xBF, 1,{0x01}},
		 {0xD1, 52,{0x00,0x00,0x00,0x15,0x00,0x47,0x00,0x6d,0x00,0x80,0x00,0xa1,0x00,0xba,0x00,0xf9,0x01,0x2f,0x01,0x79,0x01,0xab,0x01,0xf6,0x02,0x2c,0x02,0x2d,0x02,0x5e,0x02,0x92,0x02,0xb3,0x02,0xe1,0x03,0x01,0x03,0x2c,0x03,0x48,0x03,0x6c,0x03,0x87,0x03,0xbc,0x03,0xbd,0x03,0xbe}},
		 {0xD2, 52,{0x00,0x00,0x00,0x15,0x00,0x47,0x00,0x6d,0x00,0x80,0x00,0xa1,0x00,0xba,0x00,0xf9,0x01,0x2f,0x01,0x79,0x01,0xab,0x01,0xf6,0x02,0x2c,0x02,0x2d,0x02,0x5e,0x02,0x92,0x02,0xb3,0x02,0xe1,0x03,0x01,0x03,0x2c,0x03,0x48,0x03,0x6c,0x03,0x87,0x03,0xbc,0x03,0xbd,0x03,0xbe}},

		 {0x8F, 6,{0x5A,0x96,0x3C,0xC3,0xA5,0x69}},
		 {0x8A, 1,{0x13}},
		 {0x85, 6,{0x11,0x44,0x44,0x11,0x44,0x44}},
		 {0x3A, 1,{0x77}},
         {0x35, 1,{0x00}},

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
     


	{0x28, 0,{0x00}},	
	{REGFLAG_DELAY, 50, {}},  
	{0x10, 0,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x4f, 1, {0x01}},
	

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

	params->dsi.vertical_sync_active	= 4;// 3    2  4
	params->dsi.vertical_backporch		= 8;// 20   11  6
	params->dsi.vertical_frontporch		= 20; // 8 1  12 11
	params->dsi.vertical_active_line	= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active	= 4;// 10 50  20
	params->dsi.horizontal_backporch	= 120;//30  150
	params->dsi.horizontal_frontporch	= 10;//30 150
  
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
    params->dsi.PLL_CLOCK				= 204;
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
		printf("sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 lcm_init \n");
	#else
		printk("sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 lcm_init \n");
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
		printf("%s, LK sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 debug \n", __func__);
    #else
		printk("%s, kernel sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 debug \n", __func__);
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
    printf("%s,lk sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 id=0x%x,buffer[0]=%x,buffer[1]=%x,buffer[2]=%x\n",__func__,id, buffer[0], buffer[1], buffer[2]);
    #endif
	
	//id1_adc = adc_read_vol(1);
	id2_adc = adc_read_vol(12);

  	printf("11 id2_adc=%d--- \n",id2_adc);

    #ifdef BUILD_LK
    printf("%s,lk sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 lcd_auxadc_id=%d\n",__func__,id_auxadc);
    #endif
    ////// for id_auxadc end
    
    //if((0x9503 == id || 0x9504 == id) && (id_auxadc == 1))
	if((0x1282 == id) && (id2_adc == 0))
    {
        #ifdef BUILD_LK
        printf("%s,lk sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120,read lcd id success,return 1\n", __func__);
        #else
        printk("%s,kernel sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120,read lcd id success,return 1\n", __func__);
        #endif
        
        return 1;
    }

    
    #ifdef BUILD_LK
    printf("%s,lk sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120 id,read lcd id fail,return 0\n", __func__);
    #else
    printk("%s,kernel sh1282g_fwvga_dsi_vdo_boe_zhongmingan_taiyi_sp120,read lcd id fail,return 0\n", __func__);
    #endif
    
    return 0;
}


LCM_DRIVER sh1282g_fwvga_dsi_boe_tn_zhongmingan_lcm_drv =
{
    .name			= "sh1282g_fwvga_dsi_boe_tn_zhongmingan",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
