/*
 *  linux/arch/arm/mm/proc-syms.c
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/mm.h>

#include <asm/cacheflush.h>
#include <asm/proc-fns.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#if (!defined MULTI_CPU) && (!defined CONFIG_ARM_LGUEST_GUEST)
EXPORT_SYMBOL(cpu_dcache_clean_area);
EXPORT_SYMBOL(cpu_set_pte_ext);
#else
EXPORT_SYMBOL(processor);
#endif

#if (!defined MULTI_CACHE) && (!defined CONFIG_ARM_LGUEST_GUEST)
EXPORT_SYMBOL(__cpuc_flush_kern_all);
EXPORT_SYMBOL(__cpuc_flush_user_all);
EXPORT_SYMBOL(__cpuc_flush_user_range);
EXPORT_SYMBOL(__cpuc_coherent_kern_range);
EXPORT_SYMBOL(__cpuc_flush_dcache_area);
#else
EXPORT_SYMBOL(cpu_cache);
#endif

#ifdef CONFIG_MMU
#if (!defined MULTI_USER) && (!defined CONFIG_ARM_LGUEST_GUEST)
EXPORT_SYMBOL(__cpu_clear_user_highpage);
EXPORT_SYMBOL(__cpu_copy_user_highpage);
#else
EXPORT_SYMBOL(cpu_user);
#endif
#endif

/*
 * No module should need to touch the TLB (and currently
 * no modules do.  We export this for "loadkernel" support
 * (booting a new kernel from within a running kernel.)
 */
#if (defined MULTI_TLB) || (defined CONFIG_ARM_LGUEST_GUEST)
EXPORT_SYMBOL(cpu_tlb);
#endif
