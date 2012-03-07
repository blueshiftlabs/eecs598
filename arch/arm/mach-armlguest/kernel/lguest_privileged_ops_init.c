/* linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hardirq.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/lguest_privileged_fp.h>
#include <asm/lguest_native.h>
#include <asm/unified.h>

/*
 * The following native_* functions are defined linux/arch/arm/include/asm/lguest_native.h
 * These inline functions call privileged instructions. They will be replaced when the Guest
 * kernel boots up.
 * Please also see linux/arch/arm/include/asm/lguest_privileged_fp.h,
 * linux/arch/arm/include/asm/lguest_privileged_ops.h and 
 * linux/arch/arm/mach-armlguest/kernel/boot.c
 */

static void avm_nop_context_switch(struct task_struct *prev)
{

}

/*
 * The lazy hypercall has not been implemented by now.
 * we just return AVM_LAZY_NONE, so lazy_hcall works the same
 * way as immediate_hcall. 
 */
enum avm_lazy_mode avm_get_lazy_mode(void)
{
	/* AVM_LAZY_NONE is defined in linux/arch/arm/include/asm/lguest_privileged_fp.h */
	return AVM_LAZY_NONE;
}


struct avm_irq_ops avm_irq_ops = {
	.m2f_raw_local_irq_save = native_raw_local_irq_save,
	.raw_local_irq_enable = native_raw_local_irq_enable,
	.raw_local_irq_disable = native_raw_local_irq_disable,
	.m2f_raw_local_save_flags = native_raw_local_save_flags,
	.raw_local_irq_restore = native_raw_local_irq_restore,
	.local_fiq_enable = native_local_fiq_enable,
	.local_fiq_disable = native_local_fiq_disable,
};

struct avm_cpu_ops avm_cpu_ops = {

	.read_cpuid = native_read_cpuid,
	.read_cpuid_ext = native_read_cpuid_ext,

	.get_cr = native_get_cr,
	.set_cr = native_set_cr,	
	.get_copro_access = native_get_copro_access,
	.set_copro_access = native_set_copro_access,
	
	.user_mode = native_user_mode,
	.processor_mode = native_processor_mode,
	.cpu_relax = native_cpu_relax,
	 	
	.start_context_switch = avm_nop_context_switch,
	.end_context_switch = avm_nop_context_switch,
};




struct avm_mmu_ops avm_mmu_ops = {
	.local_flush_tlb_all = native_local_flush_tlb_all,
	.local_flush_tlb_mm = native_local_flush_tlb_mm,
	.local_flush_tlb_page = native_local_flush_tlb_page,
	.local_flush_tlb_kernel_page = native_local_flush_tlb_kernel_page,
	.flush_pmd_entry = native_flush_pmd_entry,
	.clean_pmd_entry = native_clean_pmd_entry,
	.enter_lazy_tlb = native_enter_lazy_tlb, 
	.switch_mm = native_switch_mm,
	.switch_to = __switch_to,
	.set_pte_at = native_set_pte_at,
	.pte_clear = native_pte_clear,
	.copy_pmd = native_copy_pmd,
	.pmd_clear = native_pmd_clear,

	.__flush_icache_all = __native_flush_icache_all,

	.__pmd_populate = __native_pmd_populate,
	.cpu_get_pgd = native_cpu_get_pgd,
	.set_domain = native_set_domain,
};


EXPORT_SYMBOL    (avm_cpu_ops);
EXPORT_SYMBOL    (avm_mmu_ops);
EXPORT_SYMBOL    (avm_irq_ops);
