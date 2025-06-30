//
// Created by Li shuang ( pseudonym ) on 2024-03-26 
// --------------------------------------------------------------
// | Note: This code file just for study, not for commercial use 
// | Contact Author: lishuang.mk@whu.edu.cn 
// --------------------------------------------------------------
//

/*loongarch hal使用*/

#pragma once

#define LOONGARCH_CSR_CRMD		    0x0	    /* Current mode info 当前模式信息 */
#define LOONGARCH_CSR_PRMD          0x1	    /* Previous mode info 例外前模式信息 */
#define LOONGARCH_CSR_EUEN		    0x2	    /* Extension unit enable 扩展部件使能 */
#define LOONGARCH_CSR_MISC          0x3	    /* Miscellaneous control 杂项控制 */
#define LOONGARCH_CSR_ECFG          0x4	    /* Exception config 例外配置 */
#define LOONGARCH_CSR_ESTAT         0x5	    /* Exception status 例外状态 */
#define LOONGARCH_CSR_ERAISE        0x6	    /* Exception return address 例外返回地址 */
#define LOONGARCH_CSR_BADV          0x7	    /* Bad virtual address 错误虚拟地址 */
#define LOONGARCH_CSR_BADI          0x8	    /* Bad instruction 错误指令 */
#define LOONGARCH_CSR_EENTRY        0x9	    /* Exception entry address 例外入口地址 */

#define LOONGARCH_CSR_CPUID		    0x20	/* CPU core id */
// 上下文保存寄存器

#define LOONGARCH_CSR_PRCFG1		0x21	/* Privilege config 1 */		 

#define LOONGARCH_CSR_PRCFG2		0x22	/* Config2 */
#define LOONGARCH_CSR_PRCFG3		0x23	/* Config3 */               

#define LOONGARCH_CSR_SAVE0		    0x30    /* Kscratch registers */

#define LOONGARCH_CSR_TID   		0x40	/* Timer ID */
#define LOONGARCH_CSR_TCFG          0x41    /* Timer config */

#define LOONGARCH_CSR_DMWIN0		0x180	/* 64 direct map win0: MEM & IF */
#define LOONGARCH_CSR_DMWIN1		0x181	/* 64 direct map win1: MEM & IF */
#define LOONGARCH_CSR_DMWIN2		0x182	/* 64 direct map win2: MEM */
#define LOONGARCH_CSR_DMWIN3		0x183	/* 64 direct map win3: MEM */

#define LOONGARCH_CSR_TLBEHI		0x11	/* TLB EntryHi */
#define LOONGARCH_CSR_PGDL          0x19
#define LOONGARCH_CSR_PGDH          0x1a
#define LOONGARCH_CSR_PGD           0x1b
#define LOONGARCH_CSR_PWCL          0x1c
#define LOONGARCH_CSR_PWCH          0x1d

#define LOONGARCH_CSR_TLBRENTRY		0x88	/* TLB refill exception entry */
#define LOONGARCH_CSR_TLBRBADV		0x89	/* TLB refill badvaddr */
#define LOONGARCH_CSR_TLBRSAVE		0x8b	/* KScratch for TLB

#define LOONGARCH_CSR_STLBPS        0x1e    


/* Direct Map window 0/1 */


