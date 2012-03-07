/*
 *  linux/arch/arm/mach-armlguest/kernel/proc-lguest.S
 *
 *  Copyright (C) 2009 Mingli Wu (myfavor_linux@msn.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hwcap.h>

#define CPU_MM_MMU_FLAGS  0   /* The Guest does not care about it*/
#define CPU_IO_MMU_FLAGS  0   /* The Guest does not care about it*/

/*
 * It seems to be safe to select 0xfff1ffff as the Guest's 
 * virtual processor ID.
 * please also see linux/arch/arm/include/asm/lguest.h and
 * linux/arch/arm/lguest/driver/arm/run_guest.c.
 */
#define LGUEST_CPU_ID_VALUE     0xfff1ffff
#define LGUEST_CPU_ID_MASK      0xfff1ffff

#ifdef CONFIG_CPU_V7
#define ELF_HWCAP  HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
#else
#error Sorry, ARM lguest has not been tested on other versions of ARM by now.
#endif



	.align	5



lguest_cpu_name:
	.ascii  "ARM LGUEST VRITUALProcessor"
	.align



    __INITDATA

	.type   lguest_cache_fns, #object
ENTRY(lguest_cache_fns)
	.long   lguest_flush_kern_cache_all
	.long   lguest_flush_user_cache_all
	.long   lguest_flush_user_cache_range
	.long   lguest_coherent_kern_cache_range
	.long   lguest_coherent_user_cache_range
	.long   lguest_flush_kern_dcache_area
	.long   lguest_dma_inv_cache_range
	.long   lguest_dma_clean_cache_range
	.long   lguest_dma_flush_cache_range
	.size   lguest_cache_fns, . - lguest_cache_fns



	__INIT

	.type   lguest_tlb_fns, #object
ENTRY(lguest_tlb_fns)
	.long   lguest_flush_user_tlb_range
	.long   lguest_flush_kern_tlb_range
	.long   lguest_tlb_flags
	.size   lguest_tlb_fns, . - lguest_tlb_fns


/*
 *	__lguest_setup
 *
 * We do nothing here, Host do it for Guest	
 */
__lguest_setup:
	mov	pc, lr				
ENDPROC(__lguest_setup)




	.type	lguest_processor_functions, #object
ENTRY(lguest_processor_functions)
	.word	lguest_data_abort
	.word	lguest_prefetch_abort
	.word	lguest_cpu_proc_init
	.word	lguest_cpu_proc_fin
	.word	lguest_reset
	.word	lguest_do_idle
	.word	lguest_dcache_clean_area
	.word	lguest_cpu_switch_mm
	.word	lguest_cpu_set_pte_ext
	.size	lguest_processor_functions, . - lguest_processor_functions

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"arm_lguest"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"lguest"
	.size	cpu_elf_name, . - cpu_elf_name
	.align
	
	


	.section ".proc.info.init", #alloc, #execinstr


	.type	__lguest_proc_info, #object
__lguest_proc_info:
	.long	LGUEST_CPU_ID_VALUE		@ Required ID value
	.long	LGUEST_CPU_ID_MASK		@ Mask for ID
	.long	CPU_MM_MMU_FLAGS
	.long	CPU_IO_MMU_FLAGS
	b	__lguest_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	ELF_HWCAP
	.long	lguest_cpu_name
	.long	lguest_processor_functions
	.long	lguest_tlb_fns
	.long	lguest_user_fns
	.long	lguest_cache_fns
	.size	__lguest_proc_info, . - __lguest_proc_info

