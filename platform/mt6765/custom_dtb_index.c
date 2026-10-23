/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2017. All rights reserved.
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

#include <odm_mdtbo.h>                  // for customized_get_odm_mdtbo_index()
#include <debug.h>                      // for dprintf()

/******************************************************************************
* ODM DTBO entry selection for Realme C21 / RMX3201 (project 20701)
*
* Stock dtbo.img holds 3 entries and their dt_table_entry.id values are
* 0, 1 and 2.  load_dtb_from_dtb_entry() walks the entries looking for
* (entry->id == hw_mdtbo_index); because the ids are a plain 0..2 sequence
* that lookup is an identity map, so whatever this function returns is used
* directly as the dtbo entry index.
*
*   entry 0 : platform_support_project = 132769
*             touchscreen@0 sits under &spi3 (/spi@11013000),
*             reset-gpio = GPIO174
*
*   entry 1 : platform_support_project = 20701   <-- Realme C21 / RMX3201
*             touchscreen@0 sits under &spi2 (/spi@11012000),
*             reset-gpio = GPIO178
*
*   entry 2 : platform_support_project = 132769, 20375..20379, 131962
*             touchscreen@0 sits under &spi3 (/spi@11013000),
*             reset-gpio = GPIO174
*
* The previous implementation derived the index from GPIO163/GPIO164.  That
* table is the MT6771 internal-project PCB-ID table (see the original note
* in git history) and those pads are not the C21 straps: on this board the
* read returns 0, entry 0 was selected, and the kernel received a DT whose
* touchscreen node lives on spi3 with reset GPIO174 while the real panel is
* wired to spi2 with reset GPIO178.  Result: every OPPO touch driver failed
* to read the chip id, and the camera / sensor / SCP nodes were taken from
* the wrong project as well.
*
* This LK targets the C21 only, so the entry index is pinned to 1.
*
* NOTE: the string "C21/20701: PCB_ID high" is grepped by
*       .github/workflows/build.yml after the build - keep it.
******************************************************************************/
int customized_get_odm_mdtbo_index(void)
{
	dprintf(INFO, "C21/20701: PCB_ID high, dtb idx = 1\n");
	return 1;
}
