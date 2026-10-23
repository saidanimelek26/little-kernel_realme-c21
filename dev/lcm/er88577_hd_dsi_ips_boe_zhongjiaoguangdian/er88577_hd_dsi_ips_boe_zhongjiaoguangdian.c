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

#define LCM_ID_FT8006M       								(0x8006)

static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;

#define GPIO_LCM_LDO_3V3      GPIO17
#define GPIO_LCM_LDO_1V8      GPIO18

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)       (lcm_util.mdelay(n))
#define UDELAY(n)       (lcm_util.udelay(n))


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
/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (800)
#define FRAME_HEIGHT                                    (1280)

#ifndef MACH_FPGA
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

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
    {0xF7,4,{0x5A,0xA5,0x95,0x27}},
    {REGFLAG_DELAY, 10, {}},
};

static struct LCM_setting_table init_setting[] = {
{0xE0,2,{0xAB,0xBA}},
{0xE1,2,{0xBA,0xAB}},
{0xB0,1,{0x00}},
{0xB1,4,{0x10,0x01,0x47,0xFF}},
{0xB2,6,{0x0c,0x14,0x04,0x50,0x50,0x14}},
{0xB3,3,{0x56,0xD3,0x00}},
{0xB4,3,{0x22,0x30,0x04}},
{0xB5,1,{0x00}},
{0xB6,7,{0xB0,0x00,0x00,0x10,0x00,0x10,0x00}},
{0xB7,8,{0x0E,0x00,0xFF,0x08,0x08,0xFF,0xFF,0x00}},
{0xB8,7,{0x05,0x12,0x29,0x49,0x48,0x00,0x00}},
{0xB9,38,{0x7c,0x5f,0x4d,0x40,0x3c,0x2d,0x32,0x1d,0x37,0x37,0x39,0x59,0x49,0x53,0x46,0x43,0x36,0x25,0x06,0x7c,0x5f,0x4d,0x40,0x3c,0x2d,0x32,0x1d,0x37,0x37,0x39,0x59,0x49,0x53,0x46,0x43,0x36,0x25,0x06}},
{0xBA,8,{0x00,0x00,0x00,0x44,0x24,0x00,0x00,0x00}},
{0xBB,3,{0x76,0x00,0x00}},
{0xBC,2,{0x00,0x00}},
{0xBD,5,{0xFF,0x00,0x00,0x00,0x00}},
{0xBE,1,{0x00}},
{0xC0,16,{0x76,0x54,0x12,0x34,0x33,0x33,0x44,0x44,0x10,0x04,0x90,0x04,0x0F,0x00,0x00,0x00}},
{0xC1,10,{0x53,0x94,0x02,0x8D,0x10,0x04,0x90,0x04,0x54,0x00}},
{0xC2,12,{0x37,0x0B,0x0E,0x8B,0x0E,0x11,0x22,0x33,0x44,0x82,0x18,0x00}},
{0xC3,22,{0x9C,0x5C,0x05,0x05,0x07,0x07,0x1D,0x1D,0x23,0x02,0x22,0x22,0x02,0x0F,0x0F,0x0D,0x0D,0x13,0x13,0x11,0x11,0x00}},
{0xC4,22,{0x1C,0x1C,0x04,0x04,0x06,0x06,0x1D,0x1D,0x23,0x02,0x22,0x22,0x02,0x0E,0x0E,0x0C,0x0C,0x12,0x12,0x10,0x10,0x00}},
{0xC5,3,{0xE8,0x85,0x76}},
{0xC6,2,{0x21,0x21}},
{0xC7,22,{0x41,0x01,0x0D,0x11,0x09,0x15,0x19,0x4F,0x10,0xD7,0xCF,0x19,0x1B,0x1D,0x03,0x02,0x25,0x30,0x00,0x03,0xFF,0x00}},
{0xC8,6,{0x61,0x00,0x02,0x43,0x54,0x16}},
{0xC9,5,{0xA1,0x22,0xFF,0xC4,0x23}},
{0xCA,2,{0x4B,0x43}},
{0xCC,4,{0x2E,0x02,0x04,0x08}},
{0xCD,8,{0x0E,0x4B,0x4B,0x20,0x19,0x6B,0x06,0x83}},
{0xD0,3,{0x27,0x10,0x80}},
{0xD1,4,{0x00,0x0D,0xFF,0x0F}},
{0xD2,4,{0xE3,0x2B,0x38,0x00}},
{0xD4,11,{0x00,0x01,0x00,0x0E,0x04,0x44,0x08,0x10,0x00,0x07,0x00}},
{0xD5,1,{0x00}},
{0xD6,2,{0x00,0x00}},
{0xD7,4,{0x00,0x00,0x00,0x00}},
{0xE4,3,{0x08,0x55,0x03}},
{0xE6,8,{0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},
{0xE7,3,{0x00,0x00,0x00}},
{0xE8,7,{0xD5,0xFF,0xFF,0xFF,0x00,0x00,0x00}},
{0xE9,1,{0xFF}},
{0xF0,5,{0x12,0x03,0x20,0x00,0xFF}},
{0xF1,26,{0xA6,0xC8,0xEA,0xE6,0xE4,0xCC,0xE4,0xBE,0xF0,0xB2,0xAA,0xC7,0xFF,0x66,0x98,0xE3,0x87,0xC8,0x99,0xC8,0x8C,0xBE,0x96,0x91,0x8F,0xFF}},
{0xF3,1,{0x03}},
{0xF4,26,{0xFF,0xFE,0xFC,0xFA,0xF8,0xF4,0xF0,0xE8,0xE0,0xD0,0xC0,0xA0,0x80,0x7F,0x5F,0x3F,0x2F,0x1F,0x17,0x0F,0x0B,0x07,0x05,0x03,0x01,0x00}},
{0xF5,26,{0xFF,0xFE,0xFC,0xFA,0xF8,0xF4,0xF0,0xE8,0xE0,0xD0,0xC0,0xA0,0x80,0x7F,0x5F,0x3F,0x2F,0x1F,0x17,0x0F,0x0B,0x07,0x05,0x03,0x01,0x00}},
{0xF6,26,{0xFF,0xFE,0xFC,0xFA,0xF8,0xF4,0xF0,0xE8,0xE0,0xD0,0xC0,0xA0,0x80,0x7F,0x5F,0x3F,0x2F,0x1F,0x17,0x0F,0x0B,0x07,0x05,0x03,0x01,0x00}},
{0xF7,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xF8,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xF9,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
{0xFA,25,{0x00,0x84,0x12,0x21,0x48,0x48,0x21,0x12,0x84,0x69,0x69,0x5A,0xA5,0x96,0x96,0xA5,0x5A,0xB7,0xDE,0xED,0x7B,0x7B,0xED,0xDE,0xB7}},
{0xFB,23,{0x00,0x12,0x0F,0xFF,0xFF,0xFF,0x00,0x38,0x40,0x08,0x70,0x0B,0x40,0x19,0x50,0x21,0xC0,0x27,0x60,0x2D,0x00,0x00,0x0F}},

{0x11,0,{0x00}},             
{REGFLAG_DELAY, 120, {}},    
{0x29,0,{0x00}},             
{REGFLAG_DELAY, 20, {}},   
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
     // {0x26,1,{0x03}}, 
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	
    // Sleep Mode On
	{0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
  
//	{REGFLAG_DELAY, 250, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	{0x11,1,{0x00}},  // Sleep-Out
	{REGFLAG_DELAY, 120, {0}},	
	{0x29,1,{0x00}},  // Display On
	{REGFLAG_DELAY, 20, {0}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

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
	
	params->physical_width = 68;
	params->physical_height = 136;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
    params->dsi.mode = SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE; SYNC_PULSE_VDO_MODE SYNC_EVENT_VDO_MODE
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
	params->dsi.intermediat_buffer_num = 2; 
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active    = 4; //1; //20150824 , 1->2
	params->dsi.vertical_backporch      = 12; //3;
	params->dsi.vertical_frontporch     = 20; //7;
	params->dsi.vertical_active_line    = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active  = 20; //12;
	params->dsi.horizontal_backporch    = 80; //36; //45;
	params->dsi.horizontal_frontporch   = 80; //96; //50; //96;
	params->dsi.horizontal_active_pixel     = FRAME_WIDTH;
#if 0
    // ESD
    params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	//params->dsi.lcm_esd_check_table[0].cmd   = 0xAC;
	//params->dsi.lcm_esd_check_table[0].count = 1;
	//params->dsi.lcm_esd_check_table[0].para_list[0] = 0x00;		
#endif
	params->dsi.HS_TRAIL=20;
	// Bit rate calculation
	params->dsi.PLL_CLOCK = 250;
	//1 Every lane speed
	//params->dsi.pll_div1=0;		// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	//params->dsi.pll_div2=0;		// div2=0,1,2,3;div1_real=1,2,4,4	
	//params->dsi.fbk_div =9;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	

}

static void lcm_init_power(void)
{
}

static void lcm_suspend_power(void)
{
}

static void lcm_resume_power(void)
{
}

static unsigned int lcm_compare_id(void)
{
#if 1
	unsigned int id = 0;
	unsigned char buffer[3];

	unsigned int data_array[16];

	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(120);

	data_array[0]=0x00033902;
	data_array[1]=0x005A5AF0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]=0x00033902;
	data_array[1]=0x00A5A5F1;
	dsi_set_cmdq(data_array, 2, 1);

	MDELAY(10);

	data_array[0] = 0x00023700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, buffer, 3);
	id = buffer[0]<<16 | buffer[1]<<8 | buffer[2]; //we only need ID
#ifdef BUILD_LK
	printf("wangguiwu id==0x%02x 1\r\n",id);
#else
	printk("wangguiwu id==0x%02x 1\r\n",id);
#endif
 		return (id == 0x83102d)?1:0;
	//return 1;
#endif
	//return 1;
}

static void lcm_init(void)
{
	
	int ret = 0;

	SET_RESET_PIN(0);
	
	mt_set_gpio_mode(GPIO_LCM_LDO_3V3, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_LDO_3V3, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_LDO_3V3, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_mode(GPIO_LCM_LDO_1V8, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_LDO_1V8, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_LDO_1V8, GPIO_OUT_ONE);
#if 0
#ifndef MACH_FPGA
	/*config rt5081 register 0xB2[7:6]=0x3, that is set db_delay=4ms.*/
	ret = PMU_REG_MASK(0xB2, (0x3 << 6), (0x3 << 6));

	/* set AVDD 5.4v, (4v+28*0.05v) */
	/*ret = RT5081_write_byte(0xB3, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB3, 36, (0x3F << 0));
	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", 0xB3);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", 0xB3);

	/* set AVEE */
	/*ret = RT5081_write_byte(0xB4, (1 << 6) | 28);*/
	ret = PMU_REG_MASK(0xB4, 36, (0x3F << 0));
	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", 0xB4);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", 0xB4);

	/* enable AVDD & AVEE */
	/* 0x12--default value; bit3--Vneg; bit6--Vpos; */
	/*ret = RT5081_write_byte(0xB1, 0x12 | (1<<3) | (1<<6));*/
	ret = PMU_REG_MASK(0xB1, (1<<3) | (1<<6), (1<<3) | (1<<6));
	if (ret < 0)
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write error----\n", 0xB1);
	else
		LCM_LOGI("nt35695----tps6132----cmd=%0x--i2c write success----\n", 0xB1);

	MDELAY(15);

#endif
#endif
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(10);///1
	SET_RESET_PIN(0);
	MDELAY(30);//10

	SET_RESET_PIN(1);
	MDELAY(100);//10

	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	//push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);

}

static void lcm_resume(void)
{
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
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
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}



/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	} else {
		LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
		return FALSE;
	}
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];
	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A; /* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700; /* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	        && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A; /* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35521 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_setbacklight(unsigned int level)
{
	LCM_LOGI("%s,nt35521 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
	/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {    /* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;    /* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;  /* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;  /* disable video mode secondly */
	} else {        /* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;  /* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


LCM_DRIVER er88577_hd_dsi_ips_boe_zhongjiaoguangdian_lcm_drv = 
{
    .name			= "er88577_hd_dsi_ips_boe_zhongjiaoguangdian",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,

};

