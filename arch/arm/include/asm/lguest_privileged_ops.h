#ifndef _ASM_LGUEST_PRIVILEGED_OPS_H
#define _ASM_LGUEST_PRIVILEGED_OPS_H


#ifndef __ASSEMBLY__
#include <asm/lguest_privileged_fp.h>

/*
 * The original ones of the following inline functions are defined in some header files which
 * are under linux/arch/arm/include/asm/ directory. The original ones call privileged
 * instructions. We change them to call function pointers, so the Guest kernel can change them 
 * to pointer at its own functions when the Guest kernel boots up.
 * Please also see linux/arch/arm/include/asm/lguest_privileged_fp.h,
 * linux/arch/arm/include/asm/lguest_native.h, and
 * linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c 
 * linux/arch/arm/mach-armlguest/kernel/boot.c
 *
 */


#define CPUID_EXT_PFR0  0x10
#define CPUID_EXT_PFR1  0x11
#define CPUID_EXT_DFR0  0x12
#define CPUID_EXT_AFR0  0x13
#define CPUID_EXT_MMFR0 0x14
#define CPUID_EXT_MMFR1 0x15
#define CPUID_EXT_MMFR2 0x16
#define CPUID_EXT_MMFR3 0x17
#define CPUID_EXT_ISAR0 0x20
#define CPUID_EXT_ISAR1 0x21
#define CPUID_EXT_ISAR2 0x22
#define CPUID_EXT_ISAR3 0x23
#define CPUID_EXT_ISAR4 0x24
#define CPUID_EXT_ISAR5 0x25



/*
 * Save the current interrupt enable state.
 */

/* linux/arch/arm/include/asm/irqflags.h */
#define raw_local_save_flags(x)             \
	m2f_raw_local_save_flags(&x)

/* linux/arch/arm/include/asm/irqflags.h */
#define raw_local_irq_save(x)       \
    m2f_raw_local_irq_save(&x)      \


/* linux/arch/arm/include/asm/system.h */
#define switch_to(prev,next,last)                   \
do {												\
    last = avm_mmu_ops.switch_to(prev,task_thread_info(prev), task_thread_info(next));    \
} while (0)


/* linux/arch/arm/include/asm/domain.h */
static inline void  set_domain(unsigned long domain)
{
	avm_mmu_ops.set_domain(domain);
}

/* linux/arch/arm/include/asm/pgalloc.h */
static inline void __pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
	avm_mmu_ops.__pmd_populate(pmdp, pmdval);
}


/* linux/arch/arm/include/asm/pgtable.h */
static inline void copy_pmd(pmd_t *pmdp, pmd_t *pmdps)
{
	avm_mmu_ops.copy_pmd(pmdp, pmdps);
} 


/* linux/arch/arm/include/asm/pgtable.h */
static inline void pmd_clear(pmd_t *pmdp)
{
	avm_mmu_ops.pmd_clear(pmdp);
}

/* linux/arch/arm/include/asm/pgtable.h */
static inline void set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pteval)
{
	avm_mmu_ops.set_pte_at(mm, addr, ptep, pteval); 
}

/* linux/arch/arm/include/asm/pgtable.h */
static inline void pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	avm_mmu_ops.pte_clear(mm, addr, ptep);    
}

/* please see macro raw_local_irq_save above */
static inline void m2f_raw_local_irq_save(unsigned long *px)
{
	avm_irq_ops.m2f_raw_local_irq_save(px);
}


/* pleasea see macro raw_local_save_flags */
static inline void m2f_raw_local_save_flags(unsigned long *px)
{
	avm_irq_ops.m2f_raw_local_save_flags(px);
}


/* linux/arch/arm/include/asm/irqflags.h */
static inline void raw_local_irq_enable(void)
{
	avm_irq_ops.raw_local_irq_enable();	
}


/* linux/arch/arm/include/asm/irqflags.h */
static inline void raw_local_irq_disable(void)
{
	avm_irq_ops.raw_local_irq_disable();
}


/* linux/arch/arm/include/asm/irqflags.h */
static inline void local_fiq_enable(void)
{
	avm_irq_ops.local_fiq_enable();
}


/* linux/arch/arm/include/asm/irqflags.h */
static inline void local_fiq_disable(void)
{
	avm_irq_ops.local_fiq_disable();
}


/* linux/arch/arm/include/asm/irqflags.h */
static inline void raw_local_irq_restore(unsigned long x)
{
	avm_irq_ops.raw_local_irq_restore(x);
}


/* linux/arch/arm/include/asm/tlbflush.h */
static inline void local_flush_tlb_all(void)
{
	avm_mmu_ops.local_flush_tlb_all();
}

/* linux/arch/arm/include/asm/tlbflush.h */
static inline void local_flush_tlb_mm(struct mm_struct *mm)
{
	avm_mmu_ops.local_flush_tlb_mm(mm);
}

/* linux/arch/arm/include/asm/tlbflush.h */
static inline void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	avm_mmu_ops.local_flush_tlb_page(vma, uaddr);
}

/* linux/arch/arm/include/asm/tlbflush.h */
static inline void local_flush_tlb_kernel_page(unsigned long kaddr)
{
    avm_mmu_ops.local_flush_tlb_kernel_page(kaddr);
}

/* linux/arch/arm/include/asm/tlbflush.h */
static inline void flush_pmd_entry(pmd_t *pmd)
{
	avm_mmu_ops.flush_pmd_entry(pmd);
}

/* linux/arch/arm/include/asm/tlbflush.h */
static inline void clean_pmd_entry(pmd_t *pmd)
{
	avm_mmu_ops.clean_pmd_entry(pmd);
}


/* linux/arch/arm/include/asm/mmu_context.h */
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	avm_mmu_ops.enter_lazy_tlb(mm, tsk);
}


/* linux/arch/arm/include/asm/mmu_context.h */
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
      struct task_struct *tsk)
{
	avm_mmu_ops.switch_mm(prev, next, tsk);
}


/* linux/arch/arm/include/asm/proc-fns.h */
static inline pgd_t *cpu_get_pgd(void)
{
	return avm_mmu_ops.cpu_get_pgd();
}


/* linux/arch/arm/include/asm/cacheflush.h */
static inline void __flush_icache_all(void)
{
	avm_mmu_ops.__flush_icache_all();
}


//=============================================================
#define  __HAVE_ARCH_START_CONTEXT_SWITCH
static inline void arch_start_context_switch(struct task_struct *prev)
{
	avm_cpu_ops.start_context_switch(prev);
}

static inline void arch_end_context_switch(struct task_struct *next)
{
	avm_cpu_ops.end_context_switch(next);
}
//============================================================



/* linux/arch/arm/include/asm/cputype.h */
static inline unsigned int read_cpuid(unsigned long reg)
{
	return avm_cpu_ops.read_cpuid(reg);
}


/* linux/arch/arm/include/asm/cputype.h */
static inline unsigned int read_cpuid_ext(unsigned long ext_reg)
{
	return avm_cpu_ops.read_cpuid_ext(ext_reg);
}


/* linux/arch/arm/include/asm/system.h */
static inline unsigned int get_cr(void)
{
	return avm_cpu_ops.get_cr();
}


/* linux/arch/arm/include/asm/system.h */
static inline void set_cr(unsigned int val)
{
	avm_cpu_ops.set_cr(val);
}

/* linux/arch/arm/include/asm/system.h */
static inline unsigned int get_copro_access(void)
{
	return avm_cpu_ops.get_copro_access();
}

/* linux/arch/arm/include/asm/system.h */
static inline void set_copro_access(unsigned int val)
{
	avm_cpu_ops.set_copro_access(val);
}

/* linux/arch/arm/include/asm/processor.h */
static inline void cpu_relax(void)
{
	avm_cpu_ops.cpu_relax();
}


/* linux/arch/arm/include/asm/ptrace.h */
static inline int user_mode(struct pt_regs *regs)
{
	return avm_cpu_ops.user_mode(regs);
}

static inline unsigned int processor_mode(struct pt_regs *regs)
{
	return avm_cpu_ops.processor_mode(regs);
}
#endif /* __ASSEMBLY__ */
#endif /* _ASM_LGUEST_PRIVILEGED_OPS_H */
