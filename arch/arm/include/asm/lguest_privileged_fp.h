#ifndef _ASM_LGUEST_PRIVILEGED_FP_H
#define _ASM_LGUEST_PRIVILEGED_FP_H



#include <asm/page.h>
struct page;
struct thread_struct;
struct mm_struct;
struct task_struct;
struct cpumask;
struct vm_area_struct;
struct thread_info;


/*
 * The function pointers of varibles avm_cpu_ops, avm_irq_ops and and avm_mmu_ops 
 * in the following structures is initialized in
 * linux/arm/arch/mach-armlguest/kernel/lguest_privileged_ops_init.c, with
 * the functions defined in linux/arch/arm/include/asm/lguest_native.h.
 * please also see linux/arch/arm/include/asm/lguest_priviledged_ops.h
 */


struct avm_mmu_ops {
	void (*local_flush_tlb_all)(void);
	void (*local_flush_tlb_mm)(struct mm_struct *mm);
	void (*local_flush_tlb_page)(struct vm_area_struct *vma, unsigned long uaddr);
	void (*local_flush_tlb_kernel_page)(unsigned long kaddr);
	void (*flush_pmd_entry)(pmd_t *pmd);
	void (*clean_pmd_entry)(pmd_t *pmd);
	void (*enter_lazy_tlb)(struct mm_struct *mm, struct task_struct *tsk);
	void (*switch_mm)(struct mm_struct *prev, struct mm_struct *next,
								  struct task_struct *tsk);
	struct task_struct * (*switch_to)(struct task_struct *, 
						struct thread_info *, struct thread_info *);
	void (*set_pte_at)(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pte);
	void (*pte_clear)(struct mm_struct *mm, unsigned long addr, pte_t *ptep);
	void (*copy_pmd)(pmd_t *pmdp, pmd_t *pmdps);
	void (*pmd_clear)(pmd_t *pmdp);

	pgd_t* (*cpu_get_pgd)(void);
	void (*__flush_icache_all)(void);	
	void (* __pmd_populate)(pmd_t *pmdp, unsigned long pmdval);
	void (* set_domain)(unsigned long domain);						  
};





struct avm_irq_ops {
	void (*m2f_raw_local_irq_save)(unsigned long *px);
	void (*raw_local_irq_enable)(void);	
	void (*raw_local_irq_disable)(void);
	void (*m2f_raw_local_save_flags)(unsigned long *px);
	void (*raw_local_irq_restore)(unsigned long x);
	void (*local_fiq_enable)(void);
	void (*local_fiq_disable)(void);
};

struct avm_cpu_ops {
	unsigned int (*read_cpuid)(unsigned long reg);
	unsigned int (*read_cpuid_ext)(unsigned long ext_reg);

	unsigned int (*get_cr)(void);
	void (*set_cr)(unsigned int val);
	unsigned int (*get_copro_access)(void);
	void (*set_copro_access)(unsigned int val);
	int (*user_mode)(struct pt_regs *regs);
	unsigned int (*processor_mode)(struct pt_regs *regs);
	void (*cpu_relax)(void);

	void (*start_context_switch)(struct task_struct *prev);
	void (*end_context_switch)(struct task_struct *next);
};




extern struct avm_cpu_ops avm_cpu_ops;
extern struct avm_irq_ops avm_irq_ops;
extern struct avm_mmu_ops avm_mmu_ops;




/* In the future, We may implement lazy mode. */
enum avm_lazy_mode {
	AVM_LAZY_NONE,
	AVM_LAZY_CPU,
};

enum avm_lazy_mode avm_get_lazy_mode(void);

#endif  //_ASM_LGUEST_PRIVILEGED_FP_H
