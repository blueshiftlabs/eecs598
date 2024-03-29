/*
 * Copyright (C) 1995-2003 Russell King
 *               2001-2002 Keith Owens
 *     
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/glue-df.h>
#include <asm/glue-pf.h>
#include <asm/mach/arch.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
#include <asm/procinfo.h>
#include <asm/hardware/cache-l2x0.h>
#include <linux/kbuild.h>

#if defined(CONFIG_LGUEST) || defined(CONFIG_ARM_LGUEST_GUEST) || defined(CONFIG_LGUEST_MODULE)
#include "../mach-armlguest/driver/lg.h"
#endif

/*
 * Make sure that the compiler and target are compatible.
 */
#if defined(__APCS_26__)
#error Sorry, your compiler targets APCS-26 but this kernel requires APCS-32
#endif
/*
 * GCC 3.0, 3.1: general bad code generation.
 * GCC 3.2.0: incorrect function argument offset calculation.
 * GCC 3.2.x: miscompiles NEW_AUX_ENT in fs/binfmt_elf.c
 *            (http://gcc.gnu.org/PR8896) and incorrect structure
 *	      initialisation in fs/jffs2/erase.c
 */
#if (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#error Your compiler is too buggy; it is known to miscompile kernels.
#error    Known good compilers: 3.3
#endif

int main(void)
{
  DEFINE(TSK_ACTIVE_MM,		offsetof(struct task_struct, active_mm));
#ifdef CONFIG_CC_STACKPROTECTOR
  DEFINE(TSK_STACK_CANARY,	offsetof(struct task_struct, stack_canary));
#endif
  BLANK();
  DEFINE(TI_FLAGS,		offsetof(struct thread_info, flags));
  DEFINE(TI_PREEMPT,		offsetof(struct thread_info, preempt_count));
  DEFINE(TI_ADDR_LIMIT,		offsetof(struct thread_info, addr_limit));
  DEFINE(TI_TASK,		offsetof(struct thread_info, task));
  DEFINE(TI_EXEC_DOMAIN,	offsetof(struct thread_info, exec_domain));
  DEFINE(TI_CPU,		offsetof(struct thread_info, cpu));
  DEFINE(TI_CPU_DOMAIN,		offsetof(struct thread_info, cpu_domain));
  DEFINE(TI_CPU_SAVE,		offsetof(struct thread_info, cpu_context));
  DEFINE(TI_USED_CP,		offsetof(struct thread_info, used_cp));
  DEFINE(TI_TP_VALUE,		offsetof(struct thread_info, tp_value));
  DEFINE(TI_FPSTATE,		offsetof(struct thread_info, fpstate));
  DEFINE(TI_VFPSTATE,		offsetof(struct thread_info, vfpstate));
#ifdef CONFIG_SMP
  DEFINE(VFP_CPU,		offsetof(union vfp_state, hard.cpu));
#endif
#ifdef CONFIG_ARM_THUMBEE
  DEFINE(TI_THUMBEE_STATE,	offsetof(struct thread_info, thumbee_state));
#endif
#ifdef CONFIG_IWMMXT
  DEFINE(TI_IWMMXT_STATE,	offsetof(struct thread_info, fpstate.iwmmxt));
#endif
#ifdef CONFIG_CRUNCH
  DEFINE(TI_CRUNCH_STATE,	offsetof(struct thread_info, crunchstate));
#endif
  BLANK();
  DEFINE(S_R0,			offsetof(struct pt_regs, ARM_r0));
  DEFINE(S_R1,			offsetof(struct pt_regs, ARM_r1));
  DEFINE(S_R2,			offsetof(struct pt_regs, ARM_r2));
  DEFINE(S_R3,			offsetof(struct pt_regs, ARM_r3));
  DEFINE(S_R4,			offsetof(struct pt_regs, ARM_r4));
  DEFINE(S_R5,			offsetof(struct pt_regs, ARM_r5));
  DEFINE(S_R6,			offsetof(struct pt_regs, ARM_r6));
  DEFINE(S_R7,			offsetof(struct pt_regs, ARM_r7));
  DEFINE(S_R8,			offsetof(struct pt_regs, ARM_r8));
  DEFINE(S_R9,			offsetof(struct pt_regs, ARM_r9));
  DEFINE(S_R10,			offsetof(struct pt_regs, ARM_r10));
  DEFINE(S_FP,			offsetof(struct pt_regs, ARM_fp));
  DEFINE(S_IP,			offsetof(struct pt_regs, ARM_ip));
  DEFINE(S_SP,			offsetof(struct pt_regs, ARM_sp));
  DEFINE(S_LR,			offsetof(struct pt_regs, ARM_lr));
  DEFINE(S_PC,			offsetof(struct pt_regs, ARM_pc));
  DEFINE(S_PSR,			offsetof(struct pt_regs, ARM_cpsr));
  DEFINE(S_OLD_R0,		offsetof(struct pt_regs, ARM_ORIG_r0));
  DEFINE(S_FRAME_SIZE,		sizeof(struct pt_regs));
  BLANK();
#ifdef CONFIG_CACHE_L2X0
  DEFINE(L2X0_R_PHY_BASE,	offsetof(struct l2x0_regs, phy_base));
  DEFINE(L2X0_R_AUX_CTRL,	offsetof(struct l2x0_regs, aux_ctrl));
  DEFINE(L2X0_R_TAG_LATENCY,	offsetof(struct l2x0_regs, tag_latency));
  DEFINE(L2X0_R_DATA_LATENCY,	offsetof(struct l2x0_regs, data_latency));
  DEFINE(L2X0_R_FILTER_START,	offsetof(struct l2x0_regs, filter_start));
  DEFINE(L2X0_R_FILTER_END,	offsetof(struct l2x0_regs, filter_end));
  DEFINE(L2X0_R_PREFETCH_CTRL,	offsetof(struct l2x0_regs, prefetch_ctrl));
  DEFINE(L2X0_R_PWR_CTRL,	offsetof(struct l2x0_regs, pwr_ctrl));
  BLANK();
#endif
#ifdef CONFIG_CPU_HAS_ASID
  DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, context.id));
  BLANK();
#endif
  DEFINE(VMA_VM_MM,		offsetof(struct vm_area_struct, vm_mm));
  DEFINE(VMA_VM_FLAGS,		offsetof(struct vm_area_struct, vm_flags));
  BLANK();
  DEFINE(VM_EXEC,	       	VM_EXEC);
  BLANK();
  DEFINE(PAGE_SZ,	       	PAGE_SIZE);
  BLANK();
  DEFINE(SYS_ERROR0,		0x9f0000);
  BLANK();
  DEFINE(SIZEOF_MACHINE_DESC,	sizeof(struct machine_desc));
  DEFINE(MACHINFO_TYPE,		offsetof(struct machine_desc, nr));
  DEFINE(MACHINFO_NAME,		offsetof(struct machine_desc, name));
  BLANK();
  DEFINE(PROC_INFO_SZ,		sizeof(struct proc_info_list));
  DEFINE(PROCINFO_INITFUNC,	offsetof(struct proc_info_list, __cpu_flush));
  DEFINE(PROCINFO_MM_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
  DEFINE(PROCINFO_IO_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_io_mmu_flags));
  BLANK();
#ifdef MULTI_DABORT
  DEFINE(PROCESSOR_DABT_FUNC,	offsetof(struct processor, _data_abort));
#endif
#ifdef MULTI_PABORT
  DEFINE(PROCESSOR_PABT_FUNC,	offsetof(struct processor, _prefetch_abort));
#endif
#if defined(MULTI_CPU) || defined(CONFIG_ARM_LGUEST_GUEST)
  DEFINE(CPU_SLEEP_SIZE,	offsetof(struct processor, suspend_size));
  DEFINE(CPU_DO_SUSPEND,	offsetof(struct processor, do_suspend));
  DEFINE(CPU_DO_RESUME,		offsetof(struct processor, do_resume));
#endif
#if defined(MULTI_CACHE) || defined(CONFIG_ARM_LGUEST_GUEST)
  DEFINE(CACHE_FLUSH_KERN_ALL,	offsetof(struct cpu_cache_fns, flush_kern_all));
#endif
  BLANK();
  DEFINE(DMA_BIDIRECTIONAL,	DMA_BIDIRECTIONAL);
  DEFINE(DMA_TO_DEVICE,		DMA_TO_DEVICE);
  DEFINE(DMA_FROM_DEVICE,	DMA_FROM_DEVICE);

#if defined(CONFIG_LGUEST) || defined(CONFIG_ARM_LGUEST_GUEST) || defined(CONFIG_LGUEST_MODULE)
  BLANK();
  DEFINE(LGUEST_PAGES_host_general_regs, offsetof(struct lguest_ro_state, gregs));
  DEFINE(LGUEST_PAGES_host_pgd0, offsetof(struct lguest_ro_state, host_pgd0));
  DEFINE(LGUEST_PAGES_host_pgd1, offsetof(struct lguest_ro_state, host_pgd1));
  DEFINE(LGUEST_PAGES_host_context_id, offsetof(struct lguest_ro_state, host_cont_id));
  DEFINE(LGUEST_PAGES_host_copro, offsetof(struct lguest_ro_state, host_copro));
  DEFINE(LGUEST_PAGES_host_ctrl, offsetof(struct lguest_ro_state, host_ctrl));
  DEFINE(LGUEST_PAGES_host_tls, offsetof(struct lguest_ro_state, host_tls));
  DEFINE(LGUEST_PAGES_host_domain, offsetof(struct lguest_ro_state, host_domain));
  DEFINE(LGUEST_PAGES_host_undstack, offsetof(struct lguest_ro_state, host_undstack));
  DEFINE(LGUEST_PAGES_host_abtstack, offsetof(struct lguest_ro_state, host_abtstack));
  DEFINE(LGUEST_PAGES_host_irqstack, offsetof(struct lguest_ro_state, host_irqstack));
  DEFINE(LGUEST_PAGES_host_usrstack, offsetof(struct lguest_ro_state, host_usrstack));
  DEFINE(LGUEST_PAGES_host_svcstack, offsetof(struct lguest_ro_state, host_svcstack));

  BLANK();
  DEFINE(LGUEST_PAGES_guest_general_regs, offsetof(struct lguest_pages, regs.gregs));
  DEFINE(LGUEST_PAGES_guest_pgd0, offsetof(struct lguest_pages, regs.guest_pgd0));
  DEFINE(LGUEST_PAGES_guest_pgd1, offsetof(struct lguest_pages, regs.guest_pgd1));
  DEFINE(LGUEST_PAGES_guest_sp_offset, offsetof(struct lguest_pages, regs.guest_sp_offset));
  DEFINE(LGUEST_PAGES_guest_context_id, offsetof(struct lguest_pages, regs.guest_cont_id));
  DEFINE(LGUEST_PAGES_guest_tls, offsetof(struct lguest_pages, regs.guest_tls));
  DEFINE(LGUEST_PAGES_guest_domain, offsetof(struct lguest_pages, regs.guest_domain));
  DEFINE(LGUEST_PAGES_guest_retcode, offsetof(struct lguest_pages, regs.guest_retcode));
  DEFINE(LGUEST_PAGES_guest_copro, offsetof(struct lguest_pages, regs.guest_copro));
  DEFINE(LGUEST_PAGES_guest_ctrl, offsetof(struct lguest_pages, regs.guest_ctrl));

  DEFINE(LGUEST_PAGES_guest_cpuid_id, offsetof(struct lguest_pages, regs.guest_cpuid_id));
  DEFINE(LGUEST_PAGES_guest_cpuid_cachetype, offsetof(struct lguest_pages, regs.guest_cpuid_cachetype));
  DEFINE(LGUEST_PAGES_guest_cpuid_tcm, offsetof(struct lguest_pages, regs.guest_cpuid_tcm));
  DEFINE(LGUEST_PAGES_guest_cpuid_tlbtype, offsetof(struct lguest_pages, regs.guest_cpuid_tlbtype));
  DEFINE(LGUEST_PAGES_guest_time, offsetof(struct lguest_pages, regs.guest_time));
  DEFINE(LGUEST_PAGES_guest_irq_disabled, offsetof(struct lguest_pages, regs.irq_disabled));
  DEFINE(LGUEST_PAGES_guest_gpgdir, offsetof(struct lguest_pages, regs.gpgdir));
  DEFINE(LGUEST_PAGES_guest_cpu_arch, offsetof(struct lguest_pages, regs.guest_cpu_arch));
  DEFINE(LGUEST_PAGES_guest_irqs_pending, offsetof(struct lguest_pages, regs.irqs_pending));
  DEFINE(LGUEST_PAGES_guest_blocked_interrupts, offsetof(struct lguest_pages, regs.blocked_interrupts));


  DEFINE(LGUEST_PAGES_guest_irq_sp, offsetof(struct lguest_regs, guest_estack.irq[0]));
  DEFINE(LGUEST_PAGES_guest_abt_sp, offsetof(struct lguest_regs, guest_estack.abt[0]));
  DEFINE(LGUEST_PAGES_guest_und_sp, offsetof(struct lguest_regs, guest_estack.und[0]));
  DEFINE(LGUEST_PAGES_guest_hcalls, offsetof(struct lguest_regs, hcalls));

  DEFINE(LGUEST_PAGES_guest_svc_sp, offsetof(struct lguest_pages, spare));
#endif

  return 0; 
}
