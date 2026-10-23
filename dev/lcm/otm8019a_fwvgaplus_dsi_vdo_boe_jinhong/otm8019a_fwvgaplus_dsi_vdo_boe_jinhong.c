/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* The following software/firmware and/or related documentation ("MediaTek Software")
* have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
* applicable license agreements with MediaTek Inc.
*/
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
	#include <mach/mt_gpio.h>
#endif



// ---------------------------------------------------------------------------
// Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH (480)
#define FRAME_HEIGHT (960)

#define REGFLAG_DELAY 0XFD
#define REGFLAG_END_OF_TABLE 0xFE // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE 0   
#define LCM_ID_OTM8019A                       (0x8019)


#define LCD_MODUL_ID (0x02)

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);

#if 0
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE; ///only for ESD test

#endif

// ---------------------------------------------------------------------------
// Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
// Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size) 

static struct LCM_setting_table {
unsigned char cmd;
unsigned char count;
unsigned char para_list[64];
}lcm_initialization_setting[],lcm_deep_sleep_mode_in_setting[];

static struct LCM_setting_table lcm_initialization_setting[] = {
{0x00,1,{0x00}},
{0xFF,3,{0x80,0x19,0x01}},
{0x00,1,{0x80}},
{0xFF,2,{0x80,0x19}},
{0x00,1,{0x90}},
{0xB3,1,{0x02}},
{0x00,1,{0x92}},
{0xB3,1,{0x40}},
{0x00,1,{0xA2}},
{0xB3,4,{0x01,0xE0,0x03,0xC0}},
{0x00,1,{0xA7}},
{0xB3,1,{0x04}},
{0x00,1,{0x80}},
{0xC0,6,{0x00,0x59,0x00,0x15,0x15,0x00}},
{0x00,1,{0x90}},
{0xC0,6,{0x00,0x15,0x00,0x00,0x00,0x03}},
{0x00,1,{0xA0}},
{0xC1,1,{0xe8}},
{0x00,1,{0x88}}, 
{0xce,4,{0x01,0xe0,0x03,0xc0}},
{0x00,1,{0x8b}}, 
{0xce,1,{0x04}},
{0x00,1,{0x80}}, 
{0xce,12,{0x85,0x01,0x00,0x84,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0x90}}, 
{0xce,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xa0}}, 
{0xce,14,{0x18,0x01,0x03,0xc0,0x00,0x00,0x00,0x18,0x00,0x03,0xc1,0x00,0x00,0x00}},
{0x00,1,{0xb0}}, 
{0xce,14,{0x18,0x03,0x03,0xc2,0x00,0x00,0x00,0x18,0x02,0x03,0xc4,0x00,0x00,0x00}},
{0x00,1,{0xc0}},
{0xce,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xd0}}, 
{0xce,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xc0}},
{0xcf,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00}},
{0x00,1,{0x80}}, 
{0xcb,9,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0x90}}, 
{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xa0}}, 
{0xcb,1,{0x00}},
{0x00,1,{0xa5}}, 
{0xcb,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xb0}},
{0xcb,6,{0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xc0}}, 
{0xcb,15,{0x15,0x3f,0x15,0x15,0x15,0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xd0}}, 
{0xcb,1,{0x00}},
{0x00,1,{0xd5}}, 
{0xcb,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xe0}}, 
{0xcb,6,{0x15,0x15,0x15,0x15,0x3f,0x15}},
{0x00,1,{0xf0}}, 
{0xcb,8,{0xff,0x3f,0x00,0x00,0x00,0x00,0xfc,0xff}},
{0x00,1,{0x80}},  
{0xcc,10,{0x26,0x1d,0x0c,0x0a,0x02,0x25,0x1e,0x00,0x00,0x00}},
{0x00,1,{0x90}},   
{0xcc,6,{0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0x9a}}, 
{0xcc,5,{0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xa0}},
{0xcc,11,{0x00,0x00,0x00,0x00,0x1e,0x25,0x01,0x09,0x0b,0x1d,0x26}},
{0x00,1,{0xb0}},  
{0xcc,10,{0x25,0x1d,0x09,0x0b,0x01,0x26,0x1e,0x00,0x00,0x00}},
{0x00,1,{0xc0}}, 
{0xcc,6,{0x00,0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xca}}, 
{0xcc,5,{0x00,0x00,0x00,0x00,0x00}},
{0x00,1,{0xd0}},  
{0xcc,11,{0x00,0x00,0x00,0x00,0x1e,0x26,0x02,0x0c,0x0a,0x1d,0x25}},
{0x00,1,{0x80}},
{0xc1,2,{0x03,0x33}},
{0x00,1,{0x90}}, 
{0xc5,7,{0x4e,0x2d,0x01,0x91,0x33,0x34,0x23}},
{0x00,1,{0x82}},
{0xC5,1,{0xb1}},
{0x00,1,{0x00}},
{0xD8,2,{0xA8,0xA8}},
{0x00,1,{0x00}},
{0xD9,1,{0x23}},
{0x00,1,{0xb1}},
{0xC5,1,{0xb9}},
{0x00,1,{0x89}},
{0xC4,1,{0x08}},
{0x00,1,{0x00}},
{0xE1,20,{0x0A,0x0E,0x13,0x18,0x20,0x28,0x24,0x45,0x30,0x49,0xB9,0xA0,0xAB,0x74,0x6B,0x58,0x44,0x31,0x21,0x05}},
{0x00,1,{0x00}},
{0xE2,20,{0x0A,0x0E,0x13,0x18,0x20,0x28,0x24,0x45,0x30,0x49,0xB9,0xA0,0xAB,0x74,0x6B,0x58,0x44,0x31,0x21,0x05}},
{0x00,1,{0x90}}, 
{0xC0,6,{0x00,0x15,0x00,0x00,0x00,0x04}},
{0x00,1,{0x80}}, 
{0xC4,1,{0x30}},
{0x00,1,{0x80}}, 
{0xC1,3,{0x03,0x33,0x84}},
{0x00,1,{0x98}}, 
{0xC0,1,{0x00}},
{0x00,1,{0xA9}}, 
{0xC0,1,{0x0A}},
{0x00,1,{0xB0}}, 
{0xC1,6,{0x20,0x00,0x00,0x00,0x02,0x01}},
{0x00,1,{0xE1}}, 
{0xC0,2,{0x40,0x30}},
{0x00,1,{0xA0}}, 
{0xC1,1,{0xE8}},
{0x00,1,{0x90}}, 
{0xB6,2,{0xB4,0x5A}},
{0x00,1,{0x83}}, 
{0xC5,1,{0x01}},
{0x00,1,{0xC4}}, 
{0xC5,2,{0x00,0x00}},
{0x00,1,{0xBA}}, 
{0xF5,2,{0x00,0x00}},
{0x00,1,{0x00}}, 
{0xFB,1,{0x01}},
{0x00,1,{0x80}}, 
{0xFF,2,{0xFF,0xFF}},
{0x00,1,{0x00}},
{0xFF,3,{0xFF,0xFF,0xFF}},

{0x11,0,{0x00}},
{REGFLAG_DELAY,150,{}},
{0x29,0,{0x00}},
{REGFLAG_DELAY,30,{}},

{REGFLAG_END_OF_TABLE, 0x00, {}}


};


#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
     // Display off sequence
     {0x28, 1, {0x00}},
     {REGFLAG_DELAY, 20, {}},
     // Sleep Mode Ondiv1_real*div2_real
     {0x10, 1, {0x00}},
     {REGFLAG_DELAY, 120, {}},
     {REGFLAG_END_OF_TABLE, 0x00, {}}
};

#if 0
static struct LCM_setting_table lcm_backlight_level_setting[] = {
    {0x51, 1, {0xFF}},
    {0x53, 1, {0x24}},//close dimming
    {0x55, 1, {0x00}},//close cabc
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

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
        }
    }
}


// ---------------------------------------------------------------------------
// LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;
		params->physical_width =55;
		params->physical_height = 111;

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
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 8;
		params->dsi.vertical_backporch					= 20;//8
		params->dsi.vertical_frontporch					= 20;//8
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 4;//6
		params->dsi.horizontal_backporch				= 32;//37
		params->dsi.horizontal_frontporch				= 32;//37
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
/*
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;		
*/
		params->dsi.ssc_disable = 1;
		params->dsi.PLL_CLOCK=177;

}





static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
    lcm_init();
}
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
	unsigned int id=0;
    unsigned char buffer[5]={0};
    unsigned int array[2]={0};
	int id1_adc = 2;
	int id2_adc = 2;

    SET_RESET_PIN(1);
	MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(25);
    SET_RESET_PIN(1);
    MDELAY(50);

    array[0]=0x00043902;
    array[1]=0x011980ff;
    dsi_set_cmdq(array, 2, 1); 
    
    array[0]=0x80001500;
    dsi_set_cmdq(array, 1, 1); 
    
    array[0]=0x00033902;
    array[1]=0x001980ff;
    dsi_set_cmdq(array, 2, 1);  
  
    array[0] = 0x00053700;
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xA1, buffer, 5); 

    id = buffer[2]<<8 |buffer[3];
	//adc_read_vol(0);
	//id1_adc = adc_read_vol(1);
	//id2_adc = adc_read_vol(12);

  	//printf("22---id1_adc=%d, id2_adc=%d--- \n",id1_adc,id2_adc);
	printf("---id=0x%x--- \n",id);
    //return ((id==LCM_ID_OTM8019A)&& ((id1_adc == 0)&& (id2_adc == 1)))?1:0;
	return (id==LCM_ID_OTM8019A)?1:0;	
}

LCM_DRIVER otm8019a_fwvgaplus_dsi_vdo_boe_jinhong_lcm_drv = 
{
    .name = "otm8019a_fwvgaplus_dsi_vdo_boe_jinhong",
    .set_util_funcs = lcm_set_util_funcs,
    .compare_id = lcm_compare_id,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
};

