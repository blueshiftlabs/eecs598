/* linux/arch/arm/mach-armlguest/kernel/lguest_privileged_ops_init.c */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hardirq.h>
#include <asm/lguest_privileged_fp.h>
#include <asm/unified.h>
#include <asm/lguest-native.h>

#include <asm/lguest-hooks.h>

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

/* Define initial versions of hookable functions. */

#define HOOK(fn, extra...) \
LGUEST_HOOK_TYPE(fn) fn extra = LGUEST_NATIVE_NAME(fn); EXPORT_SYMBOL(fn)

/* arch/arm/include/asm/cacheflush.h */
HOOK(__flush_icache_all);

/* arch/arm/include/asm/cputype.h */
HOOK(read_cpuid);
HOOK(read_cpuid_ext);

#ifdef CONFIG_CPU_USE_DOMAINS
/* arch/arm/include/asm/domain.h */
HOOK(set_domain);
#endif

/* arch/arm/include/asm/irqflags.h */
HOOK(arch_local_irq_save);
HOOK(arch_local_irq_enable);
HOOK(arch_local_irq_disable);
HOOK(local_fiq_enable);
HOOK(local_fiq_disable);
HOOK(arch_local_save_flags);
HOOK(arch_local_irq_restore);

/* arch/arm/include/asm/mmu_context.h */
HOOK(enter_lazy_tlb);
HOOK(switch_mm);

/* arch/arm/include/asm/pgalloc.h */
HOOK(__pmd_populate);

/* arch/arm/include/asm/pgtable.h */
HOOK(copy_pmd);
HOOK(pmd_clear);
HOOK(pte_clear);
HOOK(set_pte_at);

/* arch/arm/include/asm/proc-fns.h */
HOOK(cpu_get_pgd);

/* arch/arm/include/asm/processor.h */
HOOK(cpu_relax);
HOOK(__switch_to);

/* arch/arm/include/asm/ptrace.h */
HOOK(user_mode);
HOOK(processor_mode);

/* arch/arm/include/asm/system.h */
HOOK(get_cr);
HOOK(set_cr);
HOOK(get_copro_access);
HOOK(set_copro_access);

/* arch/arm/include/asm/tlbflush.h */
HOOK(local_flush_tlb_all);
HOOK(local_flush_tlb_mm);
HOOK(local_flush_tlb_page);
HOOK(local_flush_tlb_kernel_page);
HOOK(flush_pmd_entry);
HOOK(clean_pmd_entry);

/* arch/arm/kernel/traps.c */
HOOK(syscall_set_tls);
HOOK(early_trap_init, __initdata);

/* arch/arm/kernel/process.c */
HOOK(show_coproc_regs);
HOOK(kernel_thread_cpsr);

/* arch/arm/kernel/setup.c */
HOOK(__get_cpu_architecture);
HOOK(cpu_tcm_init);
