/*
 *  linux/arch/arm/mach-armlguest/driver/arm/switcher.S
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*P:900
 * This is the Switcher: code which sits  astride both the Host and Guest 
 * to do the low-level Guest<->Host switch.  
 :*/


/*S:100
 * Welcome to the Switcher itself!
 *
 * This file contains the low-level code which changes the CPU to run the Guest
 * code, and returns to the Host when something happens.  Understand this, and
 * you understand the heart of our journey.
 */



#include <asm/asm-offsets.h>
#include <linux/linkage.h>
#include <mach/entry-macro.S>
#include <asm/memory.h>
#include <asm/ptrace.h>
#inlcude <asm/tlbflush.h>
#include <asm/lguest.h>

#include "switcher-header.S"

#define HOST_IRQ_ENTRY (CONFIG_VECTORS_BASE + 0x18)
/*
 * Both the Guest kernel and user applications run under USER mode. But 
 * when exceptions happen, the Guest comes back in svc mode.  The Guest's 
 * svc stack is located in the 1st page of struct lguest_pages. 
 */

.text
/*
 * The following is entries which the Guest return to the Host.
 * when a abort, an interrupt, or system call happens, the Guest
 * will jump at the relative one entry of the following.
 */
ENTRY(start_switcher_text)
	b lguest_gsyscall
	b lguest_undef
	b lguest_hcall
	b lguest_pabt
	b lguest_dabt
	b lguest_hcall
	b lguest_irq

/*
 * Now we are ready to go to the Guest. We should 
 * save the context of the Host first, then we setup
 * the context for the Guest.
 */
ENTRY(switch_to_guest)
	@
	@ r0 = the address of struct lguest_pages
	@ We save the Host state on the 2nd page of struct lguest_pages
	@
	add r0, r0, #HOST_OFFSET 

	stmib r0, {r1 - lr}		@ save the Host svc r1 - lr	
	str lr, [r0, #S_PC]		

	mrs r2, cpsr				
	str r2, [sp, #S_PSR]	@ save the Host cpsr


	@ save the Host mmu information on 2nd page of struct lguest_pages
	save_host_mmu r0, r2, r3, r4, r5, r6, r7, r8

	@
	@ Now we setup hardware context for the Guest, the Guest information 
	@ is saved on the 1st page of struct lguest_pages
	@ r0 = address of 2nd page of struct lguest_pages
	@ r5 = address of 1st page of struct lguest_pages	
	@
	sub r5, r0, #HOST_OFFSET

	@
	@ Save usr_sp of the Host. we don't need to setup usr_sp 
	@ for the Guest here. the Guest will do it before it exits 
	@ svc mode.
	@
	add r2, r0, #LGUEST_PAGES_host_usrstack   @ get the Host usr_sp
	stmia r2, {sp}^

	@
	@ we save exception SPs of the Host and
	@ setup exception SPs for the Guest
	@
	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	str sp, [r0, #LGUEST_PAGES_host_abtstack]   @ save the Host abt_sp
	add sp, r5, #LGUEST_PAGES_guest_abt_sp      @ setup the Guest abt_sp

	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	str sp, [r0, #LGUEST_PAGES_host_undstack]   @ save the Host und_sp
	add sp, r5, #LGUEST_PAGES_guest_und_sp      @ setup the Guest und_sp

	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	str sp, [r0, #LGUEST_PAGES_host_irqstack]   @ save the Host irq_sp
	add sp, r5, #LGUEST_PAGES_guest_irq_sp      @ setup the Guest irq_sp

	@ return to svc mode
	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)

	@
	@ Everytime before switching between the Host and the Guest, we have to
	@ flush and invalid the cache of the first page of struct lguest_pages.
	@ but I arm not very sure why we have to do this.
	@
	flush_lguest_pages_cache r1, r2, r3, r4, r6

	@
	@ Now we setup mmu for the Guest. 	
	@ r1 pointer to "struct lguest_regs"
	@
	setup_guest_mmu r1, r2, r3, r4, r6, r7, r8, r9

	ldr r1, [r5, #S_PSR]
	msr spsr_cxsf, r1				@ prepare  for the Guest
	ldmia r5, {r0 - pc}^			@ load r0 - pc, cpsr
ENDPROC(switch_to_guest)




/*
 * We save all the Guest's context before we switch to the Host
 */
.macro save_guest_context
	@
	@	We need to save the Guest context on the 1st page of struct lguest_pages
	@
	str r0, [sp, #-4]	@ save r0 temporarily

	@
	@ the top of the Guest's svc stack at the end of
	@ the 1st struct lguest_pages.
	@
	mov r0, sp, lsr #12
	mov r0, r0, lsl #12

	@
	@ r0 = address of 1st page of struct lguest_pages.
	@ Save the Guest's r1 - r14(svc).
	@
	stmib r0, {r1 - lr}	

	@ Save address which the Guest will start at when we return to the Guest
	str lr, [r0, #S_PC]		

	ldr lr, [sp, #-4]
	str lr,	[r0, #S_R0]		@ save the Guest real r0 

	mrs	lr, cpsr
	str lr, [r0, #S_PSR]	@ save calling cpsr on the Guest stack

	sub lr, sp, r0
	str lr, [r0, #LGUEST_PAGES_guest_sp_offset]

	@
	@ Save the Guest mmu information
	@
	save_guest_mmu r0, r2, r3, r4, r5, r6, r7, lr

	flush_lguest_pages_cache r0, r2, r3, r4, r6
.endm




/*
 * Before we return to the Host, we should setup stacks of all modes
 * for the Host
 */
.macro  setup_host_stacks, rh, rd
	add \rd, \rh, #LGUEST_PAGES_host_usrstack   @ get the Host usr_sp
	ldmia \rd, {sp}^

	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | ABT_MODE)
	ldr sp, [\rh, #LGUEST_PAGES_host_abtstack]   @ get the Host abt_sp

	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | UND_MODE)
	ldr sp, [\rh, #LGUEST_PAGES_host_undstack]   @ get the Host und_sp

	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | IRQ_MODE)
	ldr sp, [\rh, #LGUEST_PAGES_host_irqstack]   @ save the Host irq_sp
.endm


/*
 * We are here Becasue the Guest received a system call.
 * The host will do nothing about this.
 */
lguest_gsyscall:
	save_guest_context
	mov r1, #RET_GSYSCALL          @ Tell the Host why we come back

	@ r0 = 1st page of struct lguest_pages
	str r1, [r0, #LGUEST_PAGES_guest_retcode]

	b lguest_come_back
ENDPROC(lguest_gsyscall)



/*
 * We are here becasue the Guest issue a hypercall.
 */
lguest_hcall:
	save_guest_context

	mov	r1, #RET_HCALL			@ Tell the Host why we come back

	@ r0 = 1st page of struct lguest_pages
	str r1, [r0, #LGUEST_PAGES_guest_retcode]

	b lguest_come_back
ENDPROC(lguest_hcall)



/*
 * We come back becasue a Prefetch Abort happened.
 */
lguest_pabt:
	@
	@  ABORT HANDLER can get IFAR and IFSR.
	@
	pabort_handler r0, r1	@	r0 = IFAR r1 = IFSR

	@
	@ Now, we save the Guest context on the 1st page of struct lguest_pages
	@
	save_guest_context

	mov	r1, #RET_PABT			@ Tell the Host why we come back 

	@ r0 = 1st page of struct lguest_pages
	str	r1, [r0, #LGUEST_PAGES_guest_retcode]	
	
	b lguest_come_back
ENDPROC(lguest_pabt)


/*
 * We come back becasue a Data Abort happened.
 */
lguest_dabt:
	@
	@ when we come back, we may handle the virtual irq first.
	@ We need to save registers on the the Guest stack, 
	@ so ABORT HANDLER can get FAR and FSR.
	@
	dabort_handler r0, r1	@	r0 = FAR r1 = FSR  

	@
	@ Now, we save the Guest context on the 1st page of struct lguest_pages
	@
	save_guest_context

	mov	r1, #RET_DABT			@ Tell the Host why we come back
 
	@ r0 = 1st page of struct lguest_pages
	str	r1, [r0, #LGUEST_PAGES_guest_retcode]	

	b lguest_come_back
ENDPROC(lguest_dabt)


/*
 * We come back becasue the Guest is attempting to execute 
 * an Undefined Instruction. The host will do nothing about this. 
 */
lguest_undef:
	@ Now, we save the Guest context on the 1st page of struct lguest_pages
	save_guest_context

	mov r1, #RET_UNF          @ Tell the Host why we come back

	@ r0 = 1st page of struct lguest_pages
	str r1, [r0, #LGUEST_PAGES_guest_retcode]
	
	b lguest_come_back
ENDPROC(lguest_undef)

/*
 * All exeptions except for IRQ take the Guest back to the Host
 * through this function.
 */
lguest_come_back:
	@ r0 = 1st page of struct lguest_pages
	@ the Host state is on the 2nd page
	add r0, r0, #HOST_OFFSET
	ldr r1, [r0, #S_PSR]        @ get the Host cpsr
	msr spsr_cxsf, r1           @ prepare SVC MODE for the Host

	@ setup mmu for the Host
	setup_host_mmu r0, r2, r3, r4, r5, r6, r7, r8, r9

	@ retore the Host user and exception stacks
	setup_host_stacks r0, r2

	@
	@   After "setup_host_stacks, we are in IRQ mode
	@   we return to SVC mode
	@
	msr cpsr_c, #(PSR_F_BIT | PSR_I_BIT | SVC_MODE)

	ldmia   r0, {r0 - pc}^      @ load r0 - pc, cpsr
ENDPROC(lguest_come_back)




/*
 * A real headware interrupt happens, so we get here.
 */
lguest_irq:
	save_guest_context

	mov	r1, #RET_IRQ			@ Tell the Host why we come back 
	@ r0 = 1st page of struct lguest_pages
	str	r1, [r0, #LGUEST_PAGES_guest_retcode]	 

	@ the Host state is on the 2nd page
	add r0, r0, #HOST_OFFSET	

	@ setup mmu for the Host
	setup_host_mmu r0, r2, r3, r4, r5, r6, r7, r8, r9

	@
	@ Before we set the Host stacks, we restore the Host svc 
	@ registers r1 - lr.	
	@
	ldmib r0, {r1 - lr}	

	@ setup user and exception stacks for the Host
	setup_host_stacks r0, r2

	@
	@ Now we are under IRQ mode. and we setup a hardware context 
	@ which is exactly the same as the context which should be 
	@ when a hardware interrupt happenes.
	@ r0 = the 2nd page of struct lguest_pages.
	ldr r1, [r0, #S_PSR]
	msr spsr, r1

	@
	@ lr_irq should be address of next instruction
	@ to be executed + 4 when interrupt happened
	@   
	ldr lr, [r0, #S_PC]				
	add lr, lr, #4					
									
	@
	@ r0 - r2 are corrupted 
	@
	ldmia r0, {r0 - r2}
	@ Now we jump to exception vectors of the Host (IRQ)			    
	ldr pc, .HostIrqEntry			
ENDPROC(lguest_irq)





.HostIrqEntry:
	.long HOST_IRQ_ENTRY


ENTRY(end_switcher_text)
