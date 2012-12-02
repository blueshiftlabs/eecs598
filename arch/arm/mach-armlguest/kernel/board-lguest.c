/*
 *  linux/arch/arm/mach-armlguest/kernel/board-lguest.c
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 *  This code is based on the linux/arch/x86/lguest/boot.c,
 *  written by Rusty Russell.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/console.h>
#include <linux/lguest.h>
#include <linux/lguest_launcher.h>
#include <linux/virtio_console.h>
#include <linux/highmem.h>

#include <asm/kmap_types.h>
#include <asm/lguest_hcall.h>
#include <asm/cputype.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/lguest.h>

static bool pgd_changed = false;

extern void *lguest_page_base;

extern void immediate_hcall(unsigned long arg1, unsigned long arg2, 
				unsigned long arg3, unsigned long call);

extern void lazy_hcall(unsigned long arg1, unsigned long arg2,       
                    unsigned long arg3, unsigned long call);

extern void do_guest_set_pte(pte_t *ptep, pte_t pte, unsigned long ext);

//---------------------Low level operation of lguest virtual processor------------------------
/*
 * The following functions are low level operations of ARM Lguest virtual processor, 
 * and These function either do nothing or just return to the Host and let the Host 
 * do something for the Guest. 
 * please see  arch/arm/mach-armlguest/kernel/proc-lguest.S 
 */

//============Cache operations(lguest_cache_fns)===========================
void lguest_flush_kern_cache_all(void)
{
	lazy_hcall(0, 0, 0, LHCALL_FLUSH_CACHE_KERNEL);
}


void lguest_flush_user_cache_all(void)
{
	lazy_hcall(0, 0, 0, LHCALL_FLUSH_CACHE_USER);
}


void lguest_flush_user_cache_range(unsigned long start, unsigned long end, unsigned int flags)
{
	lazy_hcall(start, end, flags, LHCALL_FLUSH_CACHE_USER_RANGE);
}


void lguest_coherent_kern_cache_range(unsigned long start, unsigned long end)
{
	lazy_hcall(start, end, 0, LHCALL_COHERENT_CACHE_KERNEL_RANGE);
}



void lguest_coherent_user_cache_range(unsigned long start, unsigned long end)
{
	lazy_hcall(start, end, 0, LHCALL_COHERENT_CACHE_USER_RANGE);
}


void lguest_flush_kern_dcache_area(void *addr, size_t size)
{
	lazy_hcall((unsigned long)addr, size, 0, LHCALL_FLUSH_DCACHE_KERNEL_AREA);
}



void lguest_dma_inv_cache_range(const void *start, const void *end)
{
	lazy_hcall((unsigned long)start, (unsigned long)end, 0, LHCALL_DMA_INV_CACHE_RANGE);
}



void lguest_dma_clean_cache_range(const void * start, const void *end)
{
    lazy_hcall((unsigned long)start, (unsigned long)end, 0, LHCALL_DMA_CLEAN_CACHE_RANGE);
}



void lguest_dma_flush_cache_range(const void *start, const void *end)
{
	lazy_hcall((unsigned long)start, (unsigned long)end, 0, LHCALL_DMA_FLUSH_CACHE_RANGE);
}
//============Cache operations(lguest_cache_fns)===========================



//============TLB operations(lguest_tlb_fns)===========================
/* lguest_tlb_fns */
unsigned long lguest_tlb_flags;


void lguest_flush_user_tlb_range(unsigned long start, unsigned long end, struct vm_area_struct *vma)
{
	lazy_hcall(start, end, (unsigned long)vma, LHCALL_FLUSH_USER_TLB);
}



void lguest_flush_kern_tlb_range(unsigned long start, unsigned long end)
{
	lazy_hcall(start, end, 0, LHCALL_FLUSH_KERNEL_TLB);
}
//============TLB operations(lguest_tlb_fns)===========================




//============lguest_processor_functions===========================
void  lguest_data_abort(unsigned long pc)
{

}


unsigned long lguest_prefetch_abort(unsigned long lr)
{
    return 0;
}


void lguest_cpu_proc_init(void)
{

}


void lguest_cpu_proc_fin(void)
{

}


void lguest_reset(unsigned long addr)
{
	immediate_hcall(0, 0, 0, LHCALL_RESET);
}


int lguest_do_idle(void)
{
	immediate_hcall(0, 0, 0, LHCALL_IDLE);
	return 0;
}


void lguest_dcache_clean_area(void *addr, int size)
{
	lazy_hcall((unsigned long)addr, (unsigned long)size, 0, LHCALL_DCACHE_CLEAN_AREA);
}


/*
 *  Set the translation table base pointer to be pgd_phys
 */
void lguest_cpu_switch_mm(unsigned long pgd_phys, struct mm_struct *mm)
{
	/*Tell the Host that the Guest want to change page table*/
	immediate_hcall(pgd_phys, mm->context.id, (unsigned long)mm, LHCALL_SWITCH_MM);
	pgd_changed = true;
}



/*
 * There are a couple of legacy places where the kernel sets a PTE, but we
 * don't know the top level any more.  This is useless for us, since we don't
 * know which pagetable is changing or what address, so we just tell the Host
 * to forget all of them except for directly mapped memory.  Fortunately, 
 * this is very rare.
 *
 */
void lguest_cpu_set_pte_ext(pte_t *ptep, pte_t pteval, unsigned long ext)
{
	do_guest_set_pte(ptep, pteval, ext);
	if(pgd_changed)
		lazy_hcall(0, 0, 0, LHCALL_SET_PTE_EXT);
}
//============lguest_processor_functions===========================




//===========lguest_user_fns==========================================
/* I just copied relative functions in linux/arch/arm/mm/copypage-v6.c here	*/
/*
 * Clear the user page.  No aliasing to deal with so we can just
 * attack the kernel's existing mapping of this page.
 */
static void lguest_clear_user_highpage_nonaliasing(struct page *page, unsigned long vaddr)
{
	void *kaddr = kmap_atomic(page, KM_USER0);
	clear_page(kaddr);
	kunmap_atomic(kaddr, KM_USER0);
}



/*
 * Copy the user page.  No aliasing to deal with so we can just
 * attack the kernel's existing mapping of these pages.
  */
static void lguest_copy_user_highpage_nonaliasing(struct page *to,
	struct page *from, unsigned long vaddr, struct vm_area_struct *vma)
{
	void *kto, *kfrom;

	kfrom = kmap_atomic(from, KM_USER0);
	kto = kmap_atomic(to, KM_USER1);
	copy_page(kto, kfrom);
	kunmap_atomic(kto, KM_USER1);
	kunmap_atomic(kfrom, KM_USER0);
}


struct cpu_user_fns lguest_user_fns __initdata = {
    .cpu_clear_user_highpage = lguest_clear_user_highpage_nonaliasing,
    .cpu_copy_user_highpage = lguest_copy_user_highpage_nonaliasing,
};
//===========lguest_user_fns==========================================
//---------------------Low level operation of CPU----------------------------------------




static void disable_lguest_irq(struct irq_data *data)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	set_bit(data->irq, lgregs->blocked_interrupts);
}


static void enable_lguest_irq(struct irq_data *data)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	clear_bit(data->irq, lgregs->blocked_interrupts);
	if(!(lgregs->irq_disabled & PSR_I_BIT))
		immediate_hcall(0, 2, 0,LHCALL_SEND_INTERRUPTS);
}



/*
 * We also need a "struct clock_event_device": Linux asks us to set it to go
 * off some time in the future.  Actually, James Morris figured all this out, I
 * just applied the patch.
 */
static int lguest_clockevent_set_next_event(unsigned long delta,
                                           struct clock_event_device *evt)
{
	/* FIXME: I don't think this can ever happen, but James tells me he had
	 * to put this code in.  Maybe we should remove it now.  Anyone? */
	if (delta < LG_CLOCK_MIN_DELTA) {
		if (printk_ratelimit())
			printk(KERN_DEBUG "%s: small delta %lu ns\n",
				 __func__, delta);
			return -ETIME;
	}
	immediate_hcall(delta,0,0,LHCALL_SET_CLOCKEVENT);
	return 0;
}




static void lguest_clockevent_set_mode(enum clock_event_mode mode,
                                      struct clock_event_device *evt)
{
	switch (mode) {
		case CLOCK_EVT_MODE_UNUSED:
		case CLOCK_EVT_MODE_SHUTDOWN:
			/* A 0 argument shuts the clock down. */
			immediate_hcall(0,0,0,LHCALL_SET_CLOCKEVENT);
			break;
		case CLOCK_EVT_MODE_ONESHOT:
			/* This is what we expect. */
			break;
		case CLOCK_EVT_MODE_PERIODIC:
			BUG();
		case CLOCK_EVT_MODE_RESUME:
			break;
	}
}


static struct clock_event_device lguest_clockevent = {
	.name                   = "lguest",
	.features               = CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event         = lguest_clockevent_set_next_event,
	.set_mode               = lguest_clockevent_set_mode,
	.rating                 = INT_MAX,
	.mult                   = 1,
	.shift                  = 0,
	.min_delta_ns           = LG_CLOCK_MIN_DELTA,
	.max_delta_ns           = LG_CLOCK_MAX_DELTA,
};

/*
 * we read the time value given by the Host.
 */
static cycle_t lguest_clock_read(struct clocksource *cs)
{
	unsigned long sec, nsec;
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;


	/*
	 * Since the time is in two parts (seconds and nanoseconds), we risk
	 * reading it just as it's changing from 99 & 0.999999999 to 100 and 0,
	 * and getting 99 and 0.  As Linux tends to come apart under the stress
	 * of time travel, we must be careful:
	 */
	do {
		/* First we read the seconds part. */
		sec = lgregs->guest_time.tv_sec;
		/*
		 * This read memory barrier tells the compiler and the CPU that
		 * this can't be reordered: we have to complete the above
		 * before going on.
		 */
		rmb();
		/* Now we read the nanoseconds part. */
		nsec = lgregs->guest_time.tv_nsec;
		/* Make sure we've done that. */
		rmb();
		/* Now if the seconds part has changed, try again. */
	} while (unlikely(lgregs->guest_time.tv_sec != sec));


	/* Our lguest clock is in real nanoseconds. */
	return sec*1000000000ULL + nsec;
}



/* This is the fallback clocksource: lower priority than the TSC clocksource. */
static struct clocksource lguest_clock = {
	.name       = "lguest",
	.rating     = 250,
	.read       = lguest_clock_read,
	.mask       = CLOCKSOURCE_MASK(32),
	.mult       = 1 << 10,
	.shift      = 10,
	.flags      = CLOCK_SOURCE_IS_CONTINUOUS,
};



/*
 * This is the Guest timer interrupt handler (hardware interrupt 0).  We just
 * call the clockevent infrastructure and it does whatever needs doing.
 */
static irqreturn_t lguest_timer_interrupt(int irq, void *dev_id)
{
	unsigned long flags;

	/* Don't interrupt us while this is running. */
	local_irq_save(flags);
	lguest_clockevent.event_handler(&lguest_clockevent);
	local_irq_restore(flags);
	return IRQ_HANDLED;
}



static struct irqaction lguest_timer_irq = {
	.name       = "lguest",
	.flags      = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler    = lguest_timer_interrupt,
};




extern void lguest_setup_irq(unsigned int irq);

/*
 * At some point in the boot process, we get asked to set up our timing
 * infrastructure.  The kernel doesn't expect timer interrupts before this, but
 * we cleverly initialized the "blocked_interrupts" field of "struct
 * lguest_regs" so that timer interrupts were blocked until now.
 */
static void __init lguest_timer_init(void)
{
	int ret;

	lguest_setup_irq(0);

	/* Set up the timer interrupt (0) to go to our simple timer routine */
    ret = setup_irq(0, &lguest_timer_irq);

	/* 
	 * We can't set cpumask in the initializer: damn C limitations!  Set it
	 * here and register our timer device. 
	 */
	lguest_clockevent.cpumask = cpumask_of(0);
	clockevents_register_device(&lguest_clockevent);

	clocksource_register(&lguest_clock);

    /* Finally, we unblock the timer interrupt. */
	enable_lguest_irq(0);
}


static struct sys_timer lguest_timer = {
	.init   = lguest_timer_init,
};



/* This structure describes the lguest IRQ controller. */
static struct irq_chip lguest_irq_chip = {
	.name		= "lguest",
	.irq_mask	= disable_lguest_irq,
	.irq_mask_ack	= disable_lguest_irq,
	.irq_unmask	= enable_lguest_irq,
};


struct irq_chip * lguest_irq_controller(void)
{
	return &lguest_irq_chip;
}


/* initilize the lguest virtual irq chip*/
static void __init lguest_init_irq(void)
{
	int i;
	for (i = 0; i < LGUEST_IRQS; i++) {
		irq_set_chip_and_handler(i, &lguest_irq_chip, handle_level_irq);
		irq_set_status_flags(i, IRQF_VALID);
	}
}


/* we do nothing here at present*/
static void __init lguest_test_init(void)
{

}


static void __init lguest_fixup(struct tag *tags, 
	char **cmdline, struct meminfo *mi)
{
	/* Maybe we will do somthing here in the future. */
}


MACHINE_START(ARMLGUEST, "LGUEST Board")
	.atag_offset	= 0x100,
	.fixup		= lguest_fixup,
	.init_irq	= lguest_init_irq,
	.init_machine	= lguest_test_init,
	.timer		= &lguest_timer,
MACHINE_END
