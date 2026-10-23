#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/wait.h>
#endif

#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constantsq
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  										(800)
#define FRAME_HEIGHT 										(1280)

#define   LCM_DSI_CMD_MODE							0

#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFFFD   // END OF REGISTERS MARKER

// static unsigned int lcm_esd_test = FALSE;      ///only for ESD test

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define SET_3V3_PIN(v)	(lcm_util.set_gpio_lcd_enp_bias((v)))
#define SET_1V8_PIN(v)	(lcm_util.set_gpio_lcd_18v_bias((v)))

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
	/*
#if 1
{0xFF,3,{0x98,0x81,0x03}},
{0x01,1,{0x00}},
{0x02,1,{0x00}},
{0x03,1,{0x73}},
{0x04,1,{0xD7}},
{0x05,1,{0x00}},
{0x06,1,{0x08}},
{0x07,1,{0x11}},
{0x08,1,{0x00}},
{0x09,1,{0x3F}},
{0x0a,1,{0x00}},
{0x0b,1,{0x00}},
{0x0c,1,{0x00}},
{0x0d,1,{0x00}},
{0x0e,1,{0x00}},
{0x0f,1,{0x3F}},
{0x10,1,{0x3F}},
{0x11,1,{0x00}},
{0x12,1,{0x00}},
{0x13,1,{0x00}},
{0x14,1,{0x00}},
{0x15,1,{0x00}},
{0x16,1,{0x00}},
{0x17,1,{0x00}},
{0x18,1,{0x00}},
{0x19,1,{0x00}},
{0x1a,1,{0x00}},
{0x1b,1,{0x00}},
{0x1c,1,{0x00}},
{0x1d,1,{0x00}},
{0x1e,1,{0x40}},
{0x1f,1,{0x80}},
{0x20,1,{0x06}},
{0x21,1,{0x01}},
{0x22,1,{0x00}},
{0x23,1,{0x00}},
{0x24,1,{0x00}},
{0x25,1,{0x00}},
{0x26,1,{0x00}},
{0x27,1,{0x00}},
{0x28,1,{0x33}},
{0x29,1,{0x33}},
{0x2a,1,{0x00}},
{0x2b,1,{0x00}},
{0x2c,1,{0x00}},
{0x2d,1,{0x00}},
{0x2e,1,{0x00}},
{0x2f,1,{0x00}},
{0x30,1,{0x00}},
{0x31,1,{0x00}},
{0x32,1,{0x00}},
{0x33,1,{0x00}},
{0x34,1,{0x00}},
{0x35,1,{0x00}},
{0x36,1,{0x00}},
{0x37,1,{0x00}},
{0x38,1,{0x00}},
{0x39,1,{0x00}},
{0x3a,1,{0x00}}, 
{0x3b,1,{0x00}},
{0x3c,1,{0x00}},
{0x3d,1,{0x00}},
{0x3e,1,{0x00}},
{0x3f,1,{0x00}},
{0x40,1,{0x00}},
{0x41,1,{0x00}},
{0x42,1,{0x00}},
{0x43,1,{0x00}},
{0x44,1,{0x00}},
{0x50,1,{0x01}},
{0x51,1,{0x23}},
{0x52,1,{0x44}},
{0x53,1,{0x67}},
{0x54,1,{0x89}},
{0x55,1,{0xab}},
{0x56,1,{0x01}},
{0x57,1,{0x23}},
{0x58,1,{0x45}},
{0x59,1,{0x67}},
{0x5a,1,{0x89}},
{0x5b,1,{0xab}},
{0x5c,1,{0xcd}},
{0x5d,1,{0xef}},
{0x5e,1,{0x00}},
{0x5f,1,{0x0C}},
{0x60,1,{0x0C}},
{0x61,1,{0x0F}},
{0x62,1,{0x0F}},
{0x63,1,{0x0E}},
{0x64,1,{0x0E}},
{0x65,1,{0x06}},
{0x66,1,{0x07}},
{0x67,1,{0x0D}},
{0x68,1,{0x02}},
{0x69,1,{0x02}},
{0x6a,1,{0x02}},
{0x6b,1,{0x02}},
{0x6c,1,{0x02}},
{0x6d,1,{0x02}},
{0x6e,1,{0x0D}},    
{0x6f,1,{0x02}},
{0x70,1,{0x02}},
{0x71,1,{0x05}},
{0x72,1,{0x01}},
{0x73,1,{0x08}},
{0x74,1,{0x00}},
{0x75,1,{0x0C}},
{0x76,1,{0x0C}},
{0x77,1,{0x0F}},
{0x78,1,{0x0F}},
{0x79,1,{0x0E}},
{0x7a,1,{0x0E}},
{0x7b,1,{0x06}},
{0x7c,1,{0x07}},
{0x7d,1,{0x0D}},
{0x7e,1,{0x02}},
{0x7f,1,{0x02}},
{0x80,1,{0x02}},
{0x81,1,{0x02}},
{0x82,1,{0x02}},
{0x83,1,{0x02}},
{0x84,1,{0x0D}},
{0x85,1,{0x02}},
{0x86,1,{0x02}},
{0x87,1,{0x05}},
{0x88,1,{0x01}},
{0x89,1,{0x08}},
{0x8A,1,{0x00}},
{0xFF,3,{0x98,0x81,0x04}},
{0x6E,1,{0x3B}},       
{0x6F,1,{0x57}},       
{0x3A,1,{0x24}},       
{0x8D,1,{0x1F}},       
{0x87,1,{0xBA}},       
{0xB2,1,{0xD1}},
{0x88,1,{0x0B}},
{0x38,1,{0x01}},      
{0x39,1,{0x00}},
{0xB5,1,{0x07}},       
{0x31,1,{0x75}},
{0x3B,1,{0x98}},  			
{0xFF,3,{0x98,0x81,0x01}},
{0x22,1,{0x0A}},
{0x31,1,{0x09}},
{0x35,1,{0x07}},
{0x53,1,{0x7B}},
{0x55,1,{0x40}},
{0x50,1,{0x86}},
{0x51,1,{0x82}},
{0x60,1,{0x27}},
{0x62,1,{0x20}},
{0xA0,1,{0x00}},
{0xA1,1,{0x12}},
{0xA2,1,{0x20}},
{0xA3,1,{0x13}},
{0xA4,1,{0x14}},
{0xA5,1,{0x27}},
{0xA6,1,{0x1D}},
{0xA7,1,{0x1F}},
{0xA8,1,{0x7C}},
{0xA9,1,{0x1D}},
{0xAA,1,{0x2A}},
{0xAB,1,{0x6B}},
{0xAC,1,{0x1A}},
{0xAD,1,{0x18}},
{0xAE,1,{0x4E}},
{0xAF,1,{0x24}},
{0xB0,1,{0x2A}},
{0xB1,1,{0x4D}},
{0xB2,1,{0x5B}},
{0xB3,1,{0x23}},
{0xC0,1,{0x00}},
{0xC1,1,{0x13}},
{0xC2,1,{0x20}},
{0xC3,1,{0x12}},
{0xC4,1,{0x15}},
{0xC5,1,{0x28}},
{0xC6,1,{0x1C}},
{0xC7,1,{0x1E}},
{0xC8,1,{0x7B}},
{0xC9,1,{0x1E}},
{0xCA,1,{0x29}},
{0xCB,1,{0x6C}},
{0xCC,1,{0x1A}},
{0xCD,1,{0x19}},
{0xCE,1,{0x4D}},
{0xCF,1,{0x22}},
{0xD0,1,{0x2A}},
{0xD1,1,{0x4D}},
{0xD2,1,{0x5B}},
{0xD3,1,{0x23}},			
{0xFF,3,{0x98,0x81,0x00}},
{0x11,1,{0x00}},  
{0x29,1,{0x00}},
{0x35,1,{0x00}},
#else
{0xFF,3,{0x98,0x81,0x03}},
//GIP_1
{0x01,1,{0x00}},
{0x02,1,{0x00}},
{0x03,1,{0x57}},    //54    //57 STV0 Width=8H   //STVA=STV0
{0x04,1,{0xD3}},    //D4    //D3 STV1_2 Width=CLK Width    //STVB=STV1_2
{0x05,1,{0x00}},        //STVC
{0x06,1,{0x11}},        //STVA_Rise
{0x07,1,{0x08}},    //09        //STVB_Rise
{0x08,1,{0x00}},        //STVC_Rise
{0x09,1,{0x00}},        //FTI1R(A)
{0x0a,1,{0x3F}},    //00     //STV宽度跟CLK调整一样宽 //FTI2R(B)
{0x0b,1,{0x00}},        //FTI3R(C)
{0x0c,1,{0x00}},        //FTI1F(A)
{0x0d,1,{0x00}},        //FTI2F(B)
{0x0e,1,{0x00}},        //FTI2F(C)
{0x0f,1,{0x3F}},    //00     //3F Duty=42%    //CLW1(ALR) 
{0x10,1,{0x3F}},    //00     //3F Duty=42%    //CLW2(ARR)
{0x11,1,{0x00}},           
{0x12,1,{0x00}},        
{0x13,1,{0x00}},        //CLWX(ATF)
{0x14,1,{0x00}},
{0x15,1,{0x00}},        //GPMRi(ALR)
{0x16,1,{0x00}},        //GPMRii(ARR)
{0x17,1,{0x00}},        //GPMFi(ALF)
{0x18,1,{0x00}},        //GPMFii(AFF)
{0x19,1,{0x00}},
{0x1a,1,{0x00}},
{0x1b,1,{0x00}},   
{0x1c,1,{0x00}},
{0x1d,1,{0x00}},
{0x1e,1,{0x40}},        //CLKA 40笆は C0も笆は(X8把σCLKB)
{0x1f,1,{0x80}},        //C0
{0x20,1,{0x06}},        //CLKA_Rise
{0x21,1,{0x01}},        //CLKA_Fall
{0x22,1,{0x00}},        //CLKB_Rise(keep toggle惠砞CLK A)
{0x23,1,{0x00}},        //CLKB_Fall
{0x24,1,{0x00}},        //CLK keep toggle(AL) 8X┕オ
{0x25,1,{0x00}},        //CLK keep toggle(AR) 8X┕オ
{0x26,1,{0x00}},
{0x27,1,{0x00}},
{0x28,1,{0x33}},       //CLK Phase
{0x29,1,{0x33}},       //CLK overlap
{0x2a,1,{0x00}},  
{0x2b,1,{0x00}},
{0x2c,1,{0x00}},       //GCH R
{0x2d,1,{0x00}},       //GCL R 
{0x2e,1,{0x00}},       //GCH F        
{0x2f,1,{0x00}},       //GCL F
{0x30,1,{0x00}},
{0x31,1,{0x00}},
{0x32,1,{0x00}},       //GCH/L ext2/1︽  5E 01:31   5E 00:42
{0x33,1,{0x00}},
{0x34,1,{0x00}},       //VDD1&2 non-overlap 04:2.62us
{0x35,1,{0x00}},       //GCH/L 跋丁 00:VS玡 01:VS 10:阁VS 11:frameい       
{0x36,1,{0x00}},
{0x37,1,{0x00}},       //GCH/L
{0x38,1,{0x78}},	//VDD1&2 toggle 2sec
{0x39,1,{0x00}},
{0x3a,1,{0x00}}, 
{0x3b,1,{0x00}},
{0x3c,1,{0x00}},
{0x3d,1,{0x00}},
{0x3e,1,{0x00}},
{0x3f,1,{0x00}},
{0x40,1,{0x00}},
{0x41,1,{0x00}},
{0x42,1,{0x00}},
{0x43,1,{0x00}},       //GCH/L
{0x44,1,{0x00}},
//GIP_2
{0x50,1,{0x00}},
{0x51,1,{0x23}},
{0x52,1,{0x45}},
{0x53,1,{0x67}},
{0x54,1,{0x89}},
{0x55,1,{0xab}},
{0x56,1,{0x01}},
{0x57,1,{0x23}},
{0x58,1,{0x45}},
{0x59,1,{0x67}},
{0x5a,1,{0x89}},
{0x5b,1,{0xab}},
{0x5c,1,{0xcd}},
{0x5d,1,{0xef}},
//GIP_3
{0x5e,1,{0x00}},
{0x5f,1,{0x0D}},     //FW_CGOUT_L[1]    CLK8
{0x60,1,{0x0D}},     //FW_CGOUT_L[2]    CLK8
{0x61,1,{0x0C}},     //FW_CGOUT_L[3]    CLK6
{0x62,1,{0x0C}},     //FW_CGOUT_L[4]    CLK6
{0x63,1,{0x0F}},     //FW_CGOUT_L[5]    CLK4
{0x64,1,{0x0F}},     //FW_CGOUT_L[6]    CLK4
{0x65,1,{0x0E}},     //FW_CGOUT_L[7]    CLK2
{0x66,1,{0x0E}},     //FW_CGOUT_L[8]    CLK2
{0x67,1,{0x08}},     //FW_CGOUT_L[9]    STV2
{0x68,1,{0x02}},     //FW_CGOUT_L[10]   
{0x69,1,{0x02}},     //FW_CGOUT_L[11]     
{0x6a,1,{0x02}},     //FW_CGOUT_L[12]  
{0x6b,1,{0x02}},     //FW_CGOUT_L[13]   
{0x6c,1,{0x02}},     //FW_CGOUT_L[14]      
{0x6d,1,{0x02}},     //FW_CGOUT_L[15]   
{0x6e,1,{0x02}},     //FW_CGOUT_L[16]   VGL    
{0x6f,1,{0x02}},     //FW_CGOUT_L[17]   VGL
{0x70,1,{0x14}},     //FW_CGOUT_L[18]   VDDO
{0x71,1,{0x15}},     //FW_CGOUT_L[19]   VDDE
{0x72,1,{0x06}},     //FW_CGOUT_L[20]   STV0
{0x73,1,{0x02}},     //FW_CGOUT_L[21]   
{0x74,1,{0x02}},     //FW_CGOUT_L[22]   
{0x75,1,{0x0D}},     //BW_CGOUT_L[1]   
{0x76,1,{0x0D}},     //BW_CGOUT_L[2]    
{0x77,1,{0x0C}},     //BW_CGOUT_L[3]    
{0x78,1,{0x0C}},     //BW_CGOUT_L[4]    
{0x79,1,{0x0F}},     //BW_CGOUT_L[5]     
{0x7a,1,{0x0F}},     //BW_CGOUT_L[6]     
{0x7b,1,{0x0E}},     //BW_CGOUT_L[7]   
{0x7c,1,{0x0E}},     //BW_CGOUT_L[8]    
{0x7d,1,{0x08}},     //BW_CGOUT_L[9]      
{0x7e,1,{0x02}},     //BW_CGOUT_L[10]
{0x7f,1,{0x02}},     //BW_CGOUT_L[11]    
{0x80,1,{0x02}},     //BW_CGOUT_L[12]   
{0x81,1,{0x02}},     //BW_CGOUT_L[13] 
{0x82,1,{0x02}},     //BW_CGOUT_L[14]      
{0x83,1,{0x02}},     //BW_CGOUT_L[15]   
{0x84,1,{0x02}},     //BW_CGOUT_L[16]      
{0x85,1,{0x02}},     //BW_CGOUT_L[17]
{0x86,1,{0x14}},     //BW_CGOUT_L[18]
{0x87,1,{0x15}},     //BW_CGOUT_L[19]
{0x88,1,{0x06}},     //BW_CGOUT_L[20]   
{0x89,1,{0x02}},     //BW_CGOUT_L[21]   
{0x8A,1,{0x02}},     //BW_CGOUT_L[22]   
//CMD_Page 4
{0xFF,3,{0x98,0x81,0x04}},
{0x6E,1,{0x3A}},    //VGH 18V
{0x6F,1,{0x57}},           //reg vcl + pumping ratio VGH=4x VGL=-3x
{0x35,1,{0x1F}},    //HZ
{0x3A,1,{0x24}},           //POWER SAVING
{0x8D,1,{0x1F}},    //VGL -12V  1F =-12V
{0x87,1,{0xBA}},           //ESD  由BA改成0A
{0xB2,1,{0xD1}},
{0x88,1,{0x0B}},
{0x38,1,{0x01}},      
{0x39,1,{0x00}},
{0xB5,1,{0x07}},           //gamma bias
{0x31,1,{0x75}},
{0x3B,1,{0x98}},  
//CMD_Page 1
{0xFF,3,{0x98,0x81,0x01}},
{0x22,1,{0x0A}},          //BGR, SS
{0x31,1,{0x09}},          ///Column inversion
{0x35,1,{0x07}},          //
{0x53,1,{0x82}},          //7F 82 87  VCOM1
{0x55,1,{0x84}},          //VCOM2 
{0x50,1,{0x85}},    // 4.308V  //95-> 4.5V //VREG1OUT
{0x51,1,{0x80}},    //-4.308V  //90->-4.5V //VREG2OUT
{0x60,1,{0x10}},          //SDT=2.8  由27，改为10
{0x62,1,{0x00}},          //由20，改为00
{0x42,1,{0x55}},    //pump
//============Gamma START=============
{0xA0,1,{0x0C}},	
{0xA1,1,{0x16}},	
{0xA2,1,{0x23}},	
{0xA3,1,{0x12}},	
{0xA4,1,{0x14}},	
{0xA5,1,{0x27}},	
{0xA6,1,{0x1C}},	
{0xA7,1,{0x1D}},	
{0xA8,1,{0x7E}},	
{0xA9,1,{0x1D}},	
{0xAA,1,{0x29}},	
{0xAB,1,{0x6C}},	
{0xAC,1,{0x19}},	
{0xAD,1,{0x16}},	
{0xAE,1,{0x49}},	
{0xAF,1,{0x20}},	
{0xB0,1,{0x27}},	
{0xB1,1,{0x50}},	
{0xB2,1,{0x64}},	
{0xB3,1,{0x3F}},	
//Neg Register
{0xC0,1,{0x00}},	
{0xC1,1,{0x16}},	
{0xC2,1,{0x23}},	
{0xC3,1,{0x12}},
{0xC4,1,{0x14}},
{0xC5,1,{0x27}},
{0xC6,1,{0x1C}},
{0xC7,1,{0x1D}},
{0xC8,1,{0x7F}},
{0xC9,1,{0x1D}},	
{0xCA,1,{0x29}},	
{0xCB,1,{0x6C}},
{0xCC,1,{0x19}},
{0xCD,1,{0x16}},
{0xCE,1,{0x49}},	
{0xCF,1,{0x20}},
{0xD0,1,{0x27}},	
{0xD1,1,{0x50}},
{0xD2,1,{0x64}},	
{0xD3,1,{0x3F}},	
//============ Gamma END===========			
			
//CMD_Page 0			
{0xFF,3,{0x98,0x81,0x00}},
{0x35,1,{0x00}},
#endif
*/
	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 120,{}},
	{0x29,1,{0x00}},	
	{REGFLAG_DELAY, 50,{}},	
//	{REGFLAG_END_OF_TABLE, 0x00, {} }	

};

static void init_lcm_registers(void)
{
   	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
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
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
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
		
		
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 8;
		params->dsi.vertical_frontporch					= 16;		
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 
		params->dsi.horizontal_sync_active				= 18;
		params->dsi.horizontal_backporch				= 18;
		params->dsi.horizontal_frontporch				= 18;

		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
		// params->dsi.compatibility_for_nvk = 0;
        params->dsi.ssc_disable = 1;

		params->dsi.esd_check_enable = 0;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
		params->dsi.lcm_esd_check_table[0].count        = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
		
		//params->dsi.HS_TRAIL=20;	
        params->dsi.PLL_CLOCK = 250;


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


static unsigned int lcm_compare_id(void)
{
	unsigned int id=0;
	unsigned char buffer[3];
	unsigned int array[16]; 
	unsigned int  id_high=0;
	unsigned int  id_low=0;
	
	struct LCM_setting_table switch_table_page1[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x01} }
	};
	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};	
	
	mt_set_gpio_mode(GPIO_LCM_PWR2_EN, GPIO_MODE_00);	//3.3
	mt_set_gpio_dir(GPIO_LCM_PWR2_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);	//1.8
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);	
	
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);
	
	push_table(switch_table_page1, sizeof(switch_table_page1) / sizeof(struct LCM_setting_table), 1);
	
	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);	
	
	read_reg_v2(0x00, buffer, 1);
	id_high = buffer[0]; 	/* we only need ID */

	read_reg_v2(0x01, buffer, 1);
	id_low = buffer[0];
	
	read_reg_v2(0x02, buffer, 1);
	id = buffer[0];

	printf("%s,ili9881c_id_high=0x%x,id_low=0x%x,id=0x%x\n", __func__, id_high, id_low,id);	
	
	push_table(switch_table_page0, sizeof(switch_table_page0) / sizeof(struct LCM_setting_table), 1);		

	if (id_high == 0x98 && id_low == 0x81)
		return 1;
	else
		return 0;
}

static void lcm_init(void)
{
	mt_set_gpio_mode(GPIO_LCM_PWR2_EN, GPIO_MODE_00);	//3.3
	mt_set_gpio_dir(GPIO_LCM_PWR2_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);	//1.8
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);	
		
    SET_RESET_PIN(1);
    MDELAY(50); //20130924  add 
    SET_RESET_PIN(0);
    MDELAY(10);//Must > 5ms   //6 20130924  changed
    SET_RESET_PIN(1);
    MDELAY(50);//Must > 50ms	//50 20130924  changed
   
   init_lcm_registers();
//	lcm_compare_id();
}


static void lcm_resume(void)
{
	lcm_init();
}

LCM_DRIVER ili9881c_wxga_vdo_boe_baileqi_lcm_drv = 
{
    .name			= "ili9881c_wxga_vdo_boe_baileqi",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
};

