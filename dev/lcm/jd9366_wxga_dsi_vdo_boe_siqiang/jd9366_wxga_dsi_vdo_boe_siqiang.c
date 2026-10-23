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
//Page0
{0xE0,1,{0x00}},
//--- PASSWORD  ----//
{0xE1,1,{0x93}},
{0xE2,1,{0x65}},
{0xE3,1,{0xF8}},
{0x80,1,{0x03}},
//--- Page1  ----//
{0xE0,1,{0x01}},
//Set VCOM
{0x00,1,{0x00}},
{0x01,1,{0xA9}},	//VCOM A8 AE AD
//Set VCOM_Reverse
{0x03,1,{0x00}},
{0x04,1,{0xA8}},	
//Set Gamma Power);W_D( VGMP);W_D(VGMN);W_D(VGSP);W_D(VGSN
{0x17,1,{0x00}},
{0x18,1,{0xB1}},
{0x19,1,{0x01}},
{0x1A,1,{0x00}},
{0x1B,1,{0xB1}},  //VGMN=0
{0x1C,1,{0x01}},            
//Set Gate Power
{0x1F,1,{0x3E}},     //VGH_R  = 15V                       
{0x20,1,{0x2D}},     //VGL_R  = -12V                      
{0x21,1,{0x2D}},     //VGL_R2 = -12V                      
{0x22,1,{0x7E}},     //PA[6]=0);W_D( PA[5]=0);W_D( PA[4]=0);W_D( PA[0]=0 
//SETPANEL
{0x37,1,{0x19}},	//SS=1);W_D(BGR=1
//SET RGBCYC
{0x38,1,{0x05}},	//JDT=101 zigzag inversion
{0x39,1,{0x00}},	//RGB_N_EQ1);W_D( modify 20140806
{0x3A,1,{0x01}},	//RGB_N_EQ2);W_D( modify 20140806
{0x3C,1,{0x78}},	//SET EQ3 for TE_H
{0x3D,1,{0xFF}},	//SET CHGEN_OFF);W_D( modify 20140827 
{0x3E,1,{0xFF}},	//SET CHGEN_OFF);W_D( modify 20140827 
{0x3F,1,{0xFF}},	//SET CHGEN_OFF2);W_D( modify 20140827
//{0x3E,1,{0x80}},	//SET CHGEN_OFF);W_D( modify 20140806 
//{0x3F,1,{0x80}},	//SET CHGEN_OFF2);W_D( modify 20140806
//Set TCON
{0x40,1,{0x06}},	//RSO=800 RGB
{0x41,1,{0xA0}},	//LN=640->1280 line
{0x43,1,{0x08}},
{0x44,1,{0x09}},
{0x45,1,{0x28}},
//--- power voltage  ----//
{0x55,1,{0x01}},	//DCDCM=1111
//{0x56,1,{0x01}},
{0x57,1,{0x89}},	//VGH_RT=100);W_D( Charge pump Ratio:3*AVDD-VCL
{0x58,1,{0x0A}},
{0x59,1,{0x0A}},	//VCL = -2.5V
{0x5A,1,{0x28}},	//VGH = 15V
{0x5B,1,{0x19}},	//VGL = -12V
//--- Gamma  ----//
{0x5D,1,{0x7C}}, 
{0x5E,1,{0x66}},
{0x5F,1,{0x56}},
{0x60,1,{0x49}},
{0x61,1,{0x44}},
{0x62,1,{0x34}},
{0x63,1,{0x37}},
{0x64,1,{0x1F}},
{0x65,1,{0x36}},
{0x66,1,{0x33}},
{0x67,1,{0x32}},
{0x68,1,{0x4F}},
{0x69,1,{0x3D}},
{0x6A,1,{0x43}},
{0x6B,1,{0x35}},
{0x6C,1,{0x30}},
{0x6D,1,{0x24}},
{0x6E,1,{0x13}},
{0x6F,1,{0x00}},
{0x70,1,{0x7C}},
{0x71,1,{0x66}},
{0x72,1,{0x56}},
{0x73,1,{0x49}},
{0x74,1,{0x44}},
{0x75,1,{0x34}},
{0x76,1,{0x37}},
{0x77,1,{0x1F}},
{0x78,1,{0x36}},
{0x79,1,{0x33}},
{0x7A,1,{0x32}},
{0x7B,1,{0x4F}},
{0x7C,1,{0x3D}},
{0x7D,1,{0x43}},
{0x7E,1,{0x35}},
{0x7F,1,{0x30}},
{0x80,1,{0x24}},
{0x81,1,{0x13}},
{0x82,1,{0x00}},
//Page2);W_D( for GIP
{0xE0,1,{0x02}},
//GIP_L Pin mapping
{0x00,1,{0x45}},//0x05
{0x01,1,{0x45}},//0x05  
{0x02,1,{0x44}},//0x04
{0x03,1,{0x44}},//0x04
{0x04,1,{0x47}},//0x07
{0x05,1,{0x47}},//0x07
{0x06,1,{0x46}},//0x06
{0x07,1,{0x46}},//0x06
{0x08,1,{0x40}},//0x00
{0x09,1,{0x1F}},//0x1F
{0x0A,1,{0x1F}},//0x1F
{0x0B,1,{0x1F}},//0x1F
{0x0C,1,{0x1F}},//0x1F
{0x0D,1,{0x1F}},//0x1F
{0x0E,1,{0x1F}},//0x1F
{0x0F,1,{0x41}},//0x01
{0x10,1,{0x1F}},//0x1F
{0x11,1,{0x1F}},//0x1F
{0x12,1,{0x1F}},//0x1F
{0x13,1,{0x1F}},//0x1F
{0x14,1,{0x1F}},//0x1F
{0x15,1,{0x1F}},//0x1F
//GIP_R Pin mapping
{0x16,1,{0x45}},
{0x17,1,{0x45}},
{0x18,1,{0x44}},
{0x19,1,{0x44}},
{0x1A,1,{0x47}},
{0x1B,1,{0x47}},
{0x1C,1,{0x46}},
{0x1D,1,{0x46}},
{0x1E,1,{0x40}},
{0x1F,1,{0x1F}},
{0x20,1,{0x1F}},
{0x21,1,{0x1F}},
{0x22,1,{0x1F}},
{0x23,1,{0x1F}},
{0x24,1,{0x1F}},
{0x25,1,{0x41}},
{0x26,1,{0x1F}},
{0x27,1,{0x1F}},
{0x28,1,{0x1F}},
{0x29,1,{0x1F}},
{0x2A,1,{0x1F}},
{0x2B,1,{0x1F}},
//GIP Timing  
{0x58,1,{0x40}},        // TGEQL);W_D( Initial porch
//{0x59,1,{0x00}},
//{0x5A,1,{0x00}},
{0x5B,1,{0x10}},       //STV number =4);W_D( STV_S0[10:8]
{0x5C,1,{0x06}},       //STV_S0[7:0] =
{0x5D,1,{0x20}},//60   //STV_W=4 );W_D( STV_S1=0);W_D(1h timing
{0x5E,1,{0x01}},       //STV_S2 =1);W_D(1h timing
{0x5F,1,{0x02}},       //STV_S3 =2);W_D(1h timing
//{0x60,1,{0x00}},     //ETV default
//{0x61,1,{0x01}},     //ETV default
//{0x62,1,{0x02}},     //ETV default
{0x63,1,{0x62}},       //SETV_ON
{0x64,1,{0x62}},       //SETV_OFF
//{0x65,1,{0x00}},    //ETV
//{0x66,1,{0x00}},    //ETV
{0x67,1,{0x32}},          //CKV0_NUM[3:0]=8);W_D( CKV0_W[3:0]=4
{0x68,1,{0x08}},   //0x06       
{0x69,1,{0x62}}, 
{0x6A,1,{0x66}}, //66 CLK Falling to Source Falling time 3us
{0x6B,1,{0x08}},  
{0x6C,1,{0x00}},
{0x6D,1,{0x04}}, //GEQ_GGND1
{0x6E,1,{0x04}}, //GEQ_GGND2
{0x6F,1,{0x88}},
//{0x70,1,{0x00}},
//{0x71,1,{0x00}},
//{0x72,1,{0x06}},
//{0x73,1,{0x7B}},
//{0x74,1,{0x00}},
//{0x75,1,{0x07}},
//{0x76,1,{0x00}},
//{0x77,1,{0x5D}},
//{0x78,1,{0x17}},
//{0x79,1,{0x1F}},
//{0x7A,1,{0x00}},
//{0x7B,1,{0x00}},
//{0x7C,1,{0x00}},
//{0x7D,1,{0x03}},
//{0x7E,1,{0x7B}},
//Page3
{0xE0,1,{0x03}},
{0x98,1,{0x3E}},	//From 2E to 3E);W_D( LED_ON
//Page4
{0xE0,1,{0x04}},  //ESD protect
{0x09,1,{0x10}},
{0x2B,1,{0x2B}},	
{0x2D,1,{0x03}},	
{0x2E,1,{0x44}},	
//Page0
{0xE0,1,{0x00}}, //Watch dog
{0xE6,1,{0x02}},
{0xE7,1,{0x06}},

	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 120,{}},
	{0x29,1,{0x00}},	
	{REGFLAG_DELAY, 5,{}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#else

static void init_karnak_fiti_kd_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
/*	
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0032D1500;
	dsi_set_cmdq(data_array, 1, 1);
*/	
	data_array[0] = 0x01E011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA9011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA8041500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0xB1181500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB11B1500;
	dsi_set_cmdq(data_array, 1, 1);	
	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);		
	
	data_array[0] = 0x3E1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x2D201500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x2D211500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x7E221500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x19371500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x05381500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x00391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x013A1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x783C1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x803E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x803F1500;
	dsi_set_cmdq(data_array, 1, 1);	
	
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x08431500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x09441500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x28451500;
	dsi_set_cmdq(data_array, 1, 1);
 /*
 data_array[0] = 0x354A1500;
	dsi_set_cmdq(data_array, 1, 1);
 MDELAY(1);*/
//--- power voltage  ----//	
	data_array[0] = 0x01551500;
	dsi_set_cmdq(data_array, 1, 1);
	
//	data_array[0] = 0x01561500;
//	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x89571500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x0A591500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x285A1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x195B1500;
	dsi_set_cmdq(data_array, 1, 1);
//--- Gamma  ----//	
	data_array[0] = 0x7C5D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x665E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x565F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49601500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44611500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x34621500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x37631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x36651500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33661500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x32671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4F681500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x3D691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x436A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x356B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x306C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x246D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x136E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7C701500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x66711500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x56721500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49731500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44741500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x34751500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x37761500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F771500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x36781500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33791500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x327A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4F7B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3D7C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x437D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x357E1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x307F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x24801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x13811500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00821500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
//GIP_L Pin mapping	
	data_array[0] = 0x45001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44021500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47051500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46061500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46071500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x40081500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x410F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x1F101500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F111500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F121500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F131500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F141500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F151500;
	dsi_set_cmdq(data_array, 1, 1);
//GIP_R Pin mapping	
	data_array[0] = 0x45161500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45171500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x44181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44191500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x471A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x471B1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x461C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x461D1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x401E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x1F201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F231500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F241500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41251500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F261500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F271500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F281500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F291500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2B1500;
	dsi_set_cmdq(data_array, 1, 1);
//GIP Timing 	
	data_array[0] = 0x40581500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x105B1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x065C1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x205D1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x015E1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x025F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x62631500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x62641500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x32671500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x08681500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x62691500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x666A1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x086B1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x046D1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x046E1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0] = 0x886F1500;
	dsi_set_cmdq(data_array, 1, 1);
	
	
//Page3	
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E981500;
	dsi_set_cmdq(data_array, 1, 1);
//Page4
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x032D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);
//Page0
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);
//SLP OUT	


	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	data_array[0] = 0x00291500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);

}	
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
		params->dsi.vertical_backporch					= 6;    //14
		params->dsi.vertical_frontporch					= 8;	//14;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 20;	//20;
		params->dsi.horizontal_backporch				= 20;	//50
		params->dsi.horizontal_frontporch				= 32;	//50

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		//params->dsi.compatibility_for_nvk = 0;
        params->dsi.PLL_CLOCK = 220; // 260 
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
	mt_set_gpio_mode(GPIO_LCM_LDO_1V8, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_LDO_1V8, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_LDO_1V8, GPIO_OUT_ONE);
	
	mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
	MDELAY(10);	
    mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
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
	init_karnak_fiti_kd_lcm();
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
   

	MDELAY(10);

	array[0]=0x00033700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer,3);
	id_high = buffer[0]; 		//93
	id_low = buffer[1]; 		 //66
	id =(id_high << 8) | id_low;
	
#ifdef BUILD_LK
	printf("zrl_lk -- jd9366 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#else
	printk("zrl_lk -- jd9366 0x%x , 0x%x, 0x%x \n", id_high, id_low, id);
#endif
	return ((id ==0x9366))?1:0;

}
LCM_DRIVER jd9366_wxga_dsi_vdo_boe_siqiang_lcm_drv = 
{
    .name			= "jd9366_wxga_dsi_vdo_boe_siqiang",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,

};

