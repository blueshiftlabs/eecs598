/*P:400
 * This contains run_guest() which actually calls into the Host<->Guest
 * Switcher and analyzes the return, such as determining if the Guest wants the
 * Host to do something.  This file also contains useful helper routines.
:*/
#include <linux/module.h>
#include <linux/stringify.h>
#include <linux/stddef.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/highmem.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/poll.h>
#include "../lg.h"

/*  
 *  This code is based on the linux/driver/lguest/core.c
 *  written by Rusty Russell.
 *  Modified by Mingli Wu for Lguest of ARM version.
 */


static struct page **switcher_page;
static unsigned long guest_vectors;
static struct vm_struct *switcher_vma;



/*
 * Release the page of the Guest's low level exception vectors
 */
static void free_vectors_page(void)
{
	free_page(guest_vectors);
}

/*
 * Allocate a page for low lever vectors of the Guest.
 */
static __init int alloc_vectors_page(void)
{
	if(guest_vectors)
		return 0;
	guest_vectors = get_zeroed_page(GFP_KERNEL);
	if (!guest_vectors) {
		return -ENOMEM;
	}
	return 0;
}


/*
 * Before the Guest runs, the Host should set up the low level exception vectors for 
 * the Guest. if macro CONFIG_ARM_LGUEST_GUEST is defined there will be a new secton 
 * ".lguest.guest.vectors.address" in Linux image (ulgImage), this section contains 
 * the addresses of the Guest's Low-level vectors in the image, the Lguest launcher 
 * will read the compiled-in low level exception vector code from the image according 
 * these addresses, so the Host can set up the vectors for the Guest.
 * when compiling Linux kernel, we should issue "make ulgImage", and we will get a image 
 * named ulgImage which is a u-boot header(64 bytes) + Image(uncompressed). 
 */
int copy_guest_vectors(const void __user *vectors, const unsigned long len)
{
	if (copy_from_user((void *)guest_vectors, vectors, 4096) != 0) {
		return -EFAULT;
	}
	return 0;
}


/*
 * This routine supplies the Guest with time. Everytime we 
 * go back to the Guest, we call this function.
 */
static void write_timestamp(struct lg_cpu *cpu)
{
	ktime_get_real_ts(&cpu->regs->guest_time);
}


/* This One Big lock protects all inter-guest data structures. */
DEFINE_MUTEX(lguest_lock);

/* Return the virtual address of the Swithcer*/
unsigned long get_switcher_addr(void)
{
	return (unsigned long)switcher_vma->addr;
}



/*H:010
 * We need to set up the Switcher at a virtual address which should be located between  
 * VMALLOC_START and VMALLOC_END. Remember the Switcher is assembler code  which actually 
 * changes the CPU to run the Guest, and then changes back to the Host when a SWI, Abort, 
 * interrupt or an Undefined Instruction happens.
 *
 * The Switcher code must be at the same virtual address in the Guest as the
 * Host since it will be running as the switchover occurs.
 *
 * Trying to map memory at a particular address is an unusual thing to do, so
 * it's not a simple one-liner.
 */
static __init int map_switcher(void)
{
	int i = 0, err;
	struct page **pagep;
	unsigned long start;
	unsigned long end;

	switcher_page = kmalloc(sizeof(switcher_page[0])*TOTAL_SWITCHER_PAGES,
                                GFP_KERNEL);
	if (!switcher_page) {
		err = -ENOMEM;
		goto out;
	}
	for (i = 0; i < TOTAL_SWITCHER_PAGES; i++) {
		switcher_page[i] = alloc_page(GFP_KERNEL|__GFP_ZERO);
		if (!switcher_page[i]) {
			err = -ENOMEM;
			goto free_some_pages;
		}
	}

	/* 
	 * The first page is switch code, and the switch context start at the second page.
	 */
	pagep = switcher_page;
	/*
	 * Now we search the "virtual memory area" for the Switcher. we start at 
	 * (VMALLOC_END - 2M). If failed, then we search next 2M towards VMALLOC_START.
	 * The Switcher address should be 2M aligned because we hope that Switcher occupies 
	 * one pgd entry(2 PMD entries). The end address needs +1 because __get_vm_area 
	 * allocates an extra guard page, so we need space for that.
	 */
	start = VMALLOC_END - SWITCHER_TOTAL_SIZE;
	end = start + (TOTAL_SWITCHER_PAGES + 1) * PAGE_SIZE;
	for(; start > VMALLOC_START; start -= SWITCHER_TOTAL_SIZE, end -= SWITCHER_TOTAL_SIZE){
		switcher_vma = __get_vm_area(TOTAL_SWITCHER_PAGES * PAGE_SIZE,
                     VM_ALLOC, start, end);
		if(switcher_vma)
			break;
	}
	if (!switcher_vma) {
		err = -ENOMEM;
		printk("lguest: could not get switcher virtual memory area.\n");
		goto free_pages;
	}

	err = map_vm_area(switcher_vma, PAGE_KERNEL_EXEC, &pagep);
	if (err) {
		printk("lguest: map_vm_area failed: %i\n", err);
		goto free_vma;
	}

	/*
	 * Now the Switcher is mapped at the right address, we can't fail!
	 * Copy in the compiled-in Switcher code (from switcher.S).
	 */
	memcpy(switcher_vma->addr, start_switcher_text,
		end_switcher_text - start_switcher_text);

	/* And we succeeded... */
	return 0;

free_vma:
	vunmap(switcher_vma->addr);
free_pages:
	i = TOTAL_SWITCHER_PAGES;
free_some_pages:
	for (--i; i >= 0; i--)
		__free_pages(switcher_page[i], 0);
	kfree(switcher_page);
out:
	return err;
}
/*:*/

/* Cleaning up the mapping when the module is unloaded is almost... too easy. */
static void unmap_switcher(void)
{
	unsigned int i;

	vunmap(switcher_vma->addr);
	/* Now we just need to free the pages we copied the Switcher into */
	for (i = 0; i < TOTAL_SWITCHER_PAGES; i++)
		__free_pages(switcher_page[i], 0);
	kfree(switcher_page);
}

/*:*/



/*H:032
 * Dealing With Guest Memory.
 *
 * Before we go too much further into the Host, we need to grok the routines
 * we use to deal with Guest memory.
 *
 * When the Guest gives us (what it thinks is) a physical address, we can use
 * the normal copy_from_user() & copy_to_user() on the corresponding place in
 * the memory region allocated by the Launcher.
 *
 * But we can't trust the Guest: it might be trying to access the Launcher
 * code.  We have to check that the range is below the pfn_limit the Launcher
 * gave us.  We have to make sure that addr + len doesn't give us a false
 * positive by overflowing, too.
 */
bool lguest_address_ok(const struct lguest *lg,
		       unsigned long addr, unsigned long len)
{
	return (addr+len) / PAGE_SIZE < lg->pfn_limit && (addr+len >= addr);
}

/*
 * This routine copies memory from the Guest.  Here we can see how useful the
 * kill_lguest() routine we met in the Launcher can be: we return a random
 * value (all zeroes) instead of needing to return an error.
 */
void __lgread(struct lg_cpu *cpu, void *b, unsigned long addr, unsigned bytes)
{
	unsigned long address = (unsigned long)cpu->lg->mem_base + addr - PHYS_OFFSET; 
	if (!lguest_address_ok(cpu->lg, address, bytes)
	    || copy_from_user(b, (void *)address, bytes) != 0) {
		/* copy_from_user should do this, but as we rely on it... */
		memset(b, 0, bytes);
		kill_guest(cpu, "bad read address %#lx len %u", address, bytes);
	}
}

/* This is the write (copy into Guest) version. */
void __lgwrite(struct lg_cpu *cpu, unsigned long addr, const void *b,
	       unsigned bytes)
{
	unsigned long address = (unsigned long)cpu->lg->mem_base + addr - PHYS_OFFSET;
	if (!lguest_address_ok(cpu->lg, address, bytes)
	    || copy_to_user((void *)address, b, bytes) != 0)
		kill_guest(cpu, "bad write address %#lx len %u", address, bytes);
}



/*:*/

/*H:030
 * Let's jump straight to the the main loop which runs the Guest.
 * Remember, this is called by the Launcher reading /dev/lguest, and we keep
 * going around and around until something interesting happens.
 */
int run_guest(struct lg_cpu *cpu, unsigned long __user *user)
{
	/* We stop running once the Guest is dead. */
	while (!cpu->lg->dead) {
		/*
		 * It's possible the Guest did a NOTIFY hypercall to the
		 * Launcher.
		 */
		if (cpu->pending_notify) {
			/*
			 * Does it just needs to write to a registered
			 * eventfd (ie. the appropriate virtqueue thread)?
			 */
			if (!send_notify_to_eventfd(cpu)) {
				/* OK, we tell the main Laucher. */
				if (put_user(cpu->pending_notify, user))
					return -EFAULT;
				return sizeof(cpu->pending_notify);
			}
		}


		/* Check for signals */
		if (signal_pending(current))
			return -ERESTARTSYS;

		/* 
		 * We already call this function in lguest_arch_run_guest after 
		 * we come back from the Guest, but if the Guest sends a 
		 * NOTIFY hcall, We stop handling hcall, and notify the Launcher.
		 * We handle the rest hypercalls now before we go to the Guest.
		 */
		do_hypercalls(cpu);

	
		/*
		 * All long-lived kernel loops need to check with this horrible
		 * thing called the freezer.  If the Host is trying to suspend,
		 * it stops us.
		 */
		try_to_freeze();

		/*
		 * Just make absolutely sure the Guest is still alive.  One of
		 * those hypercalls could have been fatal, for example.
		 */
		if (cpu->lg->dead)
			break;

		/* Tell the Guest that there are some virtual irqs.*/
		send_interrupt_to_guest(cpu);	

        /*
		 * If the Guest asked to be stopped, we sleep.  The Guest's
		 * clock timer will wake us.
		 */
		if (cpu->halted){
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}

		/*Everytime before we go return the guest, we set time for the guest */
		write_timestamp(cpu);
		/*
		 * OK, now we're ready to jump into the Guest.  First we put up
		 * the "Do Not Disturb" sign:
		 */
		local_irq_disable();

		/* Actually run the Guest until something happens. */
		lguest_arch_run_guest(cpu);

		/* Now we're ready to be interrupted or moved to other CPUs */
		local_irq_enable();


		/* Now we deal with whatever happened to the Guest. */
		lguest_arch_handle_return(cpu);
	}

	/* Special case: Guest is 'dead' but wants a reboot. */
	if (cpu->lg->dead == ERR_PTR(-ERESTART))
		return -ERESTART;

	/* The Guest is dead => "No such file or directory" */
	return -ENOENT;
}

/*H:000
 * Welcome to the Host!
 *
 * By this point your brain has been tickled by the Guest code and numbed by
 * the Launcher code; prepare for it to be stretched by the Host code.  This is
 * the heart.  Let's begin at the initialization routine for the Host's lg
 * module.
 */
static int __init init(void)
{
	int err;


	/* Allocat a page for the Guest' low level exception vectors*/
	err = alloc_vectors_page();	
	if (err)
		goto out;
	
	/* 
	 * We set the PTE page of the Guest's low level exception vectors
	 */	
	err = init_vectors_pte(guest_vectors, GUEST_VECTOR_ADDRESS);
	if (err)
		goto free_vectors;		

	/* 
	 * We set the PTE page of the Switcher. 
	 */
	err = map_switcher();
	if (err)
		goto free_vectors_pte_page;

	/* Now we set up the pagetable implementation for the Guests. */
	err = init_pagetables(switcher_page, SHARED_SWITCHER_PAGES);
	if (err)
		goto unmap;
		
	/* 
	 * Now We need to do nothing in this function. 
	 */
	err = init_interrupts();
	if (err)
		goto free_pgtables;

	/* /dev/lguest needs to be registered. */
	err = lguest_device_init();
	if (err)
		goto free_interrupts;

	/* Finally we do some architecture-specific setup. */
	lguest_arch_host_init(); 

	/* All good! */
	return 0;
	

free_interrupts:
	free_interrupts();
free_pgtables:
	free_pagetables();
unmap:
	unmap_switcher();
free_vectors_pte_page:
	free_vectors_pte();	
free_vectors:
	free_vectors_page();	
out:
	return err;
}

/* Cleaning up is just the same code, backwards.*/  
static void __exit fini(void)
{
	lguest_device_remove();
	free_interrupts();
	free_pagetables();
	unmap_switcher();
	free_vectors_pte();
	free_vectors_page();
	printk("ARM Lguest is leaving\n");
}
/*:*/

/*
 * The Host side of lguest can be a module.  This is a nice way for people to
 * play with it.
 */
module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mingli Wu");
