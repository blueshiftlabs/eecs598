/*
 *  linux/arch/arm/mach-armlguest/kernel/lguest-entry-common.S
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 *  This code is based on the linux/arch/arm/kernel/entry-common.S,
 *  written by Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */



#include <asm/unistd.h>
#include <mach/entry-macro.S>
#include <asm/lguest_hcall.h>
#include <asm/memory.h>


#include "lguest-entry-header.S"

#define KUSER_HELPER_START 0xffff0fa0
#define KUSER_HELPER_END 0xffff1000


.macro switcher_to_guest
	stmia sp, {r0 - r12}

	mov r5, sp, lsr #12
	mov r5, r5, lsl #12
	@ 
	@ Save the address of the first page of "struct lguest_pages"
	@
	ldr r6, .SLguestPageBase 
	str r5, [r6]

	@ Restore r0-sp for the Guest Kernel.
	ldmia sp, {r0 - sp}^                
	
	@ Adjuest svc sp for the Switcher
	add sp, sp, #S_FRAME_SIZE			

	@ Set CPSR for the Guest Kernel
	msr cpsr_c, #(PSR_F_BIT | USR_MODE)             
.endm


	.align	5


/*
 * This is how we return from a fork.
 */
ENTRY(lguest_ret_from_fork)
	bl	schedule_tail
	get_thread_info tsk
	ldr	r1, [tsk, #TI_FLAGS]		@ check for syscall tracing
	mov	why, #1
	tst	r1, #_TIF_SYSCALL_TRACE		@ are we tracing syscalls?
	beq	lguest_ret_slow_syscall
	mov	r1, sp
	mov	r0, #1						@ trace exit [IP = 1]
	bl	syscall_trace
	b	lguest_ret_slow_syscall
ENDPROC(lguest_ret_from_fork)



lguest_work_resched:
	bl  schedule

ENTRY(lguest_ret_to_user)
lguest_ret_slow_syscall:
	ldr r1, [tsk, #TI_FLAGS]
	tst r1, #_TIF_WORK_MASK
	bne lguest_work_pending

lguest_no_work_pending:
	add r6, sp, #S_FRAME_SIZE

	ldr r4, [sp, #S_PC]
	
	@ Check we should return to Kernel or User space.
	cmp r4, #TASK_SIZE                      

	@
	@ Before returning to User space, we should save guest kernel 
	@ stack pointer.
	@
	ldrcc r4, .GuestKernelStack	
	strcc r6, [r4]
	bcc 7f
	

	@
	@ If this is a Kernel thread, we just restore its context.
	@ ((lr >= TASK_SIZE) && (lr < 0xffff0fa0 || lr >= 0xffff1000)) = Kernel
	@ ((lr < TASK_SIZE) || (lr >= 0xffff0fa0 && lr < 0xffff1000)) = User 
	@ 
	ldr lr, =KUSER_HELPER_START
	cmp r4, lr
	strcc r6, [sp, #S_SP]
	bcc 7f

	@ We should return to User space.
	ldr lr, =KUSER_HELPER_END
	cmp r4, lr
	strcs r6, [sp, #S_SP]
	ldrcc r4, .GuestKernelStack
	strcc r6, [r4]

7:
	ldr r1, [sp, #S_PSR]
	msr cpsr, r1
	ldmia sp, {r0 - pc}          @ get calling r0 - lr

lguest_work_pending:
	tst r1, #_TIF_NEED_RESCHED
	bne lguest_work_resched
	tst r1, #_TIF_SIGPENDING|_TIF_NOTIFY_RESUME
	beq lguest_no_work_pending
	mov r0, sp                      @ 'regs'
	mov r2, why                     @ 'syscall'
	bl  do_notify_resume
	b   lguest_ret_slow_syscall     @ Check work again
ENDPROC(lguest_ret_to_user)





.equ NR_syscalls,0
#define CALL(x) .equ NR_syscalls,NR_syscalls+1
#include "../../kernel/calls.S"
#undef CALL



/*=============================================================================
 * SWI handler
 *-----------------------------------------------------------------------------
 */


	.align	5
ENTRY(lguest_vector_swi)
	sub	sp, sp, #S_FRAME_SIZE
	stmia	sp, {r0 - sp}^

	@ Check who issue a "SWI", Kernel or User space?	
	cmp lr, #TASK_SIZE			

	@ If a user application issued the "SWI", this is a system call.
	@ we get the Guest kernel stack pointer first.
	ldrcc r0, .GuestKernelStack	
	ldrcc r0, [r0]					

	@
	@ If the kernel issue the "SWI", this may be a "hypercall".
	@ We can get kernel stack pointer directly from the stack.
	@
	ldrcs r0, [sp, #S_SP]		

	sub r0, r0, #S_FRAME_SIZE
	stmib r0, {r1 - lr}^
	str lr, [r0, #S_PC]			@ interrupted pc

	mrs r8, spsr
	str r8, [r0, #S_PSR]	    @ save spsr on the Guest kernel stack

	mov r8, r0					@ r0 should be restored

	ldr r0, [sp, #S_R0]
	str r0, [r8, #S_R0]			@ save orignal r0 on guest kernel stack
	str r0, [r8, #S_OLD_R0]     @ Save OLD_R0

	@ save current Guest kernel stack pointer on svc stack(Switcher).
	str r8, [sp, #S_SP]


#if defined(CONFIG_OABI_COMPAT)
#ifdef CONFIG_ARM_THUMB
	ldr r8, [r8, #S_PSR]
	tst r8, #PSR_T_BIT
	movne   r10, #0             @ no thumb OABI emulation
	ldreq   r10, [lr, #-4]      @ get SWI instruction
#else // CONFIG_ARM_THUMB

	/*
	 * If we have CONFIG_OABI_COMPAT then we need to look at the swi
	 * value to determine if it is an EABI or an old ABI call.
	 */
	ldr	r10, [lr, #-4]						@ get SWI instruction
#endif  //CONFIG_ARM_THUMB
	bics	r8, r10, #0xff000000
	moveq r8, scno							@ If the swi argument is zero, this is an EABI call
#elif !defined(CONFIG_AEABI)
#ifdef CONFIG_ARM_THUMB
	/* Legacy ABI only, possibly thumb mode. */
	ldr r8, [r8, #S_PSR]
	tst r8, #PSR_T_BIT          @ this is SPSR from save_user_regs
	addne   scno, r7, #__NR_SYSCALL_BASE    @ put OS number in
	ldreq   scno, [lr, #-4]
#else  //CONFIG_ARM_THUMB

	/*
	 * Pure EABI user space always put syscall number into scno (r7).
	 */
	 
	/* Legacy ABI only. */
	ldr	scno, [lr, #-4]				@ get SWI instruction	
#endif  //CONFIG_ARM_THUMB
	bic	r8, scno, #0xff000000		@ mask off SWI op-code	

#else  //CONFIG_AEABI
	mov r8, scno
#endif  
	@
	@ Get the address of the Switcher.
	@
	ldr ip, .SLguestSwitcherAddr
	ldr ip, [ip]

	@
	@ Compared syscall number with the ARM Lguest Hypercall number.
	@ to determine if this is a hypercall.
	@
	ldr lr, .LguestHcallNumber	
	cmp r8, lr

	@ Calculate the the address of the entry for hypercalls in the Switcher.	
	addeq ip, ip, #SWITCHER_HCALL_OFFSET
	adreq lr, BSYM(__ret_hypercall)   @ return address
	moveq pc, ip

	@
	@ This a normal system call, but we should return to the Host first,
	@ and then come back to handle it.
	@ 
	addne ip, ip, #SWITCHER_GSYSCALL_OFFSET
	blxne ip
	
	@ This is a system call 
	zero_fp
#ifdef CONFIG_ALIGNMENT_TRAP
	ldr	ip, __cr_alignment
	ldr	ip, [ip]
	mcr	p15, 0, ip, c1, c0		@ update control register
#endif

	switcher_to_guest
	
	get_thread_info tsk
	ldr tbl, .SysCallTable
	ldr	ip, [tsk, #TI_FLAGS]	@ check for syscall tracing

#if defined(CONFIG_OABI_COMPAT)
	/*
	 * If the swi argument is zero, this is an EABI call and we do nothing.
	 *
	 * If this is an old ABI call, get the syscall number into scno and
	 * get the old ABI syscall table address.
	 */
	bics	r10, r10, #0xff000000
	eorne	scno, r10, #__NR_OABI_SYSCALL_BASE
	ldrne   tbl, .SysOabiCallTable
#elif !defined(CONFIG_AEABI)
	bic	scno, scno, #0xff000000			@ mask off SWI op-code
	eor	scno, scno, #__NR_SYSCALL_BASE	@ check OS number
#endif
	
	stmdb	sp!, {r4, r5}				@ push fifth and sixth args
	tst	ip, #_TIF_SYSCALL_TRACE			@ are we tracing syscalls?
	bne	__sys_trace

	cmp	scno, #NR_syscalls					@ check upper syscall limit
	adr   lr, BSYM(3f)   @ return address
	ldrcc	pc, [tbl, scno, lsl #2]			@ call sys_* routine

	add	r1, sp, #S_OFF
2:	mov	why, #0								@ no longer a real syscall
	cmp	scno, #(__ARM_NR_BASE - __NR_SYSCALL_BASE)
	eor	r0, scno, #__NR_SYSCALL_BASE		@ put OS number back
	bcs	arm_syscall	
	b	sys_ni_syscall						@ not private func	

3:
	@
	@ There is no fast syscall return path for the Guest kernel.
	@ We have to handle virtual interrupts first before returning 
	@ to the User Space.
	@
	str r0, [sp, #S_R0 + S_OFF]!			
	adr lr, BSYM(lguest_ret_slow_syscall)   @ return address
	mov r0, sp 
	b lguest_do_virtual_IRQ

__ret_hypercall:
	@
	@  We get there from switcher because this is a Guest's hypercall.
	@
	switcher_to_guest

	mov r0, sp
	bl lguest_do_virtual_IRQ

	ldr r2, [sp, #S_PSR]
	msr    cpsr, r2
	ldmia   sp, {r0 - pc}       @ load r0 - pc, return from exception
ENDPROC(lguest_vector_swi)	

	

	
__sys_trace:
	mov	r2, scno
	add	r1, sp, #S_OFF
	mov	r0, #0							@ trace entry [IP = 0]
	bl	syscall_trace

	adr	lr, BSYM(__sys_trace_return)	@ return address
	mov	scno, r0						@ syscall number (possibly new)
	add	r1, sp, #S_R0 + S_OFF			@ pointer to regs
	cmp	scno, #NR_syscalls				@ check upper syscall limit
	ldmccia	r1, {r0 - r3}				@ have to reload r0 - r3
	ldrcc	pc, [tbl, scno, lsl #2]		@ call sys_* routine
	b	2b

__sys_trace_return:
	str	r0, [sp, #S_R0 + S_OFF]!		@ save returned r0
	mov	r2, scno
	mov	r1, sp
	mov	r0, #1							@ trace exit [IP = 1]
	bl	syscall_trace

	adr lr, BSYM(lguest_ret_slow_syscall)   @ return address
	mov r0, sp
	b lguest_do_virtual_IRQ


    .align  5
#ifdef CONFIG_ALIGNMENT_TRAP
	.type   __cr_alignment, #object
__cr_alignment:
	.word   cr_alignment
#endif


.type   .SysCallTable, #object
.SysCallTable:
.word sys_call_table

.type   .SysOabiCallTable, #object
.SysOabiCallTable:
.word sys_oabi_call_table


.type  .LguestHcallNumber, #object
.LguestHcallNumber:
    .word HYPERCALL_NUMBER

.type   .SLguestPageBase, #object
.SLguestPageBase:
    .word lguest_page_base

.type   .SLguestSwitcherAddr, #object
.SLguestSwitcherAddr:
    .word lguest_switcher_addr


.type   .GuestKernelStack, #object
.GuestKernelStack:
    .word guest_kernel_stack


