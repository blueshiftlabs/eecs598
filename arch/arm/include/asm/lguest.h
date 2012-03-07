#ifndef _ASM_ARM_LGUEST_H
#define _ASM_ARM_LGUEST_H

#define RET_GSYSCALL	0
#define RET_UNF			0x04
#define RET_HCALL		0x08
#define RET_PABT		0x0c
#define RET_DABT		0x10
#define RET_IRQ		    0x18

#define GUEST_KERNEL_START (CONFIG_VECTORS_BASE + 0x20)

/*
 * It seems to be safe to select 0xfff1ffff as the Guest's 
 * virtual processor ID at present.
 * please also see linux/arch/arm/mach-armlguest/kernel/proc-lguest.S and
 * linux/arch/arm/mach-armlguest/driver/arm/run_guest.c.
 */
#define GUEST_PROCESSOR_ID 0xfff1ffff


#define SWITCHER_GSYSCALL_OFFSET	0
#define SWITCHER_UNDEF_OFFSET		0x04
#define SWITCHER_HCALL_OFFSET		0x08
#define SWITCHER_PABT_OFFSET		0x0c	
#define SWITCHER_DABT_OFFSET		0x10
#define SWITCHER_ADDREXCPTN_OFFSET	0x14
#define SWITCHER_IRQ_OFFSET			0x18
#define SWITCHER_FIQ_OFFSET			0x1c




#define SWITCHER_TOTAL_SIZE (1 << 21)




#ifndef __ASSEMBLY__

#define GUEST_VECTOR_ADDRESS CONFIG_VECTORS_BASE

/* Every guest maps the core switcher code. */
#define SHARED_SWITCHER_PAGES \
	DIV_ROUND_UP(end_switcher_text - start_switcher_text, PAGE_SIZE)


/* Pages for switcher itself, then two pages per cpu */
#define TOTAL_SWITCHER_PAGES (SHARED_SWITCHER_PAGES + 2 * nr_cpu_ids)


extern char lguest_noirq_start[], lguest_noirq_end[];
extern void lguest_init(void);

struct exception_stack {
	u32 irq[3];
	u32 abt[3];
	u32 und[3];
} __attribute__((__aligned__(L1_CACHE_BYTES)));



 

struct lguest_regs {
	struct pt_regs gregs;
	unsigned long guest_pgd0;
	unsigned long guest_cont_id;
	unsigned long guest_tls;
	unsigned long guest_domain;
	unsigned long guest_ctrl;
	unsigned long guest_copro;
	unsigned long guest_pgd1;
	unsigned long guest_sp_offset;
	unsigned long guest_retcode;
	unsigned long guest_cpuid_id;
	unsigned long guest_cpuid_cachetype;
	unsigned long guest_cpuid_tcm;
	unsigned long guest_cpuid_tlbtype;
	struct timespec guest_time;
	int guest_cpu_arch;
	unsigned long gpgdir;
	unsigned long irq_pending;
	unsigned long irq_disabled;
	DECLARE_BITMAP(irqs_pending, LGUEST_IRQS);
	DECLARE_BITMAP(blocked_interrupts, LGUEST_IRQS);

	/*undef, abt and irq stacks of guest*/
	struct exception_stack guest_estack;
	struct hcall_args hcalls[LHCALL_RING_SIZE];
} __attribute__((__aligned__(L1_CACHE_BYTES)));


/* This is a guest-specific page (mapped ro) into the guest. */
struct lguest_ro_state {
	struct pt_regs gregs;
	unsigned long host_pgd0;
	unsigned long host_cont_id;
	unsigned long host_tls;
	unsigned long host_domain;
	unsigned long host_ctrl;
	unsigned long host_copro;
	unsigned long host_pgd1;
	unsigned long host_undstack;
	unsigned long host_abtstack;
	unsigned long host_irqstack;
	unsigned long host_svcstack;
	unsigned long host_usrstack;
};

struct lg_cpu_arch {
	/* The address of the last guest-visible pagefault (ie. cr2). */
	unsigned long last_pagefault;
};


/*Following code comes from arch/arm/mm/fault.c */
#define FSR_LNX_PF      (1 << 31)
#define FSR_WRITE       (1 << 11)
#define FSR_FS4         (1 << 10)
#define FSR_FS3_0       (15)


#endif /* __ASSEMBLY__ */

#endif /* _ASM_ARM_LGUEST_H */
