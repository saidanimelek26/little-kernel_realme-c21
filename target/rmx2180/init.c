/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <debug.h>
#include <reg.h>
#include <target.h>
#include <platform/mt_reg_base.h>
#include <platform/boot_mode.h>
#include <target/cust_key.h>
#include <platform/mt_gpt.h>
#include <platform/mt_rtc.h>
#include <platform/mtk_key.h>
#include <platform/mt_gpio.h>

extern void dmb(void);

static void target_detect(void);

void target_early_init(void)
{

}

void target_init(void)
{
}

unsigned board_machtype(void)
{
  return MACH_TYPE;
}

/* Do any target specific intialization needed before entering fastboot mode */
void target_fastboot_init(void)
{

}

extern BOOTMODE g_boot_mode;

/*
 * The C21 DTBO describes kpd_volume_down as a GPIO94 falling-edge eint.
 * LK's mtk_detect_key() only reads the keypad KP_MEM registers, so that
 * volume-down key is invisible to fastboot_trigger().  Read GPIO94
 * directly as well (active low, pulled up).
 */
#define C21_VOL_DOWN_EINT_GPIO	GPIO94

static void c21_vol_down_gpio_init(void)
{
  mt_set_gpio_dir(C21_VOL_DOWN_EINT_GPIO, GPIO_DIR_IN);
  mt_set_gpio_pull_enable(C21_VOL_DOWN_EINT_GPIO, GPIO_PULL_ENABLE);
  mt_set_gpio_pull_select(C21_VOL_DOWN_EINT_GPIO, GPIO_PULL_UP);
}

BOOL fastboot_trigger(void)
{
  ulong begin = get_timer(0);
  dprintf(INFO,"\n Check FASTBOOT\n");

/*Check Fastboot RTC bit*/
#if 1
  if(Check_RTC_PDN1_bit13())
  {
    dprintf(INFO,"[FASTBOOT] reboot to boot loader\n");
    g_boot_mode = FASTBOOT;
    Set_Clr_RTC_PDN1_bit13(false);
   	 return TRUE;
  }
#endif

  c21_vol_down_gpio_init();

  dprintf(INFO,"Wait 500ms for special keys\n");
  
  while(get_timer(begin)<500)
  {    
    if(mtk_detect_key(MT_CAMERA_KEY) ||
       mtk_detect_key(MT65XX_BOOT_MENU_KEY) ||
       (mt_get_gpio_in(C21_VOL_DOWN_EINT_GPIO) == 0))
    { 
   	  dprintf(INFO,"[FASTBOOT]Key Detect (vol key)\n");
   	  g_boot_mode = FASTBOOT;
   	  return TRUE;
    }
  }
        
  return FALSE;	
}

