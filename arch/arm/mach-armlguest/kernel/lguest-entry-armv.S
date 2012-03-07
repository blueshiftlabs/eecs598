/*
 *  linux/arch/arm/mach-armlguest/kernel/lguest-entry-armv.S
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 *	This code is based on the linux/arch/arm/kernel/entry-armv.S, 
 *	written by Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/memory.h>
#include <asm/glue.h>
#include <asm/vfpmacros.h>
#include <mach/entry-macro.S>
#include <asm/thread_notify.h>
#include <asm/unistd.h>
#include <asm/thread_info.h>

#include <asm/ptrace.h>
#include <asm/system.h>



#include <asm/unwind.h>


#include <asm/lguest_hcall.h>
#include "lguest-entry-header.S"

#define USER_MODE 0
#define KERNEL_MODE 1
.text


/* Get switcher address the jump at it */
.macro guest_to_switcher, rd, offset = 0 
	ldr \rd, .LguestSwitcherAddr	
	ldr \rd, [\rd]
	add \rd, \rd, #\offset
	blx \rd
.endm



.macro switcher_to_guest
/*
 * switcher code may put far, fsr, or ifar, ifsr in the r0, r1
 * for the Guest's data abort handler and prefech abort handler
 */
	stmia sp, {r0 - r12}
    
	mov r5, sp, lsr #12
	mov r5, r5, lsl #12
	ldr r6, .LguestPageBase
	str r5, [r6]

	@
	@ Restore r0-sp for the Guest Kernel
	@ 
	ldmia sp, {r0 - sp}^          
	@ Adjuest the svc stack pointer(the Switcher stack);
	add sp, sp, #S_FRAME_SIZE

	@ Set the CPSR for the Guest Kernel.
	msr cpsr_c, #(PSR_F_BIT | USR_MODE)  
.endm



/*
 * kernel entry handlers
 */

#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5)
#define SPFIX(code...) code
#else
#define SPFIX(code...)
#endif

    .macro  kernel_entry
 UNWIND(.fnstart        )
 UNWIND(.save {r0 - pc}     )
	sub sp, sp, #S_FRAME_SIZE
	stmib sp, {r1 - sp}^  
	
	@ Get the Guest kernel stack pointer.
	ldr lr, [sp, #S_SP]		
	sub lr, lr, #(S_FRAME_SIZE - 4)  
 SPFIX( tst lr, #4      )
 SPFIX( subeq   lr, lr, #4  )
	@ Save calling r1 - lr on the Guest kernel stack.
	stmia lr, {r1 - lr}^		@ save calling r1 - lr on guest kernel stack

	ldmia r0, {r1 - r3}
	
	str r1, [lr, #-4]!      @ save the "real" r0 copied
			                @ from the exception stack
	str lr, [sp, #S_SP]		@ save the Guest kernel stack pointer on svc stack
	str r3, [sp, #S_PSR]	@ r3 - spsr_<exception>

	mov r4, #-1
	add r5, lr, #S_PC

	@
	@ We are now ready to fill in the remaining blanks on the stack:
	@
	@  r2 - lr_<exception>, already fixed up for correct return/restart
	@  r3 - spsr_<exception>
	@  r4 - orig_r0 (see pt_regs definition in ptrace.h)
	@
	stmia   r5, {r2 - r4}
	.endm



	.align	5
__lguest_dabt_kernel:
	kernel_entry
	@
	@ We go back to the Host and let the host
	@ handle it first.
	@ When we come back, the Host have already put the aborted 
	@ address in r0, and the fault status register in r1.  
	@
	guest_to_switcher lr, SWITCHER_DABT_OFFSET

	switcher_to_guest


	@
	@ If r4 == 0, the Host have already solved the problem.
	@ Otherwise, let do_DataAbort handle it
	@
	cmp r4, #0
	movne r2, sp
	blne  do_DataAbort

	
	@ 
	@ Let us check if there are some virtual irqs which need 
	@ to be handled.
	@
	mov r0, sp
	bl  lguest_do_virtual_IRQ

	@
	@ Restore CPSR and restart the instruction
	@
	ldr	r2, [sp, #S_PSR]
	msr	cpsr, r2
	ldmia	sp, {r0 - pc}			@ load r0 - pc
 UNWIND(.fnend      )
ENDPROC(__lguest_dabt_kernel)


	.align	5
__lguest_irq_kernel:
	kernel_entry
	
	@ 
	@ Now we go to the Host, and let the Host handle
	@ real irqs.
	@
	guest_to_switcher lr, SWITCHER_IRQ_OFFSET

	switcher_to_guest

    @
    @ Let us check if there are some virtual irqs which need
    @ to be handled.
    @
	mov r0, sp
	bl lguest_do_virtual_IRQ

	ldr r2, [sp, #S_PSR]
	msr cpsr, r2
	ldmia sp, {r0 - pc}     @ load r0 - pc, return from exception
 UNWIND(.fnend      )
ENDPROC(__lguest_irq_kernel)

	.ltorg

	.align	5
/*FIXME currently we cannot handle emulation code*/
__lguest_und_kernel:
	kernel_entry

	@
	@ Althought the Host will do nothing for the Guest about 
	@ the Undefined instruction, We return to the Host anyway.
	@
	guest_to_switcher lr, SWITCHER_UNDEF_OFFSET
    
	switcher_to_guest

	mov	r0, sp					@ struct pt_regs *regs
	bl	do_undefinstr

	@
	@ Let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	bl lguest_do_virtual_IRQ

	@
	@ restore CPSR and restart the instruction
	@
	ldr	r2, [sp, #S_PSR]		
	msr    cpsr, r2
	ldmia sp, {r0 - pc}			@ load r0 - pc, cpsr. return from exception
 UNWIND(.fnend      )
ENDPROC(__lguest_und_kernel)


	.align	5
__lguest_pabt_kernel:
	kernel_entry

	@
	@ We go back to the Host and let the host
	@ handle it first.
	@ When we come back, the Host have already put the aborted
	@ address in r0, and the fault status register in r1.
	@
	guest_to_switcher lr, SWITCHER_PABT_OFFSET

	switcher_to_guest

	@
	@ if r4 == 0, the Host have already solved the problem.
	@ Otherwise, let do_PrefetchAbort handle it
	@
	cmp r4, #0
	movne	r2, sp							
	blne	do_PrefetchAbort		@ call abort handler

	@
	@ Let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	bl lguest_do_virtual_IRQ

	
	@
	@ restore CPSR and restart the instruction
	@
	
	ldr	r2, [sp, #S_PSR]
	msr	cpsr, r2
	ldmia	sp, {r0 - pc}		@ load r0 - pc, cpsr. return from exception
 UNWIND(.fnend      )
ENDPROC(__lguest_pabt_kernel)

	.align	5
.LCcralign:
	.word	cr_alignment



/*
 * User space handlers
 */

#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5) && (S_FRAME_SIZE & 7)
#error "sizeof(struct pt_regs) must be a multiple of 8"
#endif

	.macro	usr_entry
 UNWIND(.fnstart    )
 UNWIND(.cantunwind )   @ don't unwind the user space

	sub	sp, sp, #S_FRAME_SIZE
	stmib	sp, {r1 - r12}	

	@ Get the kernel stack pointer first.
	ldr lr, .GuestKernelStack
	ldr lr, [lr]
	sub lr, lr, #S_FRAME_SIZE
	
	stmib	lr, {r1 - lr}^
	ldmia	r0, {r1 - r3}

	mov	r4, #-1				
	str	r1, [lr, #S_R0]			@ save the "real" r0 copied
								@ from the exception stack
	add r0, lr, #S_PC
	
	@
	@ We are now ready to fill in the remaining blanks on the stack:
	@
	@  r2 - lr_<exception>, already fixed up for correct return/restart
	@  r3 - spsr_<exception>
	@  r4 - orig_r0 (see pt_regs definition in ptrace.h)
	@
	stmia	r0, {r2 - r4}

	@
	@ We need to save the Guest kernel stack pointer on 
	@ SVC stack(the Switcher stack).							
	@
	str lr, [sp, #S_SP]

	@
	@ Enable the alignment trap while in kernel mode
	@
	alignment_trap r0

	@
	@ Clear FP to mark the first stack frame
	@
	zero_fp
	.endm


	.align	5
__lguest_dabt_usr:
	usr_entry
	@
	@ We go back to the Host and let the host
	@ handle it first.
	@ When we come back, the Host have already put the aborted
	@ address in r0, and the fault status register in r1.
	@
	guest_to_switcher lr, SWITCHER_DABT_OFFSET

	switcher_to_guest

	@
	@ if r4 == 0, the Host have already solved the problem.
	@ Otherwise, let do_DataAbort handle it.
    @
	cmp r4, #0
	movne r2, sp
	blne do_DataAbort


	@
	@ let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	@ Set the return address.
	adr lr, BSYM(lguest_ret_from_exception)
	b lguest_do_virtual_IRQ
 UNWIND(.fnend      )
ENDPROC(__lguest_dabt_usr)


	.align	5
__lguest_irq_usr:
	usr_entry
	@
	@ Now we go to the Host, and let the Host handle
	@ real irqs.
	@
	guest_to_switcher lr, SWITCHER_IRQ_OFFSET

	switcher_to_guest


	@
	@ let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	bl lguest_do_virtual_IRQ

	get_thread_info tsk
	mov	why, #0
	b lguest_ret_to_user
 UNWIND(.fnend      )
ENDPROC(__lguest_irq_usr)

	.ltorg

	.align	5

/*FIXME currently we cannot handle emulation code*/
__lguest_und_usr:
	usr_entry
	@
	@ Althought the Host will do nothing for the Guest about 
	@ the Undefined instruction, We return to the Host anyway.
	@
	guest_to_switcher lr, SWITCHER_UNDEF_OFFSET
	switcher_to_guest


	@ Call the do_undefinstr to handle it.
	mov r0, sp
	bl   do_undefinstr

	@
	@ Let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	adr lr, BSYM(lguest_ret_from_exception)
	b lguest_do_virtual_IRQ
 UNWIND(.fnend      )
ENDPROC(__lguest_und_usr)





	.align	5
__lguest_pabt_usr:
	usr_entry
    @
    @ We go back to the Host and let the host
    @ handle it first.
    @ When we come back, the Host have already put the aborted
    @ address in r0, and the fault status register in r1.
    @
	guest_to_switcher lr, SWITCHER_PABT_OFFSET

	switcher_to_guest

	@
	@ if r4 == 0, the Host have already solved the problem.
	@ Otherwise, let do_PrefetchAbort handle it
	@
	cmp r4, #0
	movne	r2, sp				@ regs
	blne	do_PrefetchAbort		@ call abort handler

	@
	@ let us check if there are some virtual irqs which need
	@ to be handled.
	@
	mov r0, sp
	bl lguest_do_virtual_IRQ
 UNWIND(.fnend      )
	/* fall through */
/*
 * This is the return code to user mode for abort handlers
 */
ENTRY(lguest_ret_from_exception)
 UNWIND(.fnstart    )
 UNWIND(.cantunwind )

	get_thread_info tsk
	mov	why, #0
	b	lguest_ret_to_user
 UNWIND(.fnend      )
ENDPROC(__lguest_pabt_usr)
ENDPROC(lguest_ret_from_exception)


/*
 * Register switch for ARMv3 and ARMv4 processors
 * r0 = previous task_struct, r1 = previous thread_info, r2 = next thread_info
 * previous and next are guaranteed not to be the same.
 */
ENTRY(__lguest_switch_to)
 UNWIND(.fnstart    )
 UNWIND(.cantunwind )

	add	ip, r1, #TI_CPU_SAVE
	ldr	r3, [r2, #TI_TP_VALUE]
	stmia	ip!, {r4 - sl, fp, sp, lr} 	@ Store most regs on stack

	ldr	r6, [r2, #TI_CPU_DOMAIN]

	mov	r5, r0
	add	r4, r2, #TI_CPU_SAVE
	
	mov r0, r3
	mov r1, r6

	@ This will take us back to the Host.	
	bl lguest_switch_to
	
	ldr	r0, =thread_notify_head
	mov	r1, #THREAD_NOTIFY_SWITCH
	bl	atomic_notifier_call_chain

	mov	r0, r5
	ldmia	r4, {r4 - sl, fp, sp, pc}  	@ Load all regs saved previously
UNWIND(.fnend      )
ENDPROC(__lguest_switch_to)





/*
 * This is the Guest kernel startup entry. The Host has already set the following
 * registers for Guest.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags pointer
 *  r9  = processor ID
 */
ENTRY(lguest_kernel_start)
	@ set the Guest kernel stack register first. 
	ldr lr, .GuestKernelStack
	ldmia lr, {sp}^   

	@ 
	@ Calculate the the first page address of "struct lguest_pagses"
	@ according to the SVC stack pointer(the Switcher stack).
	@      
	mov lr, sp, lsr #12     
	mov lr, lr, lsl #12

	@ Save this address. The Guest uses this page to communicate with the Host.
	ldr r6, .LguestPageBase  
	str lr, [r6]
	
	@ Save the Switcher address, and the Switcher address is 2M aligned.
	mov lr, lr, lsr #21	
	mov lr, lr, lsl #21
	ldr r6, .LguestSwitcherAddr
	str lr, [r6]


	@ Set CPSR for the Guest kernel.
	msr cpsr, #(PSR_F_BIT | USR_MODE)       

	/* The following is based on code originally from head-common.S */
	adr r3, __lguest_switch_data

	ldmia   r3!, {r4, r5, r6, r7}
	cmp r4, r5              @ Copy data segment if needed
1:  cmpne   r5, r6
	ldrne   fp, [r4], #4
	strne   fp, [r5], #4
	bne 1b

	mov fp, #0              @ Clear BSS (and zero fp)
1:  cmp r6, r7
	strcc   fp, [r6],#4
	bcc 1b

	ldmia   r3, {r4, r5, r6, r7}
	str r9, [r4]            @ Save processor ID
	str r1, [r5]            @ Save machine type
	str r2, [r6]            @ Save atags pointer
	bic r4, r0, #CR_A       @ Clear 'A' bit
	stmia   r7, {r0, r4}    @ Save control register values
	b   lguest_init
ENDPROC(lguest_kernel_start)



    .align  2
	.type   __lguest_switch_data, %object
__lguest_switch_data:
	.long   __data_loc          @ r4
	.long   _data               @ r5
	.long   __bss_start         @ r6
	.long   _end                @ r7
	.long   processor_id            @ r4
	.long   __machine_arch_type     @ r5
	.long   __atags_pointer         @ r6
	.long   cr_alignment            @ r7


.ltorg

.LguestSwitchTo:
	.word lguest_switch_to

.GuestKernelStack:
	.word guest_kernel_stack

.LguestPageBase:
	.word lguest_page_base

.LguestSwitcherAddr:
	.word lguest_switcher_addr 




    __INIT

 THUMB( .arm    )
    .macro  usr_ret, reg
#ifdef CONFIG_ARM_THUMB
	bx  \reg
#else
	mov pc, \reg
#endif
	.endm

	.align  5
	/*
	 * the Following User helpers functions are orignially defined in 
	 * arch/arm/kernel/entry-armv.S, I just copied them here.
	 */	 
    .globl  __lguest_kuser_helper_start
__lguest_kuser_helper_start:

__kuser_memory_barrier:             @ 0xffff0fa0
	smp_dmb
	usr_ret lr

    .align  5



__kuser_cmpxchg:                @ 0xffff0fc0

1:  ldrex   r3, [r2]
	subs    r3, r3, r0
	strexeq r3, r1, [r2]
	teqeq   r3, #1
	beq 1b
	rsbs    r0, r3, #0
	/* beware -- each __kuser slot must be 8 instructions max */
	usr_ret lr


	.align  5
__kuser_get_tls:                @ 0xffff0fe0
	mrc p15, 0, r0, c13, c0, 3      @ read TLS register

	usr_ret lr

	.rep    5
	.word   0           @ pad up to __kuser_helper_version
	.endr

__lguest_kuser_helper_version:             @ 0xffff0ffc
	.word   ((__lguest_kuser_helper_end - __lguest_kuser_helper_start) >> 5)

	.globl  __lguest_kuser_helper_end
__lguest_kuser_helper_end:



THUMB( .thumb  )
/*
 * Vector stubs.
 *
 * This code is copied to 0xffff0200 so we can use branches in the
 * vectors, rather than ldr's.  Note that this code must not
 * exceed 0x300 bytes.
 */
	.macro	lguest_vector_stub, name, mode, correction=0
	.align	5

lguest_vector_\name:
	.if \correction
	sub	lr, lr, #\correction
	.endif

	@
	@ Save r0, lr_<exception> (parent PC) and spsr_<exception>
	@ (parent CPSR)
	@
	stmia	sp, {r0, lr}		@ save r0, lr
	mrs	r0, spsr
	str	r0, [sp, #8]		@ save spsr

	@
	@ Prepare for SVC32 mode.  IRQs remain disabled.
	@
	mrs	r0, cpsr
	eor	r0, r0, #(\mode ^ SVC_MODE)
	msr	spsr_cxsf, r0


	/* 
	 *	Both kernel and user applications of GUEST run under user mode,
	 *  so we should check lr_<exception> to determine where we come from,
	 *  kernel space or user space?		
	 *		
	 */
	cmp lr, #TASK_SIZE                      @ guest kernel or user?
	movcc lr, #USER_MODE
	adrcc pc, BSYM(7f)

	/*we also should deal with the kuser helper*/
	ldr r0, .UserHelperStart
	cmp lr, r0	
	movcc lr, #KERNEL_MODE
	adrcc pc, BSYM(7f)

	ldr r0, .UserHelperEnd
	cmp lr, r0
	movcs lr, #KERNEL_MODE
	movcc lr, #USER_MODE
	@
	@ the branch table must immediately follow this code
	@

7:
	mov	r0, sp
	ldr	lr, [pc, lr, lsl #2]	
	movs	pc, lr			@ branch to handler in SVC mode
ENDPROC(lguest_vector_\name)

	.align	2
	@ handler addresses follow this label
1:
	.endm

	.globl	__lguest_stubs_start
__lguest_stubs_start:
/*
 * Interrupt dispatcher
 */
	lguest_vector_stub	irq, IRQ_MODE, 4

	.long	__lguest_irq_usr				@ (GUEST USER)
	.long	__lguest_irq_kernel				@ (GUEST KERNEL)

/*
 * Data abort dispatcher
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
	lguest_vector_stub	dabt, ABT_MODE, 8

	.long	__lguest_dabt_usr				@ (GUEST USER)
	.long	__lguest_dabt_kernel			@ (GUEST KERNEL)

/*
 * Prefetch abort dispatcher
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
	lguest_vector_stub	pabt, ABT_MODE, 4

	.long	__lguest_pabt_usr				@ (GUEST USER)
	.long	__lguest_pabt_kernel			@ (GUEST KERNEL)


/*
 * Undef instr entry dispatcher
 * Enter in UND mode, spsr = USR CPSR, lr = USR PC
 */
	lguest_vector_stub	und, UND_MODE

	.long	__lguest_und_usr				@ (GUEST USER)
	.long	__lguest_und_kernel				@ (GUEST KERNEL)

	.align	5


lguest_hcall_reset:
	msr cpsr_c, #(PSR_F_BIT | USR_MODE)
	ldr pc, .GuestReset

/*
 * When the Guest kernel get here, we just return to the Host
 * and let host kill guest.	
 */
lguest_vector_fiq:
	msr cpsr_c, #(PSR_F_BIT | USR_MODE)
	ldr pc, .GuestHalt

/* Same as lguest_vector_fiq*/
lguest_hcall_addrexcptn:
	msr cpsr_c, #(PSR_F_BIT | USR_MODE)
	ldr pc, .GuestHalt

	.align	5

.LCvswi:
	.word lguest_vector_swi

.LguestStartKernel:
	.word lguest_kernel_start

.GuestReset:
	.word lguest_reset
	
.GuestHalt:
	.word lguest_halt

.UserHelperStart:
	.long 0xffff0fa0

.UserHelperEnd:
	.long 0xffff1000


	.globl	__lguest_stubs_end
__lguest_stubs_end:

	.equ	stubs_offset, __lguest_vectors_start + 0x200 - __lguest_stubs_start

	.globl	__lguest_vectors_start
__lguest_vectors_start:
	W(b)	lguest_hcall_reset + stubs_offset
	W(b)	lguest_vector_und + stubs_offset
	W(ldr)	pc, .LCvswi + stubs_offset
	W(b)	lguest_vector_pabt + stubs_offset
	W(b)	lguest_vector_dabt + stubs_offset
	W(b)	lguest_hcall_addrexcptn + stubs_offset
	W(b)	lguest_vector_irq + stubs_offset
	W(b)	lguest_vector_fiq + stubs_offset
	W(ldr)  pc, .LguestStartKernel + stubs_offset

	.globl	__lguest_vectors_end
__lguest_vectors_end:


.section ".lguest.guest.vectors.address", #alloc
lguest_vectors_start:
.long __lguest_vectors_start
lguest_vectors_end:
.long __lguest_vectors_end
lguest_vector_stubs_start:
.long __lguest_stubs_start
lguest_vector_stubs_end:
.long __lguest_stubs_end
lguest_kuser_helper_start:
.long __lguest_kuser_helper_start
lguest_kuser_helper_end:
.long __lguest_kuser_helper_end


.global lguest_vectors_start
.global lguest_vectors_end
.global lguest_vector_stubs_start
.global lguest_vector_stubs_end
.global lguest_kuser_helper_start
.global lguest_kuser_helper_end

    .data
    .globl  guest_kernel_stack
guest_kernel_stack:
.long   init_thread_union + THREAD_START_SP @ guest kernel sp

.global lguest_page_base
lguest_page_base:
.space 4


.global lguest_switcher_addr
lguest_switcher_addr:
.space 4


