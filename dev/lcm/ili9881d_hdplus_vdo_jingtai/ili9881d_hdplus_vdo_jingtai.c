#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/wait.h>
#endif

#include "lcm_drv.h"

#ifdef MTK_ROUND_CORNER_SUPPORT
#include "data_rgba4444_roundedpattern.h"
#endif

// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1560)

#define   LCM_DSI_CMD_MODE							0

#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER

// static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

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
	{0xFF,03,{0x98,0x81,0x01}},                                                  
	{0x91,01,{0x00}},                    
	{0x92,01,{0x00}},                    
	{0x93,01,{0x73}},                        
	{0x94,01,{0x00}},                            
	{0x95,01,{0x00}},                            
	{0x96,01,{0x08}},                      
	{0x97,01,{0x00}},                           
	{0x98,01,{0x00}},                     
	{0x09,01,{0x00}},
	{0x0a,01,{0x00}},
	{0x0b,01,{0x00}},
	{0x0c,01,{0x00}},
	{0x0d,01,{0x00}},
	{0x0e,01,{0x00}},
	{0x0f,01,{0x20}},	
	{0x10,01,{0x20}},	
	{0x11,01,{0x00}},
	{0x12,01,{0x00}},
	{0x13,01,{0x02}},
	{0x14,01,{0x00}},
	{0x15,01,{0x08}},
	{0x16,01,{0x10}}, 
	{0x17,01,{0x00}}, 
	{0x18,01,{0x00}},
	{0x19,01,{0x00}},
	{0x1a,01,{0x00}},
	{0x1b,01,{0x00}},
	{0x1c,01,{0x00}},
	{0x1d,01,{0x00}},
	{0x1e,01,{0xC0}},	
	{0x1f,01,{0x00}},  
	{0x20,01,{0x06}},	
	{0x21,01,{0x01}},	
	{0x22,01,{0x00}},   
	{0x23,01,{0x00}},
	{0x24,01,{0x00}},  
	{0x25,01,{0x00}}, 
	{0x26,01,{0x00}},
	{0x27,01,{0x00}},
	{0x28,01,{0x33}}, 
	{0x29,01,{0x33}},
	{0x2a,01,{0x00}},  
	{0x2b,01,{0x00}},
	{0x2c,01,{0x09}},      
	{0x2d,01,{0x09}},     
	{0x2e,01,{0x09}},        
	{0x2f,01,{0x09}},      
	{0x30,01,{0x00}},
	{0x31,01,{0x00}},
	{0x32,01,{0x02}},    
	{0x33,01,{0x00}},
	{0x34,01,{0x00}},      
	{0x35,01,{0x0A}},       
	{0x36,01,{0x00}},
	{0x37,01,{0x08}},      
	{0x38,01,{0x00}},	
	{0x39,01,{0x07}},                    
	{0x3a,01,{0x00}},                    
	{0x3b,01,{0x00}},                    
	{0x3c,01,{0x00}},                                                
	{0x40,01,{0x03}},                    
	{0x41,01,{0x20}},                    
	{0x42,01,{0x00}},                    
	{0x43,01,{0x40}},                    
	{0x44,01,{0x03}},                    
	{0x45,01,{0x00}},                    
	{0x46,01,{0x01}},                    
	{0x47,01,{0x08}},                    
	{0x48,01,{0x00}},                    
	{0x49,01,{0x00}},                    
	{0x4a,01,{0x00}},                    
	{0x4b,01,{0x00}},                                                    
	{0x4c,01,{0x28}},          
	{0x4d,01,{0x22}},    
	{0x4e,01,{0x8A}},              
	{0x4f,01,{0x46}},              
	{0x50,01,{0x70}},              
	{0x51,01,{0x26}},                
	{0x52,01,{0x22}},    
	{0x53,01,{0x22}},    
	{0x54,01,{0x22}},    
	{0x55,01,{0x22}},    
	{0x56,01,{0x16}},                         
	{0x57,01,{0x29}},    
	{0x58,01,{0x22}},   
	{0x59,01,{0x9B}},   
	{0x5a,01,{0x57}},   
	{0x5b,01,{0x70}},   
	{0x5c,01,{0x27}},   
	{0x5d,01,{0x22}},   
	{0x5e,01,{0x22}},   
	{0x5f,01,{0x22}},   
	{0x60,01,{0x22}},   
	{0x61,01,{0x16}},   
	{0x62,01,{0x06}},   
	{0x63,01,{0x07}},   
	{0x64,01,{0x02}},   
	{0x65,01,{0x02}},   
	{0x66,01,{0x02}},   
	{0x67,01,{0x55}},   
	{0x68,01,{0x57}},   
	{0x69,01,{0x59}},   
	{0x6a,01,{0x5B}},   
	{0x6b,01,{0x00}},   
	{0x6c,01,{0xA7}},   
	{0x6d,01,{0x09}},   
	{0x6e,01,{0x02}},   
	{0x6f,01,{0x02}},   
	{0x70,01,{0x02}},   
	{0x71,01,{0x02}},   
	{0x72,01,{0x02}},   
	{0x73,01,{0x02}},   
	{0x74,01,{0x02}},   
	{0x75,01,{0x02}},   
	{0x76,01,{0x02}},   
	{0x77,01,{0xA6}},   
	{0x78,01,{0x01}},                   
	{0x79,01,{0x06}},            
	{0x7a,01,{0x02}},   
	{0x7b,01,{0x02}},   
	{0x7c,01,{0x02}},   
	{0x7d,01,{0x54}},           
	{0x7e,01,{0x56}},           
	{0x7f,01,{0x58}},           
	{0x80,01,{0x5A}},           
	{0x81,01,{0x00}},           
	{0x82,01,{0xA7}},           
	{0x83,01,{0x08}},            
	{0x84,01,{0x02}},   
	{0x85,01,{0x02}},  
	{0x86,01,{0x02}},                    
	{0x87,01,{0x02}},                    
	{0x88,01,{0x02}},                    
	{0x89,01,{0x02}},                    
	{0x8a,01,{0x02}},                    
	{0x8b,01,{0x02}},                    
	{0x8c,01,{0x02}},  
	{0x8d,01,{0xA6}},  
	{0x8e,01,{0x01}},  
	{0xa0,01,{0x35}},              
	{0xa1,01,{0x00}},  
	{0xa2,01,{0x00}},  
	{0xa3,01,{0x00}},  
	{0xa4,01,{0x00}},        
	{0xa5,01,{0x10}},       
	{0xa6,01,{0x08}},  
	{0xa7,01,{0x00}},                    
	{0xa8,01,{0x00}},                    
	{0xa9,01,{0x00}},                    
	{0xaa,01,{0x00}},                    
	{0xab,01,{0x00}},                    
	{0xac,01,{0x00}},                    
	{0xad,01,{0x00}},                    
	{0xae,01,{0xff}},                    
	{0xaf,01,{0x00}},                    
	{0xb0,01,{0x00}},                                    
	{0xFF,03,{0x98,0x81,0x02}},             
	{0x42,01,{0x29}},                                                  
	{0xA0,20,{0x00,0x0A,0x13,0x10,0x10,0x22,0x16,0x19,0x53,0x1A,0x26,0x54,0x1F,0x1F,0x53,0x26,0x29,0x4D,0x5D,0x3F}},						
	{0xC0,20,{0x00,0x0A,0x13,0x10,0x10,0x22,0x16,0x19,0x53,0x1A,0x26,0x54,0x1F,0x1F,0x53,0x26,0x29,0x4D,0x5D,0x3F}},				
	{0x18,01,{0xF4}},      
	{0xFF,03,{0x98,0x81,0x04}},
	{0x00,01,{0x00}},
	{0x5D,01,{0x6B}},     
	{0x5E,01,{0x6B}},     
	{0x60,01,{0x92}},     
	{0x62,01,{0x88}},     
	{0x82,01,{0x20}},     
	{0x84,01,{0x20}},     
	{0x86,01,{0x38}},     
	{0x66,01,{0x04}},     
	{0xC1,01,{0x70}},     
	{0x70,01,{0x60}}, 
	{0x71,01,{0x00}},
	{0x5B,01,{0x33}},   
	{0x6C,01,{0x10}},   
	{0x77,01,{0x03}},   
	{0x7B,01,{0x02}},   
	{0xFF,03,{0x98,0x81,0x01}},                                
	{0xF0,01,{0x01}},          
	{0xF1,01,{0x0E}},  
	{0xFF,03,{0x98,0x81,0x05}},
	{0x22,01,{0x3A}},                                    
	{0xFF,03,{0x98,0x81,0x00}},     
	{0x35,01,{0x00}},        			 
	{0x11, 1, {0x00}},		 
	{REGFLAG_DELAY, 100, {}},
	// Display ON			 
	{0x29, 1, {0x00}},		 
	{REGFLAG_DELAY, 100, {}},	
};

static void init_lcm_registers(void)
{

#if 1

   	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

#else    
    	
#endif

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
		
		params->physical_width = 64;
		params->physical_height = 139;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		//params->physical_width = 71;
		//params->physical_height = 129;
		
        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_THREE_LANE;
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
		
		
		params->dsi.vertical_sync_active				= 16;
		params->dsi.vertical_backporch					= 40;
		params->dsi.vertical_frontporch					= 30;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 30;
		params->dsi.horizontal_backporch				= 30;
		params->dsi.horizontal_frontporch				= 50;

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		// params->dsi.compatibility_for_nvk = 0;
        params->dsi.ssc_disable = 1;

		params->dsi.esd_check_enable = 1;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
		params->dsi.lcm_esd_check_table[0].count        = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		
		params->dsi.HS_TRAIL=20;	
        params->dsi.PLL_CLOCK = 320;//183 212 195 200 205 180 230

#ifdef MTK_ROUND_CORNER_SUPPORT
	params->round_corner_params.round_corner_en = 1;
	params->round_corner_params.full_content = 0;
	params->round_corner_params.w = ROUND_CORNER_W;
	params->round_corner_params.h = ROUND_CORNER_H;
	params->round_corner_params.lt_addr = left_top;
	params->round_corner_params.lb_addr = left_bottom;
	params->round_corner_params.rt_addr = right_top;
	params->round_corner_params.rb_addr = right_bottom;
#endif
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



         
#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

static unsigned int lcm_compare_id(void)
{
	int array[4];
	char buffer[5];
	char id_high=0;
	char id_low=0;
	int id=0;

	SET_RESET_PIN(1);
	MDELAY(20); 
	SET_RESET_PIN(0);
	MDELAY(20); 	  
	SET_RESET_PIN(1);
	MDELAY(120);	  
   
	array[0]=0x00043902;
	array[1]=0x068198FF;
	dsi_set_cmdq(array, 2, 1);
	MDELAY(10);

	array[0]=0x00023700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF0, buffer,1);
	id_high = buffer[0];		 //0x98

	array[0]=0x00023700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF1, buffer,1);
	id_low = buffer[0]; 		 //0x81
	id =(id_high << 8) | id_low;
#ifdef BUILD_LK
	printf("zrl_lk -- ili9881 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#else
	printk("zrl_lk -- ili9881 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#endif
	return (id ==0x9881)?1:0;
}

static void lcm_init(void)
{

		
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
	
//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

LCM_DRIVER ili9881d_hdplus_vdo_jingtai_lcm_drv = 
{
    .name			= "ili9881d_hdplus_vdo_jingtai",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
    #if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
    #endif
};

