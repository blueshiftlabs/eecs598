#ifndef _ARM_ASM_LGUEST_HOOKS_H
#define _ARM_ASM_LGUEST_HOOKS_H

#include <asm/lguest-native.h>

#include <asm/cputype.h>
#include <asm/domain.h>
#include <asm/irqflags.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/proc-fns.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/system.h>
#include <asm/tlbflush.h>

#define EXTERN_HOOK(ret, fn, sig...) \
extern ret LGUEST_NATIVE_NAME(fn) (sig);

extern void (*do_ret_to_user)(void);
extern void (*do_ret_from_fork)(void);

EXTERN_HOOK(void, syscall_set_tls, unsigned long);
EXTERN_HOOK(void, show_coproc_regs, char *);
EXTERN_HOOK(unsigned long, kernel_thread_cpsr, void);
EXTERN_HOOK(int, __get_cpu_architecture, void);
EXTERN_HOOK(void, cpu_tcm_init, void);
EXTERN_HOOK(void __init, early_trap_init, void);

#undef EXTERN_HOOK
#endif
