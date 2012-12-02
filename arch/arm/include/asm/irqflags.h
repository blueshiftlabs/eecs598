#ifndef __ASM_ARM_IRQFLAGS_H
#define __ASM_ARM_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/ptrace.h>
#include <asm/lguest-native.h>

/*
 * CPU interrupt mask handling.
 */
#if __LINUX_ARM_ARCH__ >= 6

static inline unsigned long LGUEST_NATIVE(arch_local_irq_save) (void)
{
	unsigned long flags;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

static inline void LGUEST_NATIVE(arch_local_irq_enable) (void)
{
	asm volatile(
		"	cpsie i			@ arch_local_irq_enable"
		:
		:
		: "memory", "cc");
}

static inline void LGUEST_NATIVE(arch_local_irq_disable) (void)
{
	asm volatile(
		"	cpsid i			@ arch_local_irq_disable"
		:
		:
		: "memory", "cc");
}

static inline void LGUEST_NATIVE(local_fiq_enable) (void)
{	
	asm volatile(
		"cpsie f	@ __stf"
		:
		:
		: "memory", "cc");
}

static inline void LGUEST_NATIVE(local_fiq_disable) (void)
{
	asm volatile(
		"cpsid f	@ __clf"
		:
		:
		: "memory", "cc");
}

#else /* __LINUX_ARM_ARCH__ < 6 */

/*
 * Save the current interrupt enable state & disable IRQs
 */
static inline unsigned long LGUEST_NATIVE(arch_local_irq_save) (void)
{
	unsigned long flags, temp;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	orr	%1, %0, #128\n"
		"	msr	cpsr_c, %1"
		: "=r" (flags), "=r" (temp)
		:
		: "memory", "cc");
	return flags;
}


/*
 * Enable IRQs
 */
static inline void LGUEST_NATIVE(arch_local_irq_enable) (void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_enable\n"
		"	bic	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Disable IRQs
 */
static inline void LGUEST_NATIVE(arch_local_irq_disable) (void)
{
	unsigned long temp;
	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_disable\n"
		"	orr	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

/*
 * Enable FIQs
 */
static inline void LGUEST_NATIVE(local_fiq_enable) (void)
{							
	unsigned long temp;				
	asm volatile(					
		"	mrs	%0, cpsr		@ stf\n"		
		"	bic	%0, %0, #64\n"
		"	msr	cpsr_c, %0"					
		: "=r" (temp)						
		:							
		: "memory", "cc");
}

static inline void LGUEST_NATIVE(local_fiq_disable) (void)
{							
	unsigned long temp;				
	asm volatile(					
		"	mrs	%0, cpsr		@ clf\n"		
		"	orr	%0, %0, #64\n"
		"	msr	cpsr_c, %0"					
		: "=r" (temp)						
		:							
		: "memory", "cc");
}

#endif /* __LINUX_ARM_ARCH__ >= 6 */

/*
 * Save the current interrupt enable state.
 */
static inline unsigned long LGUEST_NATIVE(arch_local_save_flags) (void)
{
	unsigned long flags;
	asm volatile(
		"	mrs	%0, cpsr	@ local_save_flags"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

/*
 * restore saved IRQ & FIQ state
 */
static inline void LGUEST_NATIVE(arch_local_irq_restore) (unsigned long flags)
{
	asm volatile(
		"	msr	cpsr_c, %0	@ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}

lguest_define_hook(arch_local_irq_save);
lguest_define_hook(arch_local_save_flags);
lguest_define_hook(arch_local_irq_restore);
lguest_define_hook(arch_local_irq_enable);
lguest_define_hook(arch_local_irq_disable);
lguest_define_hook(local_fiq_enable);
lguest_define_hook(local_fiq_disable);

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & PSR_I_BIT;
}

#endif
#endif
