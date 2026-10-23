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
*/

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)       (lcm_util.mdelay(n))
#define UDELAY(n)       (lcm_util.udelay(n))

#define GPIO_LCM_PWR_EN      GPIO91
#define GPIO_LCM_RST      GPIO45
#define GPIO_LCM_LDO_1V8      GPIO175



/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
        lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
      lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (800)
#define FRAME_HEIGHT                                    (1280)

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 10, {}},
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {} },
};

#if 1
static struct LCM_setting_table init_setting[] = {
{0xE0,1,{0x00}},
{0xE1,1,{0x93}},
{0xE2,1,{0x65}},
{0xE3,1,{0xF8}},
{0x80,1,{0x03}},

{0xE0,1,{0x01}},
{0x00,1,{0x00}},
{0x01,1,{0x2A}},
{0x03,1,{0x00}},
{0x04,1,{0x30}},

{0x17,1,{0x00}},
{0x18,1,{0xE7}},
{0x19,1,{0x00}},
{0x1A,1,{0x00}},
{0x1B,1,{0xE7}},
{0x1C,1,{0x00}},

{0x35,1,{0x23}},

{0x37,1,{0x09}},	//SS=1,BGR=1

{0x38,1,{0x04}},
{0x39,1,{0x08}},
{0x3A,1,{0x12}},
{0x3C,1,{0x78}},
{0x3D,1,{0xFF}},
{0x3E,1,{0xFF}},
{0x3F,1,{0xFF}},

{0x40,1,{0x06}},
{0x41,1,{0xA0}},
{0x43,1,{0x14}},
{0x44,1,{0x0F}},
{0x45,1,{0x30}},
{0x4B,1,{0x04}},

//{0x4A,1,{0x35}},

{0x55,1,{0x02}},
{0x57,1,{0x65}},
{0x59,1,{0x0A}},
{0x5A,1,{0x28}},
{0x5B,1,{0x15}},

//gamma2.2            //gamma2.5
{0x5D,1,{0x7C}},//0x7C
{0x5E,1,{0x5A}},//0x57
{0x5F,1,{0x45}},//0x41
{0x60,1,{0x36}},//0x31
{0x61,1,{0x2F}},//0x29
{0x62,1,{0x20}},//0x19
{0x63,1,{0x24}},//0x1D
{0x64,1,{0x0F}},//0x08
{0x65,1,{0x2C}},//0x26
{0x66,1,{0x2E}},//0x28
{0x67,1,{0x31}},//0x2A
{0x68,1,{0x52}},//0x48
{0x69,1,{0x40}},//0x35
{0x6A,1,{0x49}},//0x3B
{0x6B,1,{0x3E}},//0x30
{0x6C,1,{0x3B}},//0x2F
{0x6D,1,{0x30}},//0x25
{0x6E,1,{0x21}},//0x17
{0x6F,1,{0x0C}},//0x0C
{0x70,1,{0x7C}},//0x7C
{0x71,1,{0x5A}},//0x57
{0x72,1,{0x45}},//0x41
{0x73,1,{0x36}},//0x31
{0x74,1,{0x2F}},//0x29
{0x75,1,{0x20}},//0x19
{0x76,1,{0x24}},//0x1D
{0x77,1,{0x0F}},//0x08
{0x78,1,{0x2C}},//0x26
{0x79,1,{0x2E}},//0x28
{0x7A,1,{0x31}},//0x2A
{0x7B,1,{0x52}},//0x48
{0x7C,1,{0x40}},//0x35
{0x7D,1,{0x49}},//0x3B
{0x7E,1,{0x3E}},//0x30
{0x7F,1,{0x3B}},//0x2F
{0x80,1,{0x30}},//0x25
{0x81,1,{0x21}},//0x17
{0x82,1,{0x0C}},//0x0C


{0xE0,1,{0x02}},

{0x00,1,{0x5F}},
{0x01,1,{0x5F}},
{0x02,1,{0x53}},
{0x03,1,{0x51}},
{0x04,1,{0x4B}},
{0x05,1,{0x49}},
{0x06,1,{0x47}},
{0x07,1,{0x45}},
{0x08,1,{0x41}},
{0x09,1,{0x43}},
{0x0A,1,{0x5F}},
{0x0B,1,{0x5F}},
{0x0C,1,{0x5F}},
{0x0D,1,{0x5F}},
{0x0E,1,{0x5F}},
{0x0F,1,{0x5F}},
{0x10,1,{0x5F}},
{0x11,1,{0x5F}},
{0x12,1,{0x5F}},
{0x13,1,{0x5F}},
{0x14,1,{0x5F}},
{0x15,1,{0x5F}},

{0x16,1,{0x5F}},
{0x17,1,{0x5F}},
{0x18,1,{0x52}},
{0x19,1,{0x50}},
{0x1A,1,{0x4A}},
{0x1B,1,{0x48}},
{0x1C,1,{0x46}},
{0x1D,1,{0x44}},
{0x1E,1,{0x40}},
{0x1F,1,{0x42}},
{0x20,1,{0x5F}},
{0x21,1,{0x5F}},
{0x22,1,{0x5F}},
{0x23,1,{0x5F}},
{0x24,1,{0x5F}},
{0x25,1,{0x5F}},
{0x26,1,{0x5F}},
{0x27,1,{0x5F}},
{0x28,1,{0x5F}},
{0x29,1,{0x5F}},
{0x2A,1,{0x5F}},
{0x2B,1,{0x5F}},

{0x2C,1,{0x1F}},
{0x2D,1,{0x1F}},
{0x2E,1,{0x00}},
{0x2F,1,{0x02}},
{0x30,1,{0x08}},
{0x31,1,{0x0A}},
{0x32,1,{0x04}},
{0x33,1,{0x06}},
{0x34,1,{0x12}},
{0x35,1,{0x10}},
{0x36,1,{0x1F}},
{0x37,1,{0x1F}},
{0x38,1,{0x1F}},
{0x39,1,{0x1F}},
{0x3A,1,{0x1F}},
{0x3B,1,{0x1F}},
{0x3C,1,{0x1F}},
{0x3D,1,{0x1F}},
{0x3E,1,{0x1F}},
{0x3F,1,{0x1F}},
{0x40,1,{0x1F}},
{0x41,1,{0x1F}},

{0x42,1,{0x1F}},
{0x43,1,{0x1F}},
{0x44,1,{0x01}},
{0x45,1,{0x03}},
{0x46,1,{0x09}},
{0x47,1,{0x0B}},
{0x48,1,{0x05}},
{0x49,1,{0x07}},
{0x4A,1,{0x13}},
{0x4B,1,{0x11}},
{0x4C,1,{0x1F}},
{0x4D,1,{0x1F}},
{0x4E,1,{0x1F}},
{0x4F,1,{0x1F}},
{0x50,1,{0x1F}},
{0x51,1,{0x1F}},
{0x52,1,{0x1F}},
{0x53,1,{0x1F}},
{0x54,1,{0x1F}},
{0x55,1,{0x1F}},
{0x56,1,{0x1F}},
{0x57,1,{0x1F}},

{0x58,1,{0x40}},
{0x59,1,{0x00}},
{0x5A,1,{0x00}},
{0x5B,1,{0x30}},
{0x5C,1,{0x09}},
{0x5D,1,{0x30}},
{0x5E,1,{0x01}},
{0x5F,1,{0x02}},
{0x60,1,{0x30}},
{0x61,1,{0x03}},
{0x62,1,{0x04}},
{0x63,1,{0x1C}},
{0x64,1,{0x6A}},
{0x65,1,{0x75}},
{0x66,1,{0x0d}},
{0x67,1,{0x72}},
{0x68,1,{0x0c}},
{0x69,1,{0x1C}},
{0x6A,1,{0x6A}},
{0x6B,1,{0x08}},
{0x6C,1,{0x00}},
{0x6D,1,{0x00}},
{0x6E,1,{0x00}},
{0x6F,1,{0x88}},
{0x70,1,{0x00}},
{0x71,1,{0x00}},
{0x72,1,{0x06}},
{0x73,1,{0x7B}},
{0x74,1,{0x00}},
{0x75,1,{0xBB}},
{0x76,1,{0x01}},
{0x77,1,{0x0D}},
{0x78,1,{0x24}},
{0x79,1,{0x00}},
{0x7A,1,{0x00}},
{0x7B,1,{0x00}},
{0x7C,1,{0x00}},
{0x7D,1,{0x03}},
{0x7E,1,{0x7B}},

{0xE0,1,{0x04}},
{0x00,1,{0x0E}}, 
{0x02,1,{0xB3}}, 	
{0x0E,1,{0x4A}},


{0xE0,1,{0x00}},
{0x35,1,{0x00}},

	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 200,{}},
	{0x29,1,{0x00}},	
	{REGFLAG_DELAY, 20,{}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#else

#endif

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

			case REGFLAG_DELAY:
				if (table[i].count <= 10)
					MDELAY(table[i].count);
				else
					MDELAY(table[i].count);
				break;

			case REGFLAG_UDELAY:
				UDELAY(table[i].count);
				break;

			case REGFLAG_END_OF_TABLE:
				break;

			default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

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

//		params->physical_width = 108;
//		params->physical_height = 172;
		// enable tearing-free
//		params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
//		params->dbi.te_edge_polarity = LCM_POLARITY_RISING;		
		
        #if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
        #else
		params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
        #endif
	
		// DSI
		/* Command mode setting */
		//1 Three lane or Four lane
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;


		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
//		params->dsi.word_count=720*3;	
		
		
///CHANGE BY Q 20131105
		params->dsi.vertical_sync_active				= 4;	//  
		params->dsi.vertical_backporch					= 12;    //14
		params->dsi.vertical_frontporch					= 12;	//14;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 24;	//20;
		params->dsi.horizontal_backporch				= 24;	//50
		params->dsi.horizontal_frontporch				= 30;	//50

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		//params->dsi.compatibility_for_nvk = 0;
        params->dsi.PLL_CLOCK = 207; // 260 
		params->dsi.clk_lp_per_line_enable = 1;
		params->dsi.ssc_disable = 1;
		params->dsi.cont_clock= 0;
		
				
		params->dsi.esd_check_enable = 0;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
		params->dsi.lcm_esd_check_table[0].count        = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

}


static void lcm_init(void)
{
	
    mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	MDELAY(10);	
	
	mt_set_gpio_mode(GPIO_LCM_LDO_1V8, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_LDO_1V8, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_LDO_1V8, GPIO_OUT_ONE);
	
	mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
	MDELAY(10);	

    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
	MDELAY(10);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
	MDELAY(2);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
	MDELAY(10);

//	SET_RESET_PIN(0);
//	SET_RESET_PIN(1);
//	MDELAY(10);///1
//	SET_RESET_PIN(0);
//	MDELAY(10);//10
//	SET_RESET_PIN(1);
//	MDELAY(10);//10
#if 1
	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
#else	

#endif
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(0);
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init();
}

extern void DSI_clk_HS_mode(char enter);
static unsigned int lcm_compare_id(void)
{

	int array[4];
	char buffer[5];
	char id_high=0;
	char id_low=0;
	int id=0;
	int ret = 0;

    mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	MDELAY(10);	
	
	mt_set_gpio_mode(GPIO_LCM_LDO_1V8, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_LDO_1V8, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_LDO_1V8, GPIO_OUT_ONE);
	
	SET_RESET_PIN(1);
	MDELAY(20); 
	SET_RESET_PIN(0);
	MDELAY(20); 	  
	SET_RESET_PIN(1);
	MDELAY(120);	  
   
//return 1;
	MDELAY(10);

	array[0]=0x00033700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer,3);
	id_high = buffer[0]; 		//93
	id_low = buffer[1]; 		 //65
	id =(id_high << 8) | id_low;
//return 1;
#ifdef BUILD_LK
	printf("zrl_lk -- jd9365da 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#else
	printk("zrl_lk -- jd9365da 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#endif
	return ((id ==0x9365))?1:0;

}
LCM_DRIVER jd9365da_wxga_dsi_vdo_ivo_siqiang_lcm_drv = 
{
    .name			= "jd9365da_wxga_dsi_vdo_ivo_siqiang",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,

};

