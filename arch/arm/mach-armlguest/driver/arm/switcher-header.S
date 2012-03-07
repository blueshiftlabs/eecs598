@ rh = host state page pointer (2nd page of lguest_pages)/*
 *  linux/arch/arm/mach-armlguest/driver/arm/switcher-head.S
 *
 *  Copyright (C) 2009 Mingli Wu (myfavor_linux@msn.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define HOST_OFFSET 4096

#ifdef CONFIG_CPU_V7
@ please see arm/arm/mm/proc-v7.S
#define TTB_RGN_OC_WB   (3 << 3)
#define TTB_IRGN_WB     ((1 << 0) | (1 << 6))
#define TTB_FLAGS       (TTB_IRGN_WB|TTB_RGN_OC_WB)


/*
 * Save the Host's mmu context.
 */
.macro  save_host_mmu, rh, rd2, rd3, rd4, rd5, rd6, rd7, rd8
	@rh is the address of 2rd page of lguest_pages
	add \rd8, \rh, #LGUEST_PAGES_host_pgd0 
	mrc p15, 0, \rd2, c2, c0, 0			@ save host tranlation table pointer
	mrc	p15, 0, \rd3, c13, c0, 1		@ get host context ID
	mrc	p15, 0, \rd4, c13, c0, 3		@ save TLS register
	mrc	p15, 0, \rd5, c3, c0, 0			@ save domain register
	mrc p15, 0, \rd6, c1, c0, 0			@ save Control register	
	mrc p15, 0, \rd7, c1, c0, 2			@ save Coprocessor access control register
	stmia \rd8, {\rd2, \rd3, \rd4, \rd5, \rd6, \rd7}
.endm


/*
 * Setup the Host's mmu context.
 */
.macro  setup_host_mmu, rh, rd2, rd3, rd4, rd5, rd6, rd7, rd8, rd9
	@ rh = host state page pointer (2nd page of lguest_pages)
	mov \rd3, #0
	add \rd9, \rh, #LGUEST_PAGES_host_pgd0
	ldmia \rd9, {\rd2, \rd4, \rd5, \rd6, \rd7, \rd8}
	orr \rd2, \rd2, #TTB_FLAGS
	mcr p15, 0, \rd3, c13, c0, 1    @ set reserved context ID
	dsb
	isb
	mcr p15, 0, \rd3, c8, c7, 0     @ flush I,D TLBs
	dsb
	isb
	mcr p15, 0, \rd2, c2, c0, 0     @ set table translation pointer for host
	isb
	mcr p15, 0, \rd4, c13, c0, 1    @ set host context ID
	isb
	mcr p15, 0, \rd5, c13, c0, 3    @ set TLS register
	mcr p15, 0, \rd6, c3, c0, 0     @ Set domain register
	mcr p15, 0, \rd7, c1, c0, 0     @ set Control register
	mcr p15, 0, \rd8, c1, c0, 2     @ set Coprocessor access control register			
.endm


/*
 * Save the Guest's mmu context.
 */
.macro  save_guest_mmu, rg, rd2, rd3, rd4, rd5, rd6, rd7, rtemp
	@
	@ rg pointer to lguest_regs
	@ r2 = page table address
	@ r3 = context id
	@ r4 = TLS  r5 = DOMAIN
	@ r6 = copro r7 = control register
	@
   
	add \rtemp, \rg, #LGUEST_PAGES_guest_pgd0
	mrc p15, 0, \rd2, c2, c0, 0       @ guest page table pointer(pgd)
	mrc p15, 0, \rd3, c13, c0, 1      @ get guest context ID
	mrc p15, 0, \rd4, c13, c0, 3      @ save TLS register
	mrc p15, 0, \rd5, c3, c0, 0       @ save domain register
	mrc p15, 0, \rd6, c1, c0, 0       @ save Control register
	mrc p15, 0, \rd7, c1, c0, 2       @ save Coprocessor access control register

	stmia \rtemp, {\rd2, \rd3, \rd4, \rd5, \rd6, \rd7}
.endm


/*
 * Setup the Guest's mmu context.
 */
.macro  setup_guest_mmu, rg, rd2, rd3, rd4, rd6, rd7, rd8, rd9 
	@
	@ rg pointer to lguest_regs
	@ \r2 = page table address 
	@ \r4 = context id
	@ \r6 = TLS	\r7 = DOMAIN
	@
	add \rg, \rg, #LGUEST_PAGES_guest_pgd0
	ldmia \rg, {\rd2, \rd4, \rd6, \rd7, \rd8, \rd9}
	orr	\rd2, \rd2, #TTB_FLAGS 
	mov \rd3, #0
	
	mcr	p15, 0, \rd3, c13, c0, 1    @ set guest reserved context ID
	dsb
	isb
	mcr p15, 0, \rd3, c8, c7, 0     @ flush I,D TLBs
	dsb
	isb
	mcr	p15, 0, \rd2, c2, c0, 0     @ set guest table translation pointer for host
	isb
	mcr	p15, 0, \rd4, c13, c0, 1    @ set guest context ID
	isb
	mcr p15, 0, \rd6, c13, c0, 3    @ set TLS register
	mcr p15, 0, \rd7, c3, c0, 0     @ Set domain register
	mcr p15, 0, \rd8, c1, c0, 0     @ set Control register			
	mcr p15, 0, \rd9, c1, c0, 2     @ set Coprocessor access control register
.endm

/* defined in linux/arch/arm/mm/proc-macros.S, I just copy it here */
.macro  dcache_line_size, reg, tmp
	mrc p15, 1, \tmp, c0, c0, 0     @ read CSIDR
	and \tmp, \tmp, #7              @ cache line size encoding
	mov \reg, #16                   @ size offset
	mov \reg, \reg, lsl \tmp        @ actual cache line size
.endm

/*
 * Get the fault status and fault address of a Data Abort for the Guest.
 */
.macro dabort_handler, far, fsr
	clrex
	mrc p15, 0, \fsr, c5, c0, 0		@ get FSR
	mrc p15, 0, \far, c6, c0, 0		@ get FAR
.endm

/*
 * Get the fault status and fault address of a Prefetch Abort for the Guest.
 */
.macro pabort_handler, ifar, ifsr
	mrc p15, 0, \ifar, c6, c0, 2	@ get IFAR
	mrc p15, 0, \ifsr, c5, c0, 1    @ get IFSR
.endm


/* 
 * Everytime before switching between the Host and the Guest, we have to
 * flush and invalid the cache of the first page of struct lguest_pages.
 * but I arm not very sure why we have to do this. 
 */
.macro flush_lguest_pages_cache, rg, rstart, rend, rlsize, rtemp
	@ \rg = address of the first page of lguest_pages
	mov \rstart, \rg
	@ \rlsize = size of a cache line
	dcache_line_size \rlsize, \rtemp
	add \rend, \rstart, #4096

1:
	mcr p15, 0, \rstart, c7, c14, 1
	add \rstart, \rstart, \rlsize
	cmp \rstart, \rend
	blo 1b
	dsb
.endm

#else
#error Sorry, ARM lguest has not been tested on other versions of ARM by now.
#endif


