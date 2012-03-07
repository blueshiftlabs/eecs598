/*P:500
 * Just as userspace programs request kernel operations through a system
 * call, the Guest requests Host operations through a "hypercall".  
 * On ARM, Lguest does realize hypercalls through system calls(SWI instruction)
 * As you'd expect, this code is basically a one big switch statement.
:*/

/*  Copyright (C) 2006 Rusty Russell IBM Corporation
	
	Modified by Mingli Wu for ARM Version

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/
#include <linux/uaccess.h>
#include <linux/syscalls.h>


#include <asm/page.h>
#include <asm/pgtable.h>
#include "../lg.h"

/*
 * Note: I only test the Lguest of ARM version on Omap3530. When the Guest
 * require do something about Cache, the Host need to do nothing, and
 * and it works well by on my Development Board(Omap3530). Other ARM chips 
 * may need to do something for the Guest when the Guest requires something 
 * on Cache. As for TLBs, I and D TLBs are flushed in the Switcher. 
 */


#define GUEST_DOMAIN     (domain_val(DOMAIN_USER, DOMAIN_CLIENT) | \
                        domain_val(DOMAIN_KERNEL, DOMAIN_CLIENT) | \
                        domain_val(DOMAIN_TABLE, DOMAIN_CLIENT)  | \
                        domain_val(DOMAIN_IO, DOMAIN_CLIENT))



/*H:120
 * This is the core hypercall routine: where the Guest gets what it wants.
 * Or gets killed.  Or, in the case of LHCALL_SHUTDOWN, both.
 */
static void do_hcall(struct lg_cpu *cpu, struct hcall_args *args)
{
	switch (args->arg0) {
		case LHCALL_NOTIFY:
			cpu->pending_notify = args->arg1;
			break;

		case LHCALL_SHUTDOWN: {
			char msg[128];
			/*
			 * Shutdown is such a trivial hypercall that we do it in five
			 * lines right here.
			 *
			 * If the lgread fails, it will call kill_guest() itself; the
			 * kill_guest() with the message will be ignored.
			 */
			__lgread(cpu, msg, args->arg1, sizeof(msg));
			msg[sizeof(msg)-1] = '\0';

			kill_guest(cpu, "CRASH: %s", msg);
			if (args->arg2 == LGUEST_SHUTDOWN_RESTART)
				cpu->lg->dead = ERR_PTR(-ERESTART);
			break;
		}
		case LHCALL_RESET:
			/*
			 * The RESET Button is pressed when the Guest is running.
			 * How should we do???
			 */
			kill_guest(cpu, "The RESET Button was pressed!");
			cpu->lg->dead = ERR_PTR(-ERESTART);
			break;

		case LHCALL_HALT:
			kill_guest(cpu, "A Fiq or a Address Exception happens ");
			break;

		case LHCALL_IDLE:
			/* Similarly, this sets the halted flag for run_guest(). */
			cpu->halted = 1;
			break;

		case LHCALL_GUEST_BUSY_WAIT:
			/* The Guest needs a busy wait. */
			break;

		case LHCALL_SEND_INTERRUPTS:
			/*
			 * The Guest will send this hypercall When it knows that there are some 
			 * pending virtual irqs.
			 */
			break;

		case LHCALL_SET_DOMAIN:
			/* All domains of Guest should be Client.  */
			cpu->regs->guest_domain = GUEST_DOMAIN;
			break;
	
		case LHCALL_SET_PTE:
			/*
			 * The Guest sets a PTE entry, the Host sets the relative one of 
			 * shadow page table. 
			 */ 
			guest_set_pte(cpu, args->arg1, args->arg2, __pte(args->arg3));
			break;

		case LHCALL_SET_PTE_EXT:
			/*
			 * The Guest sets a pte entry, but We do not know top level.
			 */
			release_guest_nondirect_mapped_memory(cpu->lg);
			break;

		case LHCALL_SET_PGD:
			/*
			 * The Guest sets PGD entry
			 */
			guest_set_pgd(cpu, args->arg1, args->arg2, args->arg3);
			break;


		case LHCALL_COPY_PMD:
			/*
			 * we do nothing here, and let guest_abort_handler to do it
			 * when a Data Abort or Prefetch Abort happens.
			 */
			break;

		case LHCALL_PMD_CLEAR:
			guest_set_pgd(cpu, args->arg1, args->arg2, 0);
			break;

		case LHCALL_FLUSH_PMD:
			/* 
			 * On my Development Board(omap3530), I need to do nothing here.
			 */
			break;

		case LHCALL_CLEAN_PMD:
			break;

		case LHCALL_SET_CLOCKEVENT:
			guest_set_clockevent(cpu, args->arg1);
			break;

		case LHCALL_SWITCH_MM:
			/* The Guest requires to change the Translation Table Base Pointer */
			guest_switch_mm(cpu, args->arg1, args->arg2);
			break;	

		case LHCALL_SWITCH_TO:
			/* 
			 * The Guest requires to change TLS and domain registers.
			 * The switcher will do it for the Guest before returning
			 * to the Guest. please see switcher.S and switcher-head.S 
			 */		
			cpu->regs->guest_tls = args->arg1;
			/* All domains of Guest should be Client.  */
			cpu->regs->guest_domain = GUEST_DOMAIN;
			break;
		
		case LHCALL_SET_TLS:
		    /* 
			 * The Guest needs to set TLS Register. 
			 * The switcher will do it for the Guest before returning
			 * to the Guest. please see switcher.S and switcher-head.S 
			 */
			cpu->regs->guest_tls = args->arg1;
			break;

		case LHCALL_SET_CR:
			/* 
			 * The Guest needs to set Control Register 
			 * The switcher will do it for the Guest before returning
			 * to the Guest. please see switcher.S and switcher-head.S 
			 */
			cpu->regs->guest_ctrl = args->arg1;
			break;

		case LHCALL_SET_COPRO:
			/* 
			 * The Guest needs to set Coprocessor Access Control Register 
			 * The switcher will do it for the Guest before returning
			 * to the Guest. please see switcher.S and switcher-head.S 
			 */
			cpu->regs->guest_copro = args->arg1;
			break;

//======Operations on TLB and Cache for the Guest===============================

/*
 * Note: I only test the Lguest of ARM version on Omap3530. When the Guest
 * require do something about Cache, the Host need to do nothing, and
 * and it works well by on my Development Board(Omap3530). Other ARM chips may 
 * need to do something for the Guest when the Guest requires something on 
 * Cache. As for TLBs, I and D TLBs have already been flushed in the Switcher.  
 */

		case LHCALL_FLUSH_KERNEL_TLB:
			break;

		case LHCALL_FLUSH_USER_TLB:
			break;

		case LHCALL_LOCAL_FLUSH_TLB_ALL:
			break;

		case LHCALL_LOCAL_FLUSH_TLB_MM:
			break;

		case LHCALL_LOCAL_FLUSH_TLB_PAGE:
			break;

		case LHCALL_LOCAL_FLUSH_TLB_KERNEL_PAGE:
			break;



		case LHCALL_FLUSH_CACHE_USER:
			break;

		case LHCALL_FLUSH_CACHE_KERNEL:
			break;

		case LHCALL_FLUSH_CACHE_USER_RANGE:
			break;

		case LHCALL_COHERENT_CACHE_KERNEL_RANGE:
			break;

		case LHCALL_COHERENT_CACHE_USER_RANGE:
			break;

		case LHCALL_FLUSH_DCACHE_KERNEL_AREA:
			break;

		case LHCALL_DMA_INV_CACHE_RANGE:
			break;

		case LHCALL_DMA_CLEAN_CACHE_RANGE:
			break;

		case LHCALL_DMA_FLUSH_CACHE_RANGE:
			break;

		case LHCALL_DCACHE_CLEAN_AREA:
			break;

		case LHCALL_FLUSH_ICACHE_ALL:
			break;

		case LHCALL_CLERA_USER_HIGHPAGE_NONALIASING:
			break;

		case LHCALL_COPY_USER_HIGHPAGE_NONALIASING:
			break;
//======Operations on TLB and Cache for the Guest===============================

	

		default:
			/* It should be an architecture-specific hypercall. */
			if (lguest_arch_do_hcall(cpu, args)) 
				kill_guest(cpu, "Bad hypercall %li\n", args->arg0);
	}
}

/*H:124
 * Asynchronous hypercalls are easy: we just look in the array in the
 * Guest's "struct lguest_regs" to see if any new ones are marked "ready".
 *
 * We are careful to do these in order: obviously we respect the order the
 * Guest put them in the ring. 
 */
void do_hypercalls(struct lg_cpu *cpu)
{
#define NEXT_HCALL(current) \
	((current + 1) & (LHCALL_RING_SIZE - 1))

	unsigned int i;
	struct hcall_args *hcalls = ((struct lguest_regs *)(cpu->regs_page))->hcalls;

	/* We process "struct lguest_"s hcalls[] ring once. */
	for (i = 0; i < LHCALL_RING_SIZE; i++) {
		/*
		 * We remember where we were up to from last time.  This makes
		 * sure that the hypercalls are done in the order the Guest
		 * places them in the ring.
		 */
		unsigned int n = cpu->next_hcall;

		/* -1UL means there's no call here (yet). */
		if(hcalls[n].arg0 == -1UL)
			break;
		/*
		 * OK, we have hypercall.  Increment the "next_hcall" cursor,
		 * and wrap back to 0 if we reach the end.
		 */
		
		cpu->next_hcall = NEXT_HCALL(cpu->next_hcall);

		/* Do the hypercall, same as a normal one. */
		do_hcall(cpu, &hcalls[n]);

		/* Mark the hypercall done. */
		hcalls[n].arg0 = -1UL;

		/*
		 * Stop doing hypercalls if they want to notify the Launcher:
		 * it needs to service this first.
		 */
		if (cpu->pending_notify)
			break;
	}
}

