#ifndef __ASM_LGUEST_NATIVE_H
#define __ASM_LGUEST_NATIVE_H

#ifndef __ASSEMBLY__
#include <asm/tlbflush.h>
#include <asm/cputype.h>
#include <asm/irqflags.h>
#include <asm/mmu_context.h>
#include <asm/proc-fns.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>


/*
 * The following native_* inline functions are collected from some header files which
 * are under linux/arch/arm/include/asm/ directory. These functions call privileged
 * instructions. They will be replaced through function pointers when the Guest Kernel 
 * boots up. 
 * Please also see linux/arch/arm/include/asm/lguest_privileged_fp.h,
 * linux/arch/arm/include/asm/lguest_privileged_ops.h, and
 * linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c 
 * linux/arch/arm/mach-armlguest/kernel/boot.c
 *
 */



/* defined in entry-armv. S*/
extern struct task_struct *__switch_to(struct task_struct *, struct thread_info *, struct thread_info *);

#define S_CPUID_EXT_PFR0  "c1, 0"
#define S_CPUID_EXT_PFR1  "c1, 1"
#define S_CPUID_EXT_DFR0  "c1, 2"
#define S_CPUID_EXT_AFR0  "c1, 3"
#define S_CPUID_EXT_MMFR0 "c1, 4"
#define S_CPUID_EXT_MMFR1 "c1, 5"
#define S_CPUID_EXT_MMFR2 "c1, 6"
#define S_CPUID_EXT_MMFR3 "c1, 7"
#define S_CPUID_EXT_ISAR0 "c2, 0"
#define S_CPUID_EXT_ISAR1 "c2, 1"
#define S_CPUID_EXT_ISAR2 "c2, 2"
#define S_CPUID_EXT_ISAR3 "c2, 3"
#define S_CPUID_EXT_ISAR4 "c2, 4"
#define S_CPUID_EXT_ISAR5 "c2, 5"


#define m_read_cpuid(reg, val)								\
	({														\
		asm("mrc    p15, 0, %0, c0, c0, " __stringify(reg)  \
			: "=r"(val)										\
			:												\
			: "memory","cc");								\
	})





#define m_read_cpuid_ext(ext_reg, val)			\
	({											\
		asm("mrc    p15, 0, %0, c0, " ext_reg	\
			: "=r"(val)							\
			:									\
			: "memory","cc");					\
	})




/* linux/arch/arch/include/asm/cputype.h */
static inline unsigned int native_read_cpuid(unsigned long reg)
{
	unsigned int val = 0;
	switch(reg) {
		case CPUID_ID:
			m_read_cpuid(CPUID_ID, val);
			break;
		case CPUID_CACHETYPE:
			m_read_cpuid(CPUID_CACHETYPE, val);
			break;
		case CPUID_TCM:
			m_read_cpuid(CPUID_TCM, val);
			break;
		case CPUID_TLBTYPE:
			m_read_cpuid(CPUID_TLBTYPE, val);
			break;
	}
	return val;							
}

/* linux/arch/arch/include/asm/cputype.h */
/* This is a pretty stupid idea, but I cannot figure out a better solution */
static inline unsigned int native_read_cpuid_ext(unsigned long ext_reg)
{
	unsigned int val = 0;
	switch(ext_reg) {
		case CPUID_EXT_PFR0:
			m_read_cpuid_ext(S_CPUID_EXT_PFR0, val);
			break;
		case CPUID_EXT_PFR1:
			m_read_cpuid_ext(S_CPUID_EXT_PFR1, val);
			break;
		case CPUID_EXT_DFR0:
			m_read_cpuid_ext(S_CPUID_EXT_DFR0, val);
			break;
		case CPUID_EXT_AFR0:
			m_read_cpuid_ext(S_CPUID_EXT_AFR0, val);
			break;
		case CPUID_EXT_MMFR0:
			m_read_cpuid_ext(S_CPUID_EXT_MMFR0, val);
			break;
		case CPUID_EXT_MMFR1:
			m_read_cpuid_ext(S_CPUID_EXT_MMFR1, val);
			break;
		case CPUID_EXT_MMFR2:
			m_read_cpuid_ext(S_CPUID_EXT_MMFR2, val);
			break;
		case CPUID_EXT_MMFR3:
			m_read_cpuid_ext(S_CPUID_EXT_MMFR3, val);
			break;

		case CPUID_EXT_ISAR0:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR0, val);
			break;
		case CPUID_EXT_ISAR1:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR1, val);
			break;
		case CPUID_EXT_ISAR2:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR2, val);
			break;
		case CPUID_EXT_ISAR3:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR3, val);
			break;
		case CPUID_EXT_ISAR4:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR4, val);
			break;
		case CPUID_EXT_ISAR5:
			m_read_cpuid_ext(S_CPUID_EXT_ISAR5, val);
			break;
	}
	return val;
}


#if __LINUX_ARM_ARCH__ >= 6
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_save(unsigned long *px)
{
	unsigned long x;
	__asm__ __volatile__(                   
	"mrs %0, cpsr        @ local_irq_save\n" 
	"cpsid  i\n"                        
	"str %0, [%1] \n"                       
	: "=&r"(x) 
	: "r"(px) 
	: "memory", "cc");  
}


/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_enable(void)
{
	__asm__ __volatile__("cpsie i    @ __sti" : : : "memory", "cc");
}


/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_disable(void)
{
	__asm__ __volatile__("cpsid i    @ __cli" : : : "memory", "cc");
}


/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_local_fiq_enable(void)
{
	__asm__ __volatile__("cpsie f    @ __stf" : : : "memory", "cc");
}

/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_local_fiq_disable(void)
{
	__asm__ __volatile__("cpsid f    @ __clf" : : : "memory", "cc");
}


#else

/*
 * Save the current interrupt enable state & disable IRQs
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_save(unsigned long *px)
{
	unsigned long x; 
	
	__asm__ __volatile__(                   
	"mrs    %0, cpsr        @ local_irq_save\n" 
	"str %0, [%1] \n"                       
	"orr %0, %0, #128\n"                 
	"msr cpsr_c, %0"                 
	: "=&r" (x)                 
	: "r"(px)                          
	: "memory", "cc");      
}


/*
 * Enable IRQs
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_enable(void)
{
	unsigned long x;

	__asm__ __volatile__(
	"mrs    %0, cpsr        @ local_irq_save\n"
	"bic %0, %0, #128\n"
	"msr cpsr_c, %0"
	: "=r" (x)
	: 
	: "memory", "cc");
}

/*
 * Disable IRQs
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_disable(void)
{
    unsigned long x;

	__asm__ __volatile__(
	"mrs    %0, cpsr        @ local_irq_save\n"
	"orr %0, %0, #128\n"
	"msr cpsr_c, %0"
	: "=r" (x)
	: 
	: "memory", "cc");
}

/*
 * Enable FIQs
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_local_fiq_enable(void)
{
    unsigned long x;

	__asm__ __volatile__(
	"mrs    %0, cpsr        @ stf\n"
	"bic %0, %0, #64\n"
	"msr cpsr_c, %0"
	: "=r" (x)
	:
	: "memory", "cc");
}

/*
 * Disable FIQs
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_local_fiq_disable(void)
{
	unsigned long x;

	__asm__ __volatile__(
	"mrs    %0, cpsr        @ clf\n"
	"orr %0, %0, #64\n"
	"msr cpsr_c, %0"
	: "=r" (x)
	:
	: "memory", "cc");
}
#endif


/*
 * Save the current interrupt enable state.
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_save_flags(unsigned long *px)
{
	unsigned long x;

	__asm__ __volatile__(                   
	"mrs %0, cpsr        @ local_save_flags\n"                         
	: "=r"(x) 
	:  
	: "memory", "cc");  
	*px = x;	
} 

/*
 * restore saved IRQ & FIQ state
 */
/* linux/arch/arch/include/asm/irqflags.h */
static inline void native_raw_local_irq_restore(unsigned long x)
{
	__asm__ __volatile__(					
	"msr	cpsr_c, %0		@ local_irq_restore\n"	
	:							
	: "r" (x)						
	: "memory", "cc");
}  




/* linux/arch/arch/include/asm/mmu_context.h */
static inline void native_enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{

}

/*
 * This is the actual mm switch as far as the scheduler
 * is concerned.  No registers are touched.  We avoid
 * calling the CPU specific function when the mm hasn't
 * actually changed.
 */
/* linux/arch/arch/include/asm/mmu_context.h */
static inline void native_switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk)
{
#ifdef CONFIG_MMU
	unsigned int cpu = smp_processor_id();

#ifdef CONFIG_SMP
	/* check for possible thread migration */
	if (!cpumask_empty(mm_cpumask(next)) &&
	    !cpumask_test_cpu(cpu, mm_cpumask(next)))
		__flush_icache_all();
#endif
	if (!cpumask_test_and_set_cpu(cpu, mm_cpumask(next)) || prev != next) {
		check_context(next);
		cpu_switch_mm(next->pgd, next);
		if (cache_is_vivt())
			cpumask_clear_cpu(cpu, mm_cpumask(prev));
	}
#endif
}

/* linux/arch/arch/include/asm/proc-fns.h */
static inline pgd_t * native_cpu_get_pgd(void)	
{						
	unsigned long pg;

	__asm__("mrc	p15, 0, %0, c2, c0, 0"	
			 : "=r" (pg) : : "cc");		
	pg &= ~0x3fff;				
	return (pgd_t *)phys_to_virt(pg);		
}


/* linux/arch/arch/include/asm/cacheflush.h */
static inline void __native_flush_icache_all(void)
{
#ifdef CONFIG_ARM_ERRATA_411920
	extern void v6_icache_inval_all(void);
	v6_icache_inval_all();
#else
	asm("mcr    p15, 0, %0, c7, c5, 0   @ invalidate I-cache\n"
			:
			: "r" (0));
#endif
}


/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_local_flush_tlb_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_WB))
		dsb();

	if (tlb_flag(TLB_V3_FULL))
		asm("mcr p15, 0, %0, c6, c0, 0" : : "r" (zero) : "cc");
	if (tlb_flag(TLB_V4_U_FULL | TLB_V6_U_FULL))
		asm("mcr p15, 0, %0, c8, c7, 0" : : "r" (zero) : "cc");
	if (tlb_flag(TLB_V4_D_FULL | TLB_V6_D_FULL))
		asm("mcr p15, 0, %0, c8, c6, 0" : : "r" (zero) : "cc");
	if (tlb_flag(TLB_V4_I_FULL | TLB_V6_I_FULL))
		asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");
	if (tlb_flag(TLB_V7_UIS_FULL))
		asm("mcr p15, 0, %0, c8, c3, 0" : : "r" (zero) : "cc");

	if (tlb_flag(TLB_BTB)) {
		/* flush the branch target cache */
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero) : "cc");
		dsb();
		isb();
	}
}

/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_local_flush_tlb_mm(struct mm_struct *mm)
{
	const int zero = 0;
	const int asid = ASID(mm);
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_WB))
		dsb();

	if (cpumask_test_cpu(get_cpu(), mm_cpumask(mm))) {
		if (tlb_flag(TLB_V3_FULL))
			asm("mcr p15, 0, %0, c6, c0, 0" : : "r" (zero) : "cc");
		if (tlb_flag(TLB_V4_U_FULL))
			asm("mcr p15, 0, %0, c8, c7, 0" : : "r" (zero) : "cc");
		if (tlb_flag(TLB_V4_D_FULL))
			asm("mcr p15, 0, %0, c8, c6, 0" : : "r" (zero) : "cc");
		if (tlb_flag(TLB_V4_I_FULL))
			asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");
	}
	put_cpu();

	if (tlb_flag(TLB_V6_U_ASID))
		asm("mcr p15, 0, %0, c8, c7, 2" : : "r" (asid) : "cc");
	if (tlb_flag(TLB_V6_D_ASID))
		asm("mcr p15, 0, %0, c8, c6, 2" : : "r" (asid) : "cc");
	if (tlb_flag(TLB_V6_I_ASID))
		asm("mcr p15, 0, %0, c8, c5, 2" : : "r" (asid) : "cc");
	if (tlb_flag(TLB_V7_UIS_ASID))
		asm("mcr p15, 0, %0, c8, c3, 2" : : "r" (asid) : "cc");

	if (tlb_flag(TLB_BTB)) {
		/* flush the branch target cache */
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero) : "cc");
		dsb();
	}
}

/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_local_flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	uaddr = (uaddr & PAGE_MASK) | ASID(vma->vm_mm);

	if (tlb_flag(TLB_WB))
		dsb();

	if (cpumask_test_cpu(smp_processor_id(), mm_cpumask(vma->vm_mm))) {
		if (tlb_flag(TLB_V3_PAGE))
			asm("mcr p15, 0, %0, c6, c0, 0" : : "r" (uaddr) : "cc");
		if (tlb_flag(TLB_V4_U_PAGE))
			asm("mcr p15, 0, %0, c8, c7, 1" : : "r" (uaddr) : "cc");
		if (tlb_flag(TLB_V4_D_PAGE))
			asm("mcr p15, 0, %0, c8, c6, 1" : : "r" (uaddr) : "cc");
		if (tlb_flag(TLB_V4_I_PAGE))
			asm("mcr p15, 0, %0, c8, c5, 1" : : "r" (uaddr) : "cc");
		if (!tlb_flag(TLB_V4_I_PAGE) && tlb_flag(TLB_V4_I_FULL))
			asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");
	}

	if (tlb_flag(TLB_V6_U_PAGE))
		asm("mcr p15, 0, %0, c8, c7, 1" : : "r" (uaddr) : "cc");
	if (tlb_flag(TLB_V6_D_PAGE))
		asm("mcr p15, 0, %0, c8, c6, 1" : : "r" (uaddr) : "cc");
	if (tlb_flag(TLB_V6_I_PAGE))
		asm("mcr p15, 0, %0, c8, c5, 1" : : "r" (uaddr) : "cc");
	if (tlb_flag(TLB_V7_UIS_PAGE))
		asm("mcr p15, 0, %0, c8, c3, 1" : : "r" (uaddr) : "cc");

	if (tlb_flag(TLB_BTB)) {
		/* flush the branch target cache */
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero) : "cc");
		dsb();
	}
}


/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_local_flush_tlb_kernel_page(unsigned long kaddr)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	kaddr &= PAGE_MASK;

	if (tlb_flag(TLB_WB))
		dsb();

	if (tlb_flag(TLB_V3_PAGE))
		asm("mcr p15, 0, %0, c6, c0, 0" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V4_U_PAGE))
		asm("mcr p15, 0, %0, c8, c7, 1" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V4_D_PAGE))
		asm("mcr p15, 0, %0, c8, c6, 1" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V4_I_PAGE))
		asm("mcr p15, 0, %0, c8, c5, 1" : : "r" (kaddr) : "cc");
	if (!tlb_flag(TLB_V4_I_PAGE) && tlb_flag(TLB_V4_I_FULL))
		asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");

	if (tlb_flag(TLB_V6_U_PAGE))
		asm("mcr p15, 0, %0, c8, c7, 1" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V6_D_PAGE))
		asm("mcr p15, 0, %0, c8, c6, 1" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V6_I_PAGE))
		asm("mcr p15, 0, %0, c8, c5, 1" : : "r" (kaddr) : "cc");
	if (tlb_flag(TLB_V7_UIS_PAGE))
		asm("mcr p15, 0, %0, c8, c3, 1" : : "r" (kaddr) : "cc");

	if (tlb_flag(TLB_BTB)) {
		/* flush the branch target cache */
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero) : "cc");
		dsb();
		isb();
	}
}

/*
 *	flush_pmd_entry
 *
 *	Flush a PMD entry (word aligned, or double-word aligned) to
 *	RAM if the TLB for the CPU we are running on requires this.
 *	This is typically used when we are creating PMD entries.
 *
 *	clean_pmd_entry
 *
 *	Clean (but don't drain the write buffer) if the CPU requires
 *	these operations.  This is typically used when we are removing
 *	PMD entries.
 */
/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_flush_pmd_entry(pmd_t *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_DCLEAN))
		asm("mcr	p15, 0, %0, c7, c10, 1	@ flush_pmd"
			: : "r" (pmd) : "cc");

	if (tlb_flag(TLB_L2CLEAN_FR))
		asm("mcr	p15, 1, %0, c15, c9, 1  @ L2 flush_pmd"
			: : "r" (pmd) : "cc");

	if (tlb_flag(TLB_WB))
		dsb();
}

/* linux/arch/arch/include/asm/tlbflush.h */
static inline void native_clean_pmd_entry(pmd_t *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_DCLEAN))
		asm("mcr	p15, 0, %0, c7, c10, 1	@ flush_pmd"
			: : "r" (pmd) : "cc");

	if (tlb_flag(TLB_L2CLEAN_FR))
		asm("mcr	p15, 1, %0, c15, c9, 1  @ L2 flush_pmd"
			: : "r" (pmd) : "cc");
}


/* linux/arch/arch/include/asm/pgalloc.h */
static inline void native_set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pteval)
{
	set_pte_ext(ptep, pteval, (addr) >= TASK_SIZE ? 0 : PTE_EXT_NG); 
}


/* linux/arch/arch/include/asm/pgalloc.h */
static inline void native_pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	set_pte_ext(ptep, __pte(0), 0);
}


/* linux/arch/arch/include/asm/pgalloc.h */
static inline void __native_pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
	pmdp[0] = __pmd(pmdval);
	pmdp[1] = __pmd(pmdval + 256 * sizeof(pte_t));
	flush_pmd_entry(pmdp);
}


/* linux/arch/arch/include/asm/pgalloc.h */
static inline void native_copy_pmd(pmd_t *pmdpd, pmd_t *pmdps)
{
	pmdpd[0] = pmdps[0];
	pmdpd[1] = pmdps[1];
	flush_pmd_entry(pmdpd);
}


/* linux/arch/arch/include/asm/pgalloc.h */
static inline void native_pmd_clear(pmd_t *pmdp)
{
	pmdp[0] = __pmd(0);
	pmdp[1] = __pmd(0);
	clean_pmd_entry(pmdp);
}


/* linux/arch/arch/include/asm/system.h */
static inline unsigned int native_get_cr(void)
{
	unsigned int val;

	asm("mrc p15, 0, %0, c1, c0, 0  @ get CR" : "=r" (val) : : "cc");
	return val;
}


/* linux/arch/arch/include/asm/system.h */
static inline void native_set_cr(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 0 @ set CR"
		: : "r" (val) : "cc");
	isb();
}


/* linux/arch/arch/include/asm/system.h */
static inline unsigned int native_get_copro_access(void)
{
	unsigned int val;

	asm("mrc p15, 0, %0, c1, c0, 2 @ get copro access"
		: "=r" (val) : : "cc");
	return val;
}


/* linux/arch/arch/include/asm/system.h */
static inline void native_set_copro_access(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 2 @ set copro access"
		: : "r" (val) : "cc");
	isb();
}

/* linux/arch/arch/include/asm/domain.h */
static inline void native_set_domain(unsigned long domain)
{
	__asm__ __volatile__(               \
		"mcr    p15, 0, %0, c3, c0  @ set domain"   \
	      : : "r" (domain));                 \
	isb();                      \
}


/* linux/arch/arch/include/asm/ptrace.h */
static inline int native_user_mode(struct pt_regs *regs)
{
	return ((regs->ARM_cpsr & 0xf) == 0);
}

/* linux/arch/arch/include/asm/ptrace.h */
static inline unsigned int native_processor_mode(struct pt_regs *regs)
{
	return ((regs)->ARM_cpsr & MODE_MASK);
}


/* linux/arch/arch/include/asm/processor.h */
static inline void native_cpu_relax(void)
{
	barrier();	
}


#undef tlb_flag
#undef always_tlb_flags
#undef possible_tlb_flags

#endif //__ASSEMBLY__
#endif //__ASM_LGUEST_NATIVE_H

