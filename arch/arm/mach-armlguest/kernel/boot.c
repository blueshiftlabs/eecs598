/*
 *  linux/arch/arm/mach-armlguest/kernel/boot.c
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
#include <linux/start_kernel.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/virtio_console.h>
#include <linux/vmalloc.h>

#include <asm/lguest_hcall.h>
#include <asm/lguest.h>
#include <asm/cputype.h>
#include <asm/system.h>
#include <asm/unified.h>
#include <asm/mmu_context.h>

extern struct avm_cpu_ops avm_cpu_ops;
extern struct avm_irq_ops avm_irq_ops;
extern struct avm_mmu_ops avm_mmu_ops;



/*G:010 Welcome to the Guest!
 *
 * The Guest in our tale is a simple creature: identical to the Host but
 * behaving in simplified but equivalent ways.  In particular, the Guest is the
 * same kernel as the Host (or at least, built from the same source code).
:*/


/*
 * The address of user helper functions.
 * please linux/arch/arm/mach-armlguest/kernel/lguest-entry-armv.S.
 *
 */
#define KUSER_HELPER_START 0xffff0fa0
#define KUSER_HELPER_END 0xffff1000



/* 
 * The following varibles are defined in linux/arch/arm/mach-armlguest/kernel/lguest-entry-armv.S.
 * There will be a special section in Linux image if CONFIG_ARM_LGUEST_GUEST is defined. 
 * This section contains the address of the Guest Low-level vectors in the Image, and the Lguest
 * launcher can read vectors out from the Image according to these address, so the Host can setup 
 * low level vectors for Guest. 
 * Please see linux/arch/arm/kernel/vmlinux.lds.S 
 */
extern unsigned long __lguest_vectors_start;
extern unsigned long __lguest_vectors_end;
extern unsigned long __lguest_stubs_start;
extern unsigned long __lguest_stubs_end;


EXPORT_SYMBOL(__lguest_vectors_start);
EXPORT_SYMBOL(__lguest_vectors_end);
EXPORT_SYMBOL(__lguest_stubs_start);
EXPORT_SYMBOL(__lguest_stubs_end);
EXPORT_SYMBOL(cr_alignment);

static int lguest_guest_start = 0;


/* 
 * lguest_page_base is the address of the first page of struct lguest_pages.
 * It is passed to the Guest when coming back from the Host. 
 * lguest_switcher_addr is the Switcher address.
 * please see lguest-entry-armv.S and lguest-entry-common.S 
 */
extern void *lguest_page_base;
extern void *lguest_switcher_addr;

extern struct irq_chip * lguest_irq_controller(void);

/*This function is from arch/x86/lguest/boot.c */
void lguest_setup_irq(unsigned int irq)
{
	struct irq_chip * lg_irq_chip = lguest_irq_controller();
	irq_to_desc_alloc_node(irq, 0);
	set_irq_chip_and_handler_name(irq, lg_irq_chip,
						handle_level_irq, "level");
}


/* The Guest does not care about hardware PTEs, it Only sets linux pte entries. */
void do_guest_set_pte(pte_t *ptep, pte_t pte, unsigned long ext)
{
	*ptep = pte;
}


#define NEXT_HCALL(current) \
			((current + 1) & (LHCALL_RING_SIZE - 1))



/*
 * setup_hcall() is pretty simple: We have a ring buffer which is located in the first page of
 * "struct lguest_pages" to stored hypercalls which the Host will run though next time we 
 * do a normal hypercall. Please see linux/arch/arm/include/asm/lguest.h 
 * Each entry in the ring has 5 slots for the hypercall arguments, and a "command" word which 
 * indicates what the Host should do for the Guest, and once the Host has finished with it, 
 * the Host will set it to -1UL, which means this is an empty entry. The other four slots store
 * arguments of this hypercall. At present, A hypercall has at most 3 arguments.
 */
static int setup_hcalls(unsigned long arg1, unsigned long arg2, 
				unsigned long arg3, unsigned long call)
{
	static unsigned int next_call;
	int ret = 0;
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	struct hcall_args *hcalls = lgregs->hcalls;
	unsigned long flags;

	/*
	 * Disable interrupts if not already disabled: we don't want an
	 * interrupt handler making a hypercall while we're already doing
	 * one!
	 */
	flags = lgregs->irq_disabled;
	lgregs->irq_disabled = PSR_I_BIT;
	hcalls[next_call].arg0 = call;
	hcalls[next_call].arg1 = arg1;
	hcalls[next_call].arg2 = arg2;
	hcalls[next_call].arg3 = arg3;
	hcalls[next_call].arg4 = 0;
	wmb();
	next_call = NEXT_HCALL(next_call);
	if(hcalls[next_call].arg0 != -1UL){
		/* 
		 * This means that the Table is full, and we
		 * should return to the Host immediately.
		 */
		ret = 1;
	}
	/*
	 * restore irq flags. 
	 */
	lgregs->irq_disabled = flags;

    return ret;
}


/*
 * Here, I may disappoint you. In fact, The lazy hypercall has not 
 * been implemented by now. The Guest will immediately return to 
 * the Host when this function is called. 
 * The lazy hypercall should really be implemented in the future.
 */
void lazy_hcall(unsigned long arg1, unsigned long arg2, 
					unsigned long arg3, unsigned long call)
{
	int ret;
	ret = setup_hcalls(arg1, arg2, arg3, call);
	/* 
	 * "ret != 0" means the ring buffer is full, and the Guest should 
	 * immediately return to the Host.  As for avm_get_lazy_mode, please see 
	 * linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c
	 */
	if(ret || (avm_get_lazy_mode() == AVM_LAZY_NONE))
		lguest_hypercall();
}

/* This will immediately take us back to the Host */
void immediate_hcall(unsigned long arg1, unsigned long arg2, 
			unsigned long arg3, unsigned long call)
{
	setup_hcalls(arg1, arg2, arg3, call);
	lguest_hypercall();
}



/* Whether we are host or guest */
int lguest_guest_run(void)
{
	return lguest_guest_start;
}


/* 
 * When a Fiq or a Address Exception happens, this will be called
 * Please see linux/arch/arm/mach-armlguest/kernel/lguest-entry-arm.S
 */
void lguest_halt(void)
{
	immediate_hcall(0, 0, 0, LHCALL_HALT);
}

/* 
 * The Guest interrupt handler. Everytime we come back from the Host, 
 * this function will be called to check if there are some virtual 
 * interrupts which the Host send to the Guest. please see 
 * lguest-entry-armv.S and lguest-entry-common.S
 */
asmlinkage void __exception lguest_do_virtual_IRQ(struct pt_regs *regs)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	int irq = 0;
	DECLARE_BITMAP(blk, LGUEST_IRQS);

	/*if irqs of the Guest are disabled, We just return*/
	if(lgregs->irq_disabled & PSR_I_BIT)
		return;

	/* Before we handle irqs, We disable local irqs */
	lgregs->irq_disabled = PSR_I_BIT;
	memcpy(blk, lgregs->blocked_interrupts, sizeof(blk));
	bitmap_andnot(blk, lgregs->irqs_pending, blk, LGUEST_IRQS);


	/* We find out irqs which we can handle now */
	while((irq = find_first_bit(blk, LGUEST_IRQS)) < LGUEST_IRQS){
		clear_bit(irq, lgregs->irqs_pending);
		/* Find one, and we do it*/
		asm_do_IRQ(irq, regs);
		clear_bit(irq, blk);
	}

	/* Enable local irqs after we are done*/
	lgregs->irq_disabled = 0;

	/* If there are some pending irqs,we should go back to Host go get them  */
	if(lgregs->irq_pending){
		immediate_hcall(0, 1, 0,LHCALL_SEND_INTERRUPTS);
	}
}



/* 
 * __lguest_switch_to in linux/arch/arm/lguest/kernel/lguest-entry-arm.S 
 * calls this function. Ask the Host to set domain and tls registers fot
 * the Guest.
 */
void lguest_switch_to(unsigned long tp, unsigned long domain)
{
	lazy_hcall(tp, domain, 0, LHCALL_SWITCH_TO);
}



/* 
 * The Guest set a pte entry. it will go back to the Host and let the Host
 * set the relative shodow pagetable entry of the Guest. 
 */
static void lguest_set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pteval)
{
	unsigned long ext = 0;
	
	if(addr < TASK_SIZE){
		ext = PTE_EXT_NG;
	}
	do_guest_set_pte(ptep, pteval, ext);
	lazy_hcall(__pa(mm->pgd), addr, (unsigned long)pteval, LHCALL_SET_PTE);
}


/* 
 * The Guest clears a pte entry. it will go back to the Host and let the Host
 * clear the relative shodow pagetable entry of the Guest. 
 */
void lguest_pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	do_guest_set_pte(ptep, __pte(0), 0);
	lazy_hcall(__pa(mm->pgd), addr, 0, LHCALL_SET_PTE);
}


#define PGTABLE_SIZE (PTRS_PER_PGD * sizeof(pgd_t))
#define PGTABLE_MASK (~(PTRS_PER_PGD * sizeof(pgd_t) - 1))
#define PMD_ADDR_MASK (~((1 << 9) -1))	

//======================PMD Operations of The Guest===================================
/* 
 * PMD operations of the Guest. Please see linux/arch/arm/include/pgtable.h, 
 * find out what page tables look like on ARM Linux.
 * After the operations on  "local PMD" is done, the Guest will go to the Host
 * and let the Host to handle the Guest's shadow page tables.
 */
static void __lguest_pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
	pmdp[0] = __pmd(pmdval);
	pmdp[1] = __pmd(pmdval + 256 * sizeof(pte_t));

	lazy_hcall(__pa(pmdp) & PGTABLE_MASK, 
		(__pa(pmdp) & (PGTABLE_SIZE - 1)) / sizeof(pgd_t),		
			pmdval, LHCALL_SET_PGD);

}

static void lguest_copy_pmd(pmd_t *pmdpd, pmd_t *pmdps)
{
	pmdpd[0] = pmdps[0];
	pmdpd[1] = pmdps[1];
	lazy_hcall(__pa(pmdpd) & PGTABLE_MASK,__pa(pmdps) & PGTABLE_MASK, 
		0, LHCALL_COPY_PMD);
}


void lguest_pmd_clear(pmd_t *pmdp)
{
	pmdp[0] = __pmd(0);
	pmdp[1] = __pmd(0);
	lazy_hcall(__pa(pmdp) & PGTABLE_MASK, (__pa(pmdp) & 
		(PGTABLE_SIZE - 1)) / sizeof(pgd_t), 
				0, LHCALL_PMD_CLEAR);
}
//========PMD Operations of The Guest===========================================

/*
 * Panicing.
 *
 * Don't.  But if you did, this is what happens.
 */
static int lguest_panic(struct notifier_block *nb, unsigned long l, void *p)
{
	immediate_hcall(__pa(p), LGUEST_SHUTDOWN_POWEROFF, 0, LHCALL_SHUTDOWN);
	/* The hcall won't return, but to keep gcc happy, we're "done". */
	return NOTIFY_DONE;
}

static struct notifier_block paniced = {
	.notifier_call = lguest_panic
};



/*
 * We will eventually use the virtio console device to produce console output,
 * but before that is set up we use LHCALL_NOTIFY on normal memory to produce
 * console output.
 */
static __init int early_put_chars(u32 vtermno, const char *buf, int count)
{
	char scratch[50];
	unsigned int len = count;

	/* We use a nul-terminated string, so we make a copy.  Icky, huh? */
	if (len > sizeof(scratch) - 1)
		len = sizeof(scratch) - 1;
	scratch[len] = '\0';
	memcpy(scratch, buf, len);
	immediate_hcall(__pa(scratch),0,0,LHCALL_NOTIFY);

	/* This routine returns the number of bytes actually written. */
	return len;
}



/* 
 * The following functions replace native counter parts by avm_*_ops 
 * function pointers. the idea about avm_*_ops is borrowed from 
 * para-virtualization of X86. Please see arch/x86/include/asm/paravirt.h.
 * avm_*_ops structures contain  various instructions on ARM need to be 
 * replaced for Lguest.
 * please see linux/arch/arm/include/asm/lguest_privileged_ops.h, 
 * linux/arch/arm/include/asm/lguest_native.h,
 * linux/arch/arm/include/asm/lguest_privileged_fp.h and 
 * linux/arch/arm/mach-armlguest/kernel/guest_privileged_ops_init.c
 */

/*G:032
 * After that diversion we return to our first native-instruction
 * replacements: five functions for interrupt control.
 */

/*
 * raw_local_save_flags() and is expected to save the 
 * processor state.  
 */
static void lguest_raw_local_save_flags(unsigned long *px)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	*px = lgregs->irq_disabled;
}


/*
 * raw_local_irq_save and is expected to disable local irqs and save the 
 * processor state.  
 */
static void lguest_raw_local_irq_save(unsigned long *px)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	*px = lgregs->irq_disabled;
	lgregs->irq_disabled = PSR_I_BIT;
}


/*
 * raw_local_irq_restore and is expected to restore the processor state which 
 * is saved by raw_local_irq_save. 
 */
static void lguest_raw_local_irq_restore(unsigned long x)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	lgregs->irq_disabled = x;
	/* If there are still some pending irqs, the Guest go back to the Host*/
	if((!(lgregs->irq_disabled & PSR_I_BIT)) && (lgregs->irq_pending))
		immediate_hcall(0, 5, 0, LHCALL_SEND_INTERRUPTS);
}

/* Enable local irqs*/
static void lguest_raw_local_irq_enable(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	lgregs->irq_disabled = 0;

	/* If there are still some pending irqs, the Guest go back to the Host*/
	if(lgregs->irq_pending)
		immediate_hcall(0, 6, 0, LHCALL_SEND_INTERRUPTS);
}

/* Interrupts go off... */
static void lguest_raw_local_irq_disable(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	lgregs->irq_disabled = PSR_I_BIT;
}



/* 
 * The Guest does not handle FIQ, if a FIQ happens, it will go back to
 * the Host, and let the Host kill the Guest
 */
static void lguest_local_fiq_enable(void)
{

}

static void lguest_local_fiq_disable(void)
{

}




//========Local TLB operations===== Please see linux/arch/arm/include/asm/tlbflush.h========    
/* 
 * The Guest just goes back to the Host and let the Host handle it
 */
static void lguest_local_flush_tlb_all(void)
{
	lazy_hcall(0, 0, 0, LHCALL_LOCAL_FLUSH_TLB_ALL);
}


/* avm_mmu_ops.local_flush_tlb_mm */
static void lguest_local_flush_tlb_mm(struct mm_struct *mm)
{
	lazy_hcall((unsigned long)mm, 0, 0, LHCALL_LOCAL_FLUSH_TLB_MM);
}

/* avm_mmu_ops.local_flush_tlb_page */
static void lguest_local_flush_tlb_page(struct vm_area_struct *vma, 
					unsigned long uaddr)
{
	lazy_hcall((unsigned long)vma, uaddr, 0, LHCALL_LOCAL_FLUSH_TLB_PAGE);
}

/* avm_mmu_ops.local_flush_tlb_kernel_page */
static void lguest_local_flush_tlb_kernel_page(unsigned long kaddr)
{
	lazy_hcall(kaddr, 0, 0, LHCALL_LOCAL_FLUSH_TLB_KERNEL_PAGE);
}


/* avm_mmu_ops.flush_pmd_entry */
static void lguest_flush_pmd_entry(pmd_t *pmd)
{ 
	lazy_hcall((unsigned long)pmd,0,0,LHCALL_FLUSH_PMD);
}


/* avm_mmu_ops.clean_pmd_entry */
static void lguest_clean_pmd_entry(pmd_t *pmd)
{
	lazy_hcall((unsigned long)pmd,0,0,LHCALL_CLEAN_PMD);
}
//========Local TLB operations==========================================



/*
 * Its native counter part is defined in linux/arch/arm/include/asm/mmu_context.h.
 * Its native counter part does nothing. Should we do something here?.
 */
static void lguest_enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{

}



static DEFINE_SPINLOCK(cpu_asid_lock);
/*
 * Its native counter part is defined in linux/arch/arm/mm/context.c
 */
static void __lguest_new_context(struct mm_struct *mm)
{
	unsigned int asid;

	spin_lock(&cpu_asid_lock);
	asid = ++cpu_last_asid;
	if (asid == 0)
		asid = cpu_last_asid = ASID_FIRST_VERSION;

	if (unlikely((asid & ~ASID_MASK) == 0)) {
		asid = ++cpu_last_asid;
	}
	spin_unlock(&cpu_asid_lock);

	cpumask_copy(mm_cpumask(mm), cpumask_of(smp_processor_id()));
	mm->context.id = asid;
}

/*
 * Its native counter part is defined in
 * linux/arch/arm/include/asm/mmu_context.h
 */
static inline void lguest_check_context(struct mm_struct *mm)
{
	if (unlikely((mm->context.id ^ cpu_last_asid) >> ASID_BITS))
		__lguest_new_context(mm);

	if (unlikely(mm->context.kvm_seq != init_mm.context.kvm_seq))
		__check_kvm_seq(mm);
}



/*
 * Its native counter part is defined in linux/arch/arm/include/asm/mmu_context.h
 * You can see that this function calls cpu_switch_mm, but actually 
 * lguest_cpu_switch_mm will be called. 
 */
static void
lguest_switch_mm(struct mm_struct *prev, struct mm_struct *next,
      struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();

	if (!cpumask_test_and_set_cpu(cpu, mm_cpumask(next)) || prev != next) {
		lguest_check_context(next);
		cpu_switch_mm(next->pgd, next);
	}
}


/* 
 * Its native counter part is defined in linux/arch/arm/include/asm/proc-fns.h.
 * For the Guest, the Host will write the address of Guest's current page table
 * in  "struct lguest_regs" when Guest's current page table changes.
 */
static pgd_t *lguest_cpu_get_pgd(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	return (pgd_t *)lgregs->gpgdir;
}


/* 
 * Its native counter part is defined in linux/arch/arm/include/asm/cacheflush.h.
 * The Guest just returns to the Host and let the Host to handle it.
 */
static void __lguest_flush_icache_all(void)
{
	lazy_hcall(0,0,0,LHCALL_FLUSH_ICACHE_ALL);
}



/* 
 * This function is supposed to return the cpu id, the cache type 
 * and TLB type. Its native counter part is defined in
 * linux/arch/arm/include/asm/cputype.h
 */
static unsigned int lguest_read_cpuid(unsigned long reg)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	switch(reg){
		case CPUID_ID:
			return lgregs->guest_cpuid_id;
		case CPUID_CACHETYPE:
			return lgregs->guest_cpuid_cachetype;
		case CPUID_TCM:
			return lgregs->guest_cpuid_tcm;
		case CPUID_TLBTYPE:
			return lgregs->guest_cpuid_tlbtype;
	}
	return 0;
}

/* 
 * FIXME this function has not been implemented by now, and its native counter part
 * is defined in linux/arch/arm/include/asm/cputype.h
 */
static unsigned int lguest_read_cpuid_ext(unsigned long ext_reg)
{
	return 0;
}


/*
 * This function should return the value of Control Register. Its native 
 * counter part is defined in linux/arch/arm/include/asm/system.h
 */
static unsigned int lguest_get_cr(void)
{
	return cr_alignment;
}


/*
 * This function should set Control Register, but it calls the privileged 
 * instruction, so we let the Host handle it for the Guest. Its native 
 * counter part is defined in linux/arch/arm/include/asm/system.h
 */
static void lguest_set_cr(unsigned int val)
{
	cr_alignment = val;
	immediate_hcall(val, 0, 0, LHCALL_SET_CR);
}



/*
 * This function should return the value of Coprocessor Access Control Register, 
 * but it call the privileged instruction, so we let the Host handle it for the 
 * Guest. Its native counter part is defined in linux/arch/arm/include/asm/system.h
 */
static unsigned int lguest_get_copro_access(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	return lgregs->guest_copro;
}


/*
 * This function should set Coprocessor Access Control Register, but it is 
 * privileged instruction, so we let the Host do it for the Guest. Its native 
 * counter part is defined in linux/arch/arm/include/asm/system.h
 */
static void lguest_set_copro_access(unsigned int val)
{
	immediate_hcall(val, 0, 0, LHCALL_SET_COPRO);
}


/*
 *	Tell the Host to set domain for the Guest. Its native
 *	counter part is defined linux/arch/arm/include/asm/domain.h  
 */
void lguest_set_domain(unsigned long domain)
{
	lazy_hcall(domain, 0, 0, LHCALL_SET_DOMAIN);
}



/*
 * Because both kernel and user applications of the Guest run 
 * under user mode, we detemine user mode according to PC value.
 * the native counter part is defined in
 * linux/arch/arm/include/asm/ptrace.h
 */
static int lguest_user_mode(struct pt_regs *regs)
{
	return ((regs->ARM_pc < TASK_SIZE) || 
		((regs->ARM_pc >= KUSER_HELPER_START) && (regs->ARM_pc < KUSER_HELPER_END)));
}



/*
 * At present this function is the same as its native counter part
 * which is defined in linux/arch/arm/include/asm/ptrace.h.
 * I guess we may need to do something different here in the future.
 */
static unsigned int lguest_processor_mode(struct pt_regs *regs)
{
	unsigned long mode = (regs->ARM_cpsr & MODE_MASK);
	return mode;
}


/*
 * Its native counter part is defined include/asm/processor.h.
 * When the Guest calls this function, it just returns to the
 * Host.
 */
static void lguest_cpu_relax(void)
{
	immediate_hcall(0, 0, 0, LHCALL_GUEST_BUSY_WAIT);
}


/* Defined in linux/arch/arm/mach-armlguest/kernel/lguest-entry-armv.S */
extern struct task_struct *__lguest_switch_to(struct task_struct *, struct thread_info *, struct thread_info *);


/* 
 * Get current Guest page table pointer, Everytime, we come back from 
 * the Host, the Switcher will set it for the Guest.
 */
static unsigned long lguest_translate_table(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	return lgregs->gpgdir;
}



/* 
 * Get current Guest domain register value, Everytime, we come back from 
 * the Host, the Switcher will set it for the Guest.
 */
static unsigned long lguest_domain_control(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	return lgregs->guest_domain;
}


/* 
 * Get current Control Register value, Everytime, we come back from 
 * the Host, the Switcher will set it for the Guest.
 */
static unsigned long lguest_ctrl_register(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;
	return lgregs->guest_ctrl;
}



//============Replace some function pointers in linux/arch/arm/kernel/process.c=============
/* Defined in linux/arch/arm/kernel/process.c for Lguest*/
extern void (* do_show_co_regs)(char *buf);
extern unsigned long (* kernel_thread_cpsr)(void);
extern void (* do_ret_from_fork)(void);

static void lguest_show_co_regs(char *buf)
{
	unsigned int ctrl;

	buf[0] = '\0';
	{
		unsigned int transbase, dac;

		transbase = lguest_translate_table();
				dac = lguest_domain_control();
		snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, dac);
	}
	ctrl = lguest_ctrl_register();
	printk("Control: %08x%s\n", ctrl, buf);
	return;
}

/*
 * We are the Guest. Even the Guest'kernel runs under USER MODE.
 */
static unsigned long lguest_kernel_thread_cpsr(void)
{
	return USR_MODE | PSR_ENDSTATE | PSR_ISETSTATE;

}

/* Please see  linux/arch/arm/lguest/kernel/lguest-entry-common.S */
asmlinkage void lguest_ret_from_fork(void) __asm__("lguest_ret_from_fork");

//============Replace some function pointers in linux/arch/arm/kernel/process.c=============




//============Replace some function pointers in linux/arch/arm/kernel/traps.c=============
extern void (* do_set_tls)(unsigned long tls);

static void lguest_set_tls(unsigned long tls)
{
	lazy_hcall(tls, 0, 0, LHCALL_SET_TLS);
}
//============Replace some function pointers in linux/arch/arm/kernel/traps.c=============



//============Replace some function pointers in linux/arch/arm/kernel/setup.c=============
/* The following three are defined in linux/arch/arm/kernel/setup.c for Lguest*/
extern int ( *do_get_cpu_architecture)(void);
extern void (*do_cpu_tcm_init)(void);
extern void (*do_early_trap_init)(void);
/* 
 * The Host save value for the Guest, we just read it from the
 * the 1st page of "struct lguest_pages."
 */
static int lguest_cpu_architecture(void)
{
	struct lguest_regs *lgregs = (struct lguest_regs *)lguest_page_base;

	return lgregs->guest_cpu_arch;
}

/* We do nothing here.*/
static void lguest_cpu_tcm_init(void)
{

}

/* We do nothing here.*/
static void lguest_early_trap_init(void)
{

}
//============Replace some function pointers in linux/arch/arm/kernel/setup.c=============


/* Powerdown function for Guest please see linux/arch/arm/kernel/process.c*/
static void lguest_power_off(void)
{
	immediate_hcall(__pa("Power down"), LGUEST_SHUTDOWN_POWEROFF,
					 0, LHCALL_SHUTDOWN);
}


extern void setup_mm_for_reboot(char mode);
/* restart code of Guest. please see linux/arch/arm/kernel/process.c */
static void lguest_restart(char mode, const char *cmd)
{
	cpu_proc_fin();

	/*
	 * Tell the mm system that we are going to reboot -
	 * we may need it to insert some 1:1 mappings so that
	 * soft boot works.
	 */
	setup_mm_for_reboot(mode);
	immediate_hcall(__pa(cmd), LGUEST_SHUTDOWN_RESTART,
			 0, LHCALL_SHUTDOWN);
}



/* Defined in linux/kernel/arch/arm/lguest/kernel/lguest-entry-common.S for Lguest */
extern void lguest_ret_to_user(void);
/* Defined in linux/kernel/arch/arm/kernel/sys_arm.c for Lguest*/
extern void (* do_ret_to_user)(void);




/* 
 * Reserve the "virtual memory area" of the Switcher for the Guest. 
 * The Switcher code must be at the same virtual address in the Guest 
 * as the Host. I put this function in "early initcall" section, 
 * I hope it is not too late to reserve "virtual memory area" of 
 * switcher for guest.
 */
static int __init guest_reserve_switch_space(void)
{
	struct vm_struct *switcher_vma;
	unsigned long addr;
	
	/* we do nothing if we are the host*/
	if(!lguest_guest_run())
		return 0;

	/* 
	 * lguest_switcher_addr was set in lguest_kernel_start. Please see
	 * linux/arch/arm/mach-armlguest/kerenl/lguest-entry-armv.S 
	 */
	addr = (unsigned long)lguest_switcher_addr;

	/* 
	 * reserve SWITCHER_TOTAL_SIZE(2M) address space for guest.
	 *  __get_vm_area allocates an extra guard page.
	 */
	switcher_vma = __get_vm_area(SWITCHER_TOTAL_SIZE - PAGE_SIZE,
							 VM_ALLOC, addr, addr + SWITCHER_TOTAL_SIZE);
	if (!switcher_vma) {
		panic("lguest: could not reserve address space of switcher for guest.\n");
	}
	printk("Reserve address space from %lx to %lx for Guest \n", 
					addr, addr + SWITCHER_TOTAL_SIZE - 1);
	return 0;
}
early_initcall(guest_reserve_switch_space);



/*
 * We just replace arch_initcalls and late_initcalls with a no-op function. 
 */
static int lguest_arch_initcalls(void)
{
	return 0;	
}



/* The following varibles are define in linux/include/asm-generic/vmlinux.lds.h */
extern initcall_t __arch_initcall_start[], __arch_initcall_end[];
extern initcall_t __late_initcall_start[], __late_initcall_end[];


/*
 * Some arch_initcalls and late_initcalls which access hardware or call the 
 * privileged instructions have to be replaced. I know it is very rude to do 
 * it this way, So I Badly need some advices. 
 */
static void replace_arch_initcalls(void)
{
	initcall_t *fn;

	for (fn = __arch_initcall_start; fn < __arch_initcall_end; fn++)
		*fn = &lguest_arch_initcalls;

	for (fn = __late_initcall_start; fn < __late_initcall_end; fn++)
		*fn = &lguest_arch_initcalls;
}



/*G:029
 * Once we get to lguest_init(), we know we're a Guest.  The various
 * avm_*_ops structures in the kernel provide points for (almost) every 
 * routine we have to override to avoid privileged instructions.
 */
__init void lguest_init(void)
{


	lguest_guest_start = 1;	

	/*
	 * We are the Guest. we replace all native_* operations which are defined
	 * in linux/arch/arm/include/asm/lguest_native.h.
	 * please also see linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c,
	 * linux/arch/arm/include/asm/lguest_native.h,  
	 * linux/arch/arm/include/asm/lguest_privileged_fp.h, and 
	 * linux/arch/arm/include/asm/lguest_privileged_ops.h. 
	 */
	avm_irq_ops.m2f_raw_local_irq_save = lguest_raw_local_irq_save;
	avm_irq_ops.raw_local_irq_enable = lguest_raw_local_irq_enable;
	avm_irq_ops.raw_local_irq_disable = lguest_raw_local_irq_disable;
	avm_irq_ops.m2f_raw_local_save_flags = lguest_raw_local_save_flags;
	avm_irq_ops.raw_local_irq_restore = lguest_raw_local_irq_restore;
	avm_irq_ops.local_fiq_enable = lguest_local_fiq_enable;
	avm_irq_ops.local_fiq_disable = lguest_local_fiq_disable;


	avm_cpu_ops.read_cpuid = lguest_read_cpuid;
	avm_cpu_ops.read_cpuid_ext = lguest_read_cpuid_ext;
	avm_cpu_ops.get_cr = lguest_get_cr;
	avm_cpu_ops.set_cr = lguest_set_cr;
	avm_cpu_ops.get_copro_access = lguest_get_copro_access;
	avm_cpu_ops.set_copro_access = lguest_set_copro_access;
	avm_cpu_ops.user_mode = lguest_user_mode;
	avm_cpu_ops.processor_mode = lguest_processor_mode;
	avm_cpu_ops.cpu_relax = lguest_cpu_relax;
	

	avm_mmu_ops.local_flush_tlb_all = lguest_local_flush_tlb_all;
	avm_mmu_ops.local_flush_tlb_mm = lguest_local_flush_tlb_mm;
	avm_mmu_ops.local_flush_tlb_page = lguest_local_flush_tlb_page;
	avm_mmu_ops.local_flush_tlb_kernel_page = lguest_local_flush_tlb_kernel_page;
	avm_mmu_ops.flush_pmd_entry = lguest_flush_pmd_entry;
	avm_mmu_ops.clean_pmd_entry = lguest_clean_pmd_entry;
	avm_mmu_ops.enter_lazy_tlb = lguest_enter_lazy_tlb;
	avm_mmu_ops.switch_mm = lguest_switch_mm;
	avm_mmu_ops.switch_to = __lguest_switch_to;
	avm_mmu_ops.set_pte_at = lguest_set_pte_at;
	avm_mmu_ops.__pmd_populate = __lguest_pmd_populate;
	avm_mmu_ops.copy_pmd = lguest_copy_pmd;
	avm_mmu_ops.pmd_clear = lguest_pmd_clear;
	avm_mmu_ops.pte_clear = lguest_pte_clear;
	avm_mmu_ops.cpu_get_pgd = lguest_cpu_get_pgd;
	avm_mmu_ops.set_domain = lguest_set_domain;
	avm_mmu_ops.__flush_icache_all = __lguest_flush_icache_all;


	
	/* 
	 * Please see kernel_execve in linux/arch/arm/kernel/sys_arm.c.
	 * The Guest kernel has its own path to return to user space.
	 */
	do_ret_to_user = lguest_ret_to_user;

	/* 
	 * Please see copy_thread in linux/arch/arm/kernel/process.c.
	 * The Guest kernel has its own path to return from fork.
	 */
	do_ret_from_fork = lguest_ret_from_fork;

	/* 
	 * Please see __show_regs in linux/arch/arm/kernel/process.c
	 * The Guest cannot call the privileged instructions, and have 
	 * to ask the Host to handle it for the Guest.
	 */
	do_show_co_regs = lguest_show_co_regs;
    
	/* 
	 * Please see kernel_thread in linux/arch/arm/kernel/process.c
	 * The Guest cannot call the privileged instructions, and have 
	 * to ask the Host to handle it for the Guest.
	 */
	kernel_thread_cpsr = lguest_kernel_thread_cpsr;

	/* 
	 * Please see arm_syscall in linux/arch/arm/kernel/traps.c
	 * Guest cannot use the privileged instruction. Tell the Host
	 * to handle it for the Guest.
	 */
	do_set_tls = lguest_set_tls;

	/* 
	 * Please see cpu_architecture in linux/arch/arm/kernel/setup.c
	 * The Guest cannot use the privileged instructions, and have 
	 * to ask the Host to handle it for the Guest.
	 */
	do_get_cpu_architecture = lguest_cpu_architecture;

	/* 
	 * Please see setup_arch in linux/arch/arm/kernel/setup.c
	 */
	do_cpu_tcm_init = lguest_cpu_tcm_init;

	/* 
	 * Please see setup_arch in linux/arch/arm/kernel/setup.c
	 * The Host have already initilized the trap vectors for the 
	 * Guest, and we need to do nothing here.
	 */
	do_early_trap_init = lguest_early_trap_init;

	lockdep_init();

	/* Hook in our special panic hypercall code. */
	atomic_notifier_chain_register(&panic_notifier_list, &paniced);

	/*
	 * We set the preferred console to "hvc".  This is the "hypervisor
	 * virtual console" driver written by the PowerPC people, which we also
	 * adapted for lguest's use.
	 */
	add_preferred_console("hvc", 0, NULL);


	/* Register our very early console. */
	virtio_cons_early_init(early_put_chars);

	/*
	 * Last of all, we set the power management poweroff hook to point to
	 * the Guest routine to power off, and the reboot hook to our restart
	 * routine. please see linux/arch/arm/kernel/process.c
	 */
	pm_power_off = lguest_power_off;
	arm_pm_restart = lguest_restart; 

	
	replace_arch_initcalls();

	/*
	 *  We can start kernel now!
	 */
	start_kernel();
}
/*
 * This marks the end of stage II of our journey, The Guest.
 *
 * It is now time for us to explore the layer of virtual drivers and complete
 * our understanding of the Guest in "make Drivers".
 */

