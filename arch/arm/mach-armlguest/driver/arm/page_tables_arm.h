/*
 *  linux/arch/arm/mach-armlguest/driver/arm/page_table_arm.h
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 *  This code is based on the linux/driver/lguest/x86/core.c,
 *  written by Rusty Russell.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#ifndef __PAGE_TABLE_ARM_H_
#define __PAGE_TABLE_ARM_H_
#include <asm/pgtable.h>
#include <asm/pgalloc.h>


#define GUEST_GPMD_KERNEL_FLAGS \
	(PMD_SECT_WT | PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_DOMAIN(DOMAIN_KERNEL))


#define GUEST_BASE_PTE_FLAGS (L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_MT_WRITETHROUGH)

#define BOOT_PARAMS_OFFSET 0X100

#ifdef CONFIG_CPU_V7
/* setup guest shadow pagetable entry*/
static inline void set_guest_pte(pte_t *ptep, pte_t pte, unsigned long ext)
{
	pte_t *pt;
	pte_t temp;

	/* Set the linux versrion PTE*/
	*ptep = pte;

	/* Get the address of the hardware version PTE*/
	pt = (pte_t*)((unsigned long)ptep - 2048);

	temp = pte & ~(0x000003f0 | PTE_TYPE_MASK);
	
	/* we initialize the PTE as RW and PAGE TYPE*/
	temp |= ext | PTE_EXT_AP0 | PTE_EXT_AP1 | 2;

	/* 
	 * it seems that this is not necessary for ARMv7.
	 * Please see linux/arch/arm/mm/mmu.c
	 */ 
	if(pte & (1 << 4))
		temp |= PTE_EXT_TEX(1);


	/* Set the PTE as read only unless both "present" bit and "dirty" bit are set" */
	if(!(pte & L_PTE_WRITE) || (!(pte & L_PTE_DIRTY))){
		temp &= ~PTE_EXT_AP0;
	}

	/* This area is allowed to execute or not. */
	if(!(pte & L_PTE_EXEC)){
		temp |= PTE_EXT_XN;
	}

	/* Clear the PTE unless both "young" bit and "present" bit are set */
	if(!(pte & L_PTE_YOUNG) || !(pte & L_PTE_PRESENT)){
		temp = 0;
	}
	*pt = temp;
}
#else //CONFIG_CPU_V7
#error Sorry now ARM lguest only suppor the ARMv7 processor
#endif //CONFIG_CPU_V7


#endif //__PAGE_TABLE_ARM_H_ 
