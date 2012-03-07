#ifndef _ARM_LGUEST_H
#define _ARM_LGUEST_H

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/init.h>
#include <linux/stringify.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/err.h>

#include <linux/lguest_launcher.h>

#include <asm/irq.h>
#include <asm/lguest_hcall.h>
#include <asm/lguest.h>


void free_pagetables(void);
int init_pagetables(struct page **switcher_page, unsigned int pages);

struct pgdir {
	unsigned long gpgdir;
	pgd_t *pgdir;
};



#define SPARE_SIZE (PAGE_SIZE - sizeof(struct lguest_regs) -	\
		2 * sizeof(unsigned long))


/* We have two pages shared with guests, per cpu.  */
struct lguest_pages {
	/* Before go to the HOST, we save guest registers here*/
	struct lguest_regs regs;

	/*
	 * This is the guest svc stack mapped rw in guest.
	 * it is at the end of first page of lguest_pages 
	 */
	unsigned char guest_svc_stack[SPARE_SIZE];
	unsigned long spare[2];

    /* This is the host state & guest descriptor page, ro in guest */
	struct lguest_ro_state state;
} __attribute__((aligned(PAGE_SIZE)));




struct lg_cpu {
	unsigned int id;
	struct lguest *lg;
	struct task_struct *tsk;
	struct mm_struct *mm; 	/* == tsk->mm, but that becomes NULL on exit */

	int changed;

	unsigned long pending_notify; /* pfn from LHCALL_NOTIFY */

	/* At end of a page shared mapped over lguest_pages in guest. */
	unsigned long regs_page;
	struct lguest_regs *regs;

	struct lguest_pages *last_pages;

	int cpu_pgd; /* Which pgd this cpu is currently using */

	/* If a hypercall was asked for, this points to the arguments. */
	struct hcall_args *hcall;
	u32 next_hcall;

	/* Virtual clock device */
	struct hrtimer hrt;

	/* Did the Guest tell us to halt? */
	int halted;

	/* Pending virtual interrupts */
	DECLARE_BITMAP(irqs_pending, LGUEST_IRQS);

	struct lg_cpu_arch arch;
};

struct lg_eventfd {
	unsigned long addr;
	struct eventfd_ctx *event;
};

struct lg_eventfd_map {
	unsigned int num;
	struct lg_eventfd map[];
};

/* The private info the thread maintains about the guest. */
struct lguest {
	struct lguest_data __user *lguest_data;
	struct lg_cpu cpus[NR_CPUS];
	unsigned int nr_cpus;

	u32 pfn_limit;

	/*
	 * This provides the offset to the base of guest-physical memory in the
	 * Launcher.
	 */
	void __user *mem_base;
	unsigned long kernel_address;
	unsigned long kstart_paddr;

	struct pgdir pgdirs[4];

	unsigned long noirq_start, noirq_end;

	unsigned int stack_pages;
	u32 tsc_khz;

	struct lg_eventfd_map *eventfds;

	unsigned long mem_size;

	/* Dead? */
	const char *dead;
};

extern struct mutex lguest_lock;

/* core.c: */
bool lguest_address_ok(const struct lguest *lg,
		       unsigned long addr, unsigned long len);
void __lgread(struct lg_cpu *, void *, unsigned long, unsigned);
void __lgwrite(struct lg_cpu *, unsigned long, const void *, unsigned);

/*H:035
 * Using memory-copy operations like that is usually inconvient, so we
 * have the following helper macros which read and write a specific type (often
 * an unsigned long).
 *
 * This reads into a variable of the given type then returns that.
 */
#define lgread(cpu, addr, type)						\
	({ type _v; __lgread((cpu), &_v, (addr), sizeof(_v)); _v; })

/* This checks that the variable is of the given type, then writes it out. */
#define lgwrite(cpu, addr, type, val)				\
	do {							\
		typecheck(type, val);				\
		__lgwrite((cpu), (addr), &(val), sizeof(val));	\
	} while(0)
/* (end of memory access helper routines) :*/

int run_guest(struct lg_cpu *cpu, unsigned long __user *user);


/* arm/interrupts.c: */
unsigned int interrupt_pending(struct lg_cpu *cpu, bool *more);
void set_interrupt(struct lg_cpu *cpu, unsigned int irq);
void send_interrupt_to_guest(struct lg_cpu *cpu);
void guest_set_clockevent(struct lg_cpu *cpu, unsigned long delta);
bool send_notify_to_eventfd(struct lg_cpu *cpu);
void init_clockdev(struct lg_cpu *cpu);
bool check_syscall_vector(struct lguest *lg);
int init_interrupts(void);
void free_interrupts(void);


/* arm/page_tables.c: */
int init_guest_pagetable(struct lguest *lg);
void free_guest_pagetable(struct lguest *lg);
void guest_switch_mm(struct lg_cpu *cpu, unsigned long pgtable,
                unsigned long context_id);
void guest_set_pgd(struct lg_cpu *cpu, unsigned long gpgdir, u32 i, unsigned long gpmd);
void guest_set_pmd(struct lguest *lg, unsigned long gpgdir, u32 i);
void guest_set_pte(struct lg_cpu *cpu, unsigned long gpgdir,
		   unsigned long vaddr, pte_t val);
void map_switcher_in_guest(struct lg_cpu *cpu, struct lguest_pages *pages);
bool guest_abort_handler(struct lg_cpu *cpu, unsigned long vaddr, unsigned long fsr);
unsigned long guest_pa(struct lg_cpu *cpu, unsigned long vaddr);
void page_table_guest_hcall_init(struct lg_cpu *cpu);
int init_vectors_pte(unsigned long addr, unsigned long gvector_addr);
void map_vectors_in_guest(struct lg_cpu *cpu, unsigned long gvector_addr);
void free_vectors_pte(void);
void release_guest_nondirect_mapped_memory(struct lguest *lg);

/*arm/init.c*/
int copy_guest_vectors(const void __user *vectors, const unsigned long len);
unsigned long get_switcher_addr(void);



/* arm/run_guest.c: */
void lguest_arch_host_init(void);
void lguest_arch_host_fini(void);
void lguest_arch_run_guest(struct lg_cpu *cpu);
void lguest_arch_handle_return(struct lg_cpu *cpu);
int lguest_arch_init_hypercalls(struct lg_cpu *cpu);
int lguest_arch_do_hcall(struct lg_cpu *cpu, struct hcall_args *args);
void lguest_arch_setup_regs(struct lg_cpu *cpu, unsigned long start);

/* <arch>/switcher.S: */
extern char start_switcher_text[], end_switcher_text[], switch_to_guest[];

/* lguest_user.c: */
int lguest_device_init(void);
void lguest_device_remove(void);

/* hypercalls.c: */
void do_hypercalls(struct lg_cpu *cpu);



/*L:035
 * Let's step aside for the moment, to study one important routine that's used
 * widely in the Host code.
 *
 * There are many cases where the Guest can do something invalid, like pass crap
 * to a hypercall.  Since only the Guest kernel can make hypercalls, it's quite
 * acceptable to simply terminate the Guest and give the Launcher a nicely
 * formatted reason.  It's also simpler for the Guest itself, which doesn't
 * need to check most hypercalls for "success"; if you're still running, it
 * succeeded.
 *
 * Once this is called, the Guest will never run again, so most Host code can
 * call this then continue as if nothing had happened.  This means many
 * functions don't have to explicitly return an error code, which keeps the
 * code simple.
 *
 * It also means that this can be called more than once: only the first one is
 * remembered.  The only trick is that we still need to kill the Guest even if
 * we can't allocate memory to store the reason.  Linux has a neat way of
 * packing error codes into invalid pointers, so we use that here.
 *
 * Like any macro which uses an "if", it is safely wrapped in a run-once "do {
 * } while(0)".
 */
#define kill_guest(cpu, fmt...)					\
do {								\
	if (!(cpu)->lg->dead) {					\
		(cpu)->lg->dead = kasprintf(GFP_ATOMIC, fmt);	\
		if (!(cpu)->lg->dead)				\
			(cpu)->lg->dead = ERR_PTR(-ENOMEM);	\
	}							\
} while(0)
/* (End of aside) :*/

#endif	/* __ASSEMBLY__ */
#endif	/* _ARM_LGUEST_H */
