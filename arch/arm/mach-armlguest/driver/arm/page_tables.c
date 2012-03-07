/*P:700
 * The pagetable code, on the other hand, still shows the scars of
 * previous encounters.  It's functional, and as neat as it can be in the
 * circumstances, but be wary, for these things are subtle and break easily.
 * The Guest provides a virtual to physical mapping, but we can neither trust
 * it nor use it: we verify and convert it here then point the CPU to the
 * converted Guest pages when running the Guest.
:*/

/*
 *  linux/arch/arm/mach-armlguest/driver/arm/page_tables.c
 *
 *  Copyright (C) 2009 Mingli Wu. (myfavor_linux@msn.com)
 *
 *  This code is based on the linux/driver/lguest/page_tables.c,
 *  written by Rusty Russell.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/mm.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/percpu.h>
#include <asm/tlbflush.h>
#include <asm/uaccess.h>

#include <asm/domain.h>

#include <asm/setup.h>
#include <asm/mach/map.h>
#include "../lg.h"
#include "./page_tables_arm.h"


#define SWITCHER_PGD_INDEX  pgd_index(get_switcher_addr())

/*M:008
 * We hold reference to pages, which prevents them from being swapped.
 * It'd be nice to have a callback in the "struct mm_struct" when Linux wants
 * to swap out.  If we had this, and a shrinker callback to trim PTE pages, we
 * could probably consider launching Guests as non-root.
:*/

/*H:300
 * The Page Table Code
 *
 * We use two-level page tables for the Guest on Lguest of ARM version.  
 * Please read linux/arch/arm/include/asm/pgtable.h for pagetable management
 *
 * The Guest keeps page tables, but we maintain the actual ones here: these are
 * called "shadow" page tables.  Which is a very Guest-centric name: these are
 * the real page tables the CPU uses, although we keep them up to date to
 * reflect the Guest's.  (See what I mean about weird naming?  Since when do
 * shadows reflect anything?)
 *
 * Anyway, this is the most complicated part of the Host code.  There are seven
 * parts to this:
 *  (i) Looking up a page table entry when the Guest faults,
 *	(ii) Mapping the Guest's exception vectors when the Guest is about to run,
 *  (iii) Setting up a page table entry when the Guest tells us one has changed,
 *  (iv) Switching page tables,
 *  (v) Flushing (throwing away) page tables,
 *  (vi) Mapping the Switcher when the Guest is about to run,
 *  (vii) Setting up the page tables initially.
 */



/*
 * We actually need a separate PTE page for each CPU.  Remember that after the
 * Switcher code itself comes two pages for each CPU, and we don't want this
 * CPU's guest to see the pages of any other CPU.
 */
static DEFINE_PER_CPU(pte_t *, switcher_pte_pages);
#define switcher_pte_page(cpu) per_cpu(switcher_pte_pages, cpu)

static pte_t *vectors_pte_page;

/*H:320
 * spgd_addr() takes the virtual address and returns a pointer to the top-level
 * page directory entry (PGD) for that address.  Since we keep track of several
 * page tables, the "i" argument tells us which one we're interested in (it's
 * usually the current one).
 */
static pgd_t *spgd_addr(struct lg_cpu *cpu, u32 i, unsigned long vaddr)
{
	unsigned int index = pgd_index(vaddr);

	/* Return a pointer index'th pgd entry for the i'th page table. */
	return &cpu->lg->pgdirs[i].pgdir[index];
}


/*
 * This routine then takes the page directory entry returned above, which
 * contains the address of the page table entry (PTE) page.  It then returns a
 * pointer to the PTE entry for the given address.
 */
static pte_t *spte_addr(struct lg_cpu *cpu, pgd_t *spgd, unsigned long vaddr)
{
	pmd_t *pmd = pmd_offset(spgd, vaddr);
	pte_t *page;
	unsigned long ptr;

	ptr = pmd_val(*pmd) & ~(PTRS_PER_PTE * sizeof(void *) - 1);
	ptr += PTRS_PER_PTE * sizeof(void *);

	page =__va(ptr) + (__pte_index(vaddr) * sizeof(void *));

	return page;
}

/*
 * These functions are just like the above two, except they access the Guest
 * page tables.  Hence they return a Guest address.
 */
static unsigned long gpgd_addr(struct lg_cpu *cpu, unsigned long vaddr)
{
	unsigned int index = pgd_index(vaddr);
	return cpu->lg->pgdirs[cpu->cpu_pgd].gpgdir + index * sizeof(pgd_t);
}


/* Follow the PGD to the PTE  */
static unsigned long gpte_addr(struct lg_cpu *cpu,
				pgd_t *gpgd, unsigned long vaddr)
{
	pmd_t *pmd = pmd_offset(gpgd, vaddr);
	unsigned long gpage = pmd_val(*pmd) & ~(PTRS_PER_PTE * sizeof(void *) - 1);
        
	gpage += PTRS_PER_PTE * sizeof(void *);
	gpage += __pte_index(vaddr)* sizeof(pte_t);
	return gpage;
}




/* Setup a PGD(two PMDs) entry of the Guest Shadow pagetable*/
static inline void lguest_pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
	pmdp[0] = __pmd(pmdval);
	pmdp[1] = __pmd(pmdval + 256 * sizeof(pte_t));
}


/* Clear a PGD(two PMDs) entry of the Guest Shadown pagetable*/
static inline void lguest_pmd_clear(pmd_t *pmdp)
{
	pmdp[0] = __pmd(0);
	pmdp[1] = __pmd(0);
}




/* Get the value of a PMD entry */
static unsigned long get_pmdval(pgd_t *pgd, unsigned long vaddr)
{
	pmd_t *pmd = pmd_offset(pgd, vaddr);

	if (vaddr & SECTION_SIZE)
		pmd++;

	return pmd_val(*pmd);
}



/* Check if a PMD entry is valid.*/
static unsigned long check_pmd(unsigned long value)
{
	if(((value & PMD_TYPE_MASK) == PMD_TYPE_FAULT) ||
		((value & PMD_TYPE_MASK) == PMD_TYPE_MASK))
		return 0;
	return 1;
}



/*:*/

/*M:014
 * get_pfn is slow: we could probably try to grab batches of pages here as
 * an optimization (ie. pre-faulting).
:*/

/*H:350
 * This routine takes a page number given by the Guest and converts it to
 * an actual, physical page number.  It can fail for several reasons: the
 * virtual address might not be mapped by the Launcher, the write flag is set
 * and the page is read-only, or the write flag was set and the page was
 * shared so had to be copied, but we ran out of memory.
 *
 * This holds a reference to the page, so release_pte() is careful to put that
 * back.
 */
static unsigned long get_pfn(unsigned long virtpfn, int write)
{
	struct page *page;

	/* gup me one page at this address please! */
	if (get_user_pages_fast(virtpfn << PAGE_SHIFT, 1, write, &page) == 1)
		return page_to_pfn(page);

	/* This value indicates failure. */
	return -1UL;
}

/*H:340
 * Converting a Guest page table entry to a shadow (ie. real) page table
 * entry can be a little tricky.  The flags are (almost) the same, but the
 * Guest PTE contains a virtual page number: the CPU needs the real page
 * number.
 */
static pte_t gpte_to_spte(struct lg_cpu *cpu, pte_t gpte, int write)
{
	unsigned long pfn, base, flags;

	/*
	 * The Guest sets the global flag, because it thinks that it is using
	 * PGE.  We only told it to use PGE so it would tell us whether it was
	 * flushing a kernel mapping or a userspace mapping.  We don't actually
	 * use the global bit, so throw it away.
	 */
	flags = pte_val(gpte) & (PAGE_SIZE - 1);

	/* The Guest's pages are offset inside the Launcher. */
	base = (unsigned long)cpu->lg->mem_base / PAGE_SIZE;

	/*
	 * We need a temporary "unsigned long" variable to hold the answer from
	 * get_pfn(), because it returns 0xFFFFFFFF on failure, which wouldn't
	 * fit in spte.pfn.  get_pfn() finds the real physical number of the
	 * page, given the virtual number.
	 */
	pfn = get_pfn(base + pte_pfn(gpte)- PHYS_PFN_OFFSET, write);
	if (pfn == -1UL) {
		kill_guest(cpu, "failed to get page %lu", pte_pfn(gpte));
		/*
		 * When we destroy the Guest, we'll go through the shadow page
		 * tables and release_pte() them.  Make sure we don't think
		 * this one is valid!
		 */
		flags = 0;
	}
	/* Now we assemble our shadow PTE from the page number and flags. */
	return pfn_pte(pfn, __pgprot(flags));
}





/*H:460 And to complete the chain, release_pte() looks like this: */
static void release_pte(pte_t pte)
{
	/*
	 * Remember that get_user_pages_fast() took a reference to the page, in
	 * get_pfn()?  We have to put it back now.
	 */
	if(pte_present(pte))
		put_page(pte_page(pte));
}


/*:*/

static void check_gpte(struct lg_cpu *cpu, pte_t gpte)
{
	if ((pte_pfn(gpte) >= cpu->lg->pfn_limit) || 
				(pte_pfn(gpte) < PHYS_PFN_OFFSET))
		kill_guest(cpu, "bad page table entry");
}


/* */
static void check_gpgd(struct lg_cpu *cpu, pgd_t *gpgd, unsigned long vaddr)
{

	unsigned long ptr;
	unsigned long offset_page;

	ptr = get_pmdval(gpgd, vaddr);
	if(!check_pmd(ptr))
		goto baddirectory;
	if((ptr & PMD_TYPE_MASK) == PMD_TYPE_SECT){
		offset_page = (ptr & SECTION_MASK) >> PAGE_SHIFT;
	} else {
		offset_page = ptr >> PAGE_SHIFT;
	}
	if ((offset_page < cpu->lg->pfn_limit) &&
		(offset_page >= PHYS_PFN_OFFSET))
		return;
baddirectory:
	kill_guest(cpu, "bad page directory entry");
}


/* calculate the error code */
static inline int fsr_fs(unsigned int fsr)
{
	return (fsr & FSR_FS3_0) | (fsr & FSR_FS4) >> 6;
}



/*H:330
 * (i) Looking up a page table entry when the Guest aborts.
 *
 * We saw this call in run_guest(): when we see a data abort or prefech aboart 
 * in the Guest, we come here.  That's because we only set up the shadow page 
 * tables lazily as they're needed, so we get data aborts of prefech aborts 
 * all the time and quietly fix them up and return to the Guest without it 
 * knowing. If we fixed up the fault (ie. we mapped the address), this routine 
 * returns true.  Otherwise, it was a real fault and we need to tell the Guest.
 */
bool guest_abort_handler(struct lg_cpu *cpu, unsigned long vaddr, unsigned long fsr)
{
#define SECTION_TRANSLATION_FAULT	0x5
#define PAGE_TRANSLATION_FAULT		0x7	
#define PAGE_PERMISSION_FAULT		0xf
	pgd_t gpgd;
	pgd_t *spgd;
	unsigned long gpte_ptr;
	pte_t gpte;
	pte_t *spte;
	pmd_t *gpmd;
	pmd_t *spmd;
	unsigned long ext = 0;
	unsigned long errcode;
	unsigned long mem_size = cpu->lg->mem_size;

	
	/* calculate the error code*/
	errcode = fsr_fs(fsr);

	/*
	 * We only handle the translation faults. 
	 */
	if((errcode != PAGE_TRANSLATION_FAULT) && (errcode != SECTION_TRANSLATION_FAULT)){
		return false;
	}

	/* The Host do not handle NULL address.*/
	if(vaddr == 0){
		return false;
	}


	/* 
	 * Entries of Direct mapped memory have already be set up before the Guest runs,
	 * and they should never change before the Guest exits.
	 */
	if((vaddr >= PAGE_OFFSET) && (vaddr < mem_size + PAGE_OFFSET)){
		kill_guest(cpu, "direct mapped memory error");
	}
	
	/* 
	 * The PTEs of the Switcher and the Guest's exception vectors should always be there
	 * This should not be going to happen. 
	 */
	if(((vaddr >= GUEST_VECTOR_ADDRESS) && (vaddr < GUEST_VECTOR_ADDRESS + PAGE_SIZE)) || 
		(pgd_index(vaddr) == SWITCHER_PGD_INDEX)){
		kill_guest(cpu, "Cannot find the LGUEST SWITCHER or VECTORS");
	}


	/* First step: get the top-level Guest page table entry. */
	__lgread(cpu, &gpgd, gpgd_addr(cpu, vaddr), sizeof(pgd_t));
	gpmd = pmd_offset(&gpgd, vaddr);
	if(!check_pmd(pmd_val(*gpmd))){
		return false;
	}

	/* Now look at the matching shadow entry. */
	spgd = spgd_addr(cpu, cpu->cpu_pgd, vaddr);
	spmd = pmd_offset(spgd, vaddr);	
	if (!check_pmd(pmd_val(*spmd))) {
		/* No shadow entry: allocate a new shadow PTE page. */
		unsigned long ptepage;
		unsigned long pmd_flags;

		/* The PMD of exception vectors should be alwasy there */
		if(pgd_index(vaddr) == (PTRS_PER_PGD -1)){
			kill_guest(cpu, "Cannot find the VECTORS PMD");
		}

		/*
		 * This is not really the Guest's fault, but killing it is
		 * simple for this corner case.
		 */
		ptepage = get_zeroed_page(GFP_KERNEL);
		if (!ptepage) {
			kill_guest(cpu, "out of memory allocating pte page");
			return false;
		}
		/* We check that the Guest pgd is OK. */
		check_gpgd(cpu, &gpgd, vaddr);
		/*
		 * And we copy the flags to the shadow PGD entry.  The page
		 * number in the shadow PGD is the page we just allocated.
		 */
		pmd_flags = pmd_val(*gpmd) & (PTRS_PER_PTE * sizeof(void *) - 1);
		lguest_pmd_populate(spmd, __pa(ptepage) | pmd_flags);
	}

	/*
	 * OK, now we look at the lower level in the Guest page table: keep its
	 * address, because we might update it later.
	 */
	gpte_ptr = gpte_addr(cpu, &gpgd, vaddr);

	/* Read the actual PTE value. */
	gpte = lgread(cpu, gpte_ptr, pte_t);

	/* If the PTE is invalid, we let the Guest handle it*/
	if(!pte_present(gpte)){
		return false;
	}

	/* 
	 * If a write is attempting on a read-only page, we do
	 * nothing and let the Guest to handle it. 
	 */
	if((fsr & FSR_WRITE)&&(!(pte_write(gpte)))){
		return false;
	}

	if((fsr == PAGE_PERMISSION_FAULT) && (pte_val(gpte) & L_PTE_USER)){
		return false;
	}

    
	/*
	 * Check that the Guest PTE flags are OK, and the page number is below
	 * the pfn_limit (ie. not mapping the Launcher binary).
	 */
	check_gpte(cpu, gpte);

	gpte = pte_mkyoung(gpte);

	/* If this is a write, we make it dirty*/
	if(fsr & FSR_WRITE)
		gpte = pte_mkdirty(gpte);

	/* Get the pointer to the shadow PTE entry we're going to set. */
	spte = spte_addr(cpu, spgd, vaddr);

	/*
	 * If there was a valid shadow PTE entry here before, we release it.
	 * This can happen with a write to a previously read-only entry.
	 */
	release_pte(*spte);

	/* 
	 * The entry is for the Guest's user space. PTE_EXT_NG determines whether
	 * the translation should be mark as global(0) or process specific(1) in the
	 * TLB. Please see ARM Architecture Reference Manual B4-25.
	 */	
	if(vaddr < TASK_SIZE){
		ext = PTE_EXT_NG; 
	}

	/*
	 * If this is a write, we insist that the Guest page is writable (the
	 * final arg to gpte_to_spte()).
	 */
	if (pte_dirty(gpte))
		set_guest_pte(spte, gpte_to_spte(cpu, gpte, 1), ext);
	else {
		/*
		 * If this is a read, don't set the "writable" bit in the page
		 * table entry, even if the Guest says it's writable.  That way
		 * we will come back here when a write does actually occur, so
		 * we can update the Guest's L_PTE_DIRTY flag.
		 */
		set_guest_pte(spte, gpte_to_spte(cpu, pte_wrprotect(gpte), 0), ext);
	}

	/*
	 * Finally, we write the Guest PTE entry back: we've set the
	 * L_PTE_YOUNG and maybe the L_PTE_DIRTY flags.
	 */
	lgwrite(cpu, gpte_ptr, pte_t, gpte);

	/*
	 * The fault is fixed, the page table is populated, the mapping
	 * manipulated, the result returned and the code complete.  A small
	 * delay and a trace of alliteration are the only indications the Guest
	 * has that a page fault occurred at all.
	 */
	return true;
}


/*:*/


/*H:450
 * Release one PGD entry(2 PMDS).
 */
static void release_pgd(pgd_t *spgd)
{
	pmd_t *pmd = pmd_offset(spgd, 0);
	/* If the entry's not present, there's nothing to release. */
	if(check_pmd(pmd_val(*pmd))){

		unsigned int i;
		unsigned long ptr = pmd_val(*pmd) & ~(PAGE_SIZE - 1);
		unsigned long page = (unsigned long)__va(ptr);
		pte_t *ptepage;
		ptr += PTRS_PER_PTE * sizeof(void *);
		/*
		 * Converting the pfn to find the actual PTE page is easy: turn
		 * the page number into a physical address, then convert to a
		 * virtual address (easy for kernel pages like this one).
		 */
		ptepage = __va(ptr);
		/* For each entry in the page, we might need to release it. */
		for (i = 0; i < PTRS_PER_PTE; i++)
			release_pte(ptepage[i]);
		/* Zero out this page */
		memset((void *)page, 0, PAGE_SIZE);
		/* Now we can free the page of PTEs */
		free_page(page);
		/* And zero out the PMD entry so we never release it twice. */
		lguest_pmd_clear(pmd);
	}
}



/*H:445
 * We saw flush_user_mappings() twice: once from the flush_user_mappings()
 * hypercall and once in new_pgdir() when we re-used a top-level pgdir page.
 * It simply releases every PTE page from 0 up to the TASK_SIZE.
 */
static void flush_user_mappings(struct lguest *lg, int idx)
{
	unsigned int i;

	if(idx >= PTRS_PER_PGD)
		return;	
	/* Release every pgd entry up to the kernel's address. */
	for (i = 0; i < pgd_index(TASK_SIZE); i++)
		release_pgd(lg->pgdirs[idx].pgdir + i);
}



/*
 * We keep several page tables.  This is a simple routine to find the page
 * table (if any) corresponding to this top-level address the Guest has given
 * us.
 */
static unsigned int find_pgdir(struct lguest *lg, unsigned long pgtable)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++)
		if (lg->pgdirs[i].pgdir && lg->pgdirs[i].gpgdir == pgtable)
			break;
	return i;
}

/*H:435
 * And this is us, creating the new page directory. We do
 * not care about blank_pgdir on Lguest of ARM version.
 */
static unsigned int new_pgdir(struct lg_cpu *cpu,
                  unsigned long gpgdir,
                  int *blank_pgdir)
{
	unsigned int next;

	/*
	 * We pick one entry at random to throw out.  Choosing the Least
	 * Recently Used might be better, but this is easy.
	 */
	next = random32() % ARRAY_SIZE(cpu->lg->pgdirs);
	/* If it's never been allocated at all before, try now. */
	if (!cpu->lg->pgdirs[next].pgdir) {
		cpu->lg->pgdirs[next].pgdir =
			(pgd_t *)__get_free_pages(GFP_KERNEL, 2);
		/* If the allocation fails, just keep using the one we have */
		if (!cpu->lg->pgdirs[next].pgdir)
			next = cpu->cpu_pgd;
		else {
			unsigned int index = pgd_index(PAGE_OFFSET);
			unsigned int index_end = pgd_index(cpu->lg->mem_size +  PAGE_OFFSET);

			memset((void *)(cpu->lg->pgdirs[next].pgdir), 0, PAGE_SIZE * 4);

			
			/* We copy the entries of the Switcher, Guest Vectors, direct mapped memory */
			memcpy((void *)&cpu->lg->pgdirs[next].pgdir[index],
				(void *)&cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[index],
				(sizeof(pgd_t) * (index_end - index)));

			memcpy((void *)&cpu->lg->pgdirs[next].pgdir[SWITCHER_PGD_INDEX],
				(void *)&cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[SWITCHER_PGD_INDEX],
				sizeof(pgd_t));

			memcpy((void *)&cpu->lg->pgdirs[next].pgdir[pgd_index(GUEST_VECTOR_ADDRESS)],
				(void *)&cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[pgd_index(GUEST_VECTOR_ADDRESS)],
				sizeof(pgd_t));

			goto out;
		}
	}
	/* Release all the non-kernel mappings. */
	flush_user_mappings(cpu->lg, next);

out:	
	/* Record which Guest toplevel this shadows. */
	cpu->lg->pgdirs[next].gpgdir = gpgdir;

	return next;
}




/*H:430
 * (iv) Switching page tables
 *
 * Now we've seen all the page table setting and manipulation, let's see
 * what happens when the Guest changes page tables (ie. changes the top-level
 * pgdir).  This occurs on almost every context switch.
 */

void guest_switch_mm(struct lg_cpu *cpu, unsigned long pgtable,
                unsigned long context_id)
{
	int newpgdir, repin = 0;

	/* Look to see if we have this one already. */
	newpgdir = find_pgdir(cpu->lg, pgtable);

	/*
	 * If not, we allocate or mug an existing one. 
	 * On Lguest of ARM version we do not use "repin".
	 */
	if (newpgdir == ARRAY_SIZE(cpu->lg->pgdirs))
		newpgdir = new_pgdir(cpu, pgtable, &repin);
	/* Change the current pgd index to the new one. */
	cpu->cpu_pgd = newpgdir;
	cpu->regs->guest_cont_id = context_id;
	cpu->regs->gpgdir = pgtable;
}


/*
 *	When the Guest calls set_pte_ext, we do not know the
 *	top level pgdir, so we just release all entries of 
 *	non-direct mapped memory. Calling set_pte_ext on
 *	direct mapped memory will no be going to happen, because
 *	ARM linux adopt "section mapped mode" on this area. 
 */
void release_guest_nondirect_mapped_memory(struct lguest *lg)
{
	unsigned int i, j;
	unsigned long linemap_end = lg->mem_size +  PAGE_OFFSET;

	/* Every shadow pagetable this Guest has */
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++){
		if (lg->pgdirs[i].pgdir) {
			/* Every PGD entry except the Switcher, Guest Vectors, direct mapped memory */
			for (j = 0; j < pgd_index(PAGE_OFFSET); j++)
				release_pgd(lg->pgdirs[i].pgdir + j);
			
			for (j = pgd_index(linemap_end); j < SWITCHER_PGD_INDEX; j++)
				release_pgd(lg->pgdirs[i].pgdir + j);
			
			for (j = SWITCHER_PGD_INDEX + 1; j < PTRS_PER_PGD - 1; j++)
				release_pgd(lg->pgdirs[i].pgdir + j);
			
		}
	}
}




/*H:470
 * Finally, a routine which throws away everything: all PGD entries in all
 * the shadow page tables, including the Guest's kernel mappings.  This is used
 * when we destroy the Guest.
 */
static void release_all_pagetables(struct lguest *lg)
{
	unsigned int i, j;
    unsigned long linemap_end = lg->mem_size +  PAGE_OFFSET;

	/* Direct mapped PGD entries are shared by all tasks, so we release them first*/
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++){
		if (lg->pgdirs[i].pgdir) {
			for (j = pgd_index(PAGE_OFFSET); j < pgd_index(linemap_end); j++){
				release_pgd(lg->pgdirs[i].pgdir + j);
			}
			break;
		}
	}
    
	/* Every shadow pagetable this Guest has */
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++){
		if (lg->pgdirs[i].pgdir) {
			/* Every PGD entry except the Switcher's, Guest Vectors', direct mapped memory's */
			for (j = 0; j < pgd_index(PAGE_OFFSET); j++)
				release_pgd(lg->pgdirs[i].pgdir + j);

			for (j = pgd_index(linemap_end); j < SWITCHER_PGD_INDEX; j++)
				release_pgd(lg->pgdirs[i].pgdir + j);

			for (j = SWITCHER_PGD_INDEX + 1; j < PTRS_PER_PGD - 1; j++)
				release_pgd(lg->pgdirs[i].pgdir + j);
		}
	}
 }



/*H:420
 * This is the routine which actually sets the page table entry for then
 * "idx"'th shadow page table.
 *
 * Normally, we can just throw out the old entry and replace it with 0: if they
 * use it, guest_abort_handler() will put the new entry in.  We need to do this anyway:
 * The Guest expects L_PTE_YOUNG to be set on its PTE the first time a page
 * is read from, and L_PTE_DIRTY when it's written to.
 *
 */
static void do_set_pte(struct lg_cpu *cpu, int idx,
	       unsigned long vaddr, pte_t gpte, unsigned long ext)
{
	/* Look up the matching shadow page directory entry. */
	pgd_t *spgd = spgd_addr(cpu, idx, vaddr);
	pmd_t *spmd;
	spmd = pmd_offset(spgd, 0);

	/* If the top level isn't present, there's no entry to update. */
	if(check_pmd(pmd_val(*spmd))){
		/* Otherwise, start by releasing the existing entry. */
		pte_t *spte = spte_addr(cpu, spgd, vaddr);
		release_pte(*spte);

		/*
		 * If they're setting this entry as YOUNG, we might as well 
		 * put that entry they've given us in now.  
		 */
		if (pte_present(gpte) && pte_young(gpte)){
			check_gpte(cpu, gpte);
			set_guest_pte(spte, gpte_to_spte(cpu, gpte, pte_dirty(gpte)), ext);
		} else {
			/*
			 * Otherwise kill it and we can handle it in guest_abort_handle
			 * it in later.
			 */
			set_guest_pte(spte, __pte(0) , 0);
		}
	}
}

/*H:410
 * Updating a PTE entry is a little trickier.
 *
 * We keep track of several different page tables (the Guest uses one for each
 * process, so it makes sense to cache at least a few).  Each of these have
 * identical kernel parts: ie. every mapping above TASK_SIZE is the same for
 * all processes.  So when the page table above that address changes, we update
 * all the page tables, not just the current one.  This is rare.
 *
 * The benefit is that when we have to track a new page table, we can keep all
 * the kernel mappings.  This speeds up context switch immensely.
 */
void guest_set_pte(struct lg_cpu *cpu,
		   unsigned long gpgdir, unsigned long vaddr, pte_t gpte)
{
	unsigned long saddr = get_switcher_addr();


	/* 
	 * The PTEs of the Switcher and the Guest's exception vectors have been set 
	 * up before the Guest is about to run.
	 */
	if(((vaddr >= GUEST_VECTOR_ADDRESS) && (vaddr < GUEST_VECTOR_ADDRESS + PAGE_SIZE))
			|| ((vaddr >= saddr) && (vaddr < saddr + SWITCHER_TOTAL_SIZE))){
		return;
	}

	/* 
	 * Entries of Direct mapped memory have already be set up before the Guest runs,
	 * and they should never change before the Guest exits.
	 */
	if((vaddr >= PAGE_OFFSET) && (vaddr < cpu->lg->mem_size + PAGE_OFFSET)){
		return;
	}

	/*
	 * Kernel mappings must be changed on all top levels.  Slow, but doesn't
	 * happen often.
	 */
	if(vaddr >= TASK_SIZE) {
		unsigned int i;
		for (i = 0; i < ARRAY_SIZE(cpu->lg->pgdirs); i++)
			if (cpu->lg->pgdirs[i].pgdir)
				do_set_pte(cpu, i, vaddr, gpte, 0);
	} else {
		/* Is this page table one we have a shadow for? */
		int pgdir = find_pgdir(cpu->lg, gpgdir);
		if (pgdir != ARRAY_SIZE(cpu->lg->pgdirs))
			/* If so, do the update. */
			do_set_pte(cpu, pgdir, vaddr, gpte, PTE_EXT_NG);
	}
}



/*H:400
 * (iii) Setting up a page table entry when the Guest tells us one has changed.
 *
 *
 * We already saw that guest_abort_handler() will fill in the shadow page tables when
 * needed, so we can simply remove shadow page table entries whenever the Guest
 * tells us they've changed.  When the Guest tries to use the new entry it will
 * abort and guest_abort_handler() will fix it up.
 *
 * So with that in mind here's our code to to update a (top-level) PGD entry:
 */
void guest_set_pgd(struct lg_cpu *cpu, unsigned long gpgdir, u32 idx, unsigned long gpmd)
{
	int pgdir;


	/* 
	 * Entries of the Switcher and the Guest's exception vectors 
	 * should not be changed during the Guest runs.
	 */
	if (idx == SWITCHER_PGD_INDEX || idx >= (PTRS_PER_PGD -1))
		return;

	/* 
	 * Entries of Direct mapped memory have already be set up before the Guest runs,
	 * and they should never change before the Guest exits.
	 */
	if((idx >= pgd_index(PAGE_OFFSET)) && (idx < pgd_index(cpu->lg->mem_size + PAGE_OFFSET))){
		return;
	}

	/* If they're talking about a page table we have a shadow for... */
	pgdir = find_pgdir(cpu->lg, gpgdir);
	if (pgdir <  ARRAY_SIZE(cpu->lg->pgdirs))
		 /* ... throw it away. */
		release_pgd(cpu->lg->pgdirs[pgdir].pgdir + idx);
	return;
}



/*H:505
 * Note: ARM Linux kernel adopts "section mapped mode" for all driect mapped memory.
 * But for the Guest's shadow page tables, we cannot use section mapped mode,
 * and we have to adopt "page mapped mode". Because real physical address of memory 
 * which the Guest runs on is not necessarily to be 1M aligned.
 * Please see linux/arch/arm/mm/mmu.c 
 */
static unsigned long setup_gpagetables(struct lguest *lg,
				      unsigned long mem,
				      unsigned long initrd_size)
{
#define MAX_GUEST_MEMORY 128

	pgd_t __user *pgdir;
	unsigned long mem_base = (unsigned long)lg->mem_base;
	pmd_t *pmd;
	unsigned long ret = 0;
	unsigned int mapped_sections, i;
	pgd_t * pgd;

	pgd = (pgd_t *)get_zeroed_page(GFP_KERNEL);
	pmd = pmd_offset(pgd, 0);
	mapped_sections = mem / SECTION_SIZE;

	/* should we set a maximum size limit for the Guest? */
	if(mapped_sections > MAX_GUEST_MEMORY)
		mapped_sections = MAX_GUEST_MEMORY;

	/* Address of the Guest's kernel page table.*/
	pgdir = (pgd_t *)((void *)lg->kstart_paddr - 4 * PAGE_SIZE);

	/*we are sure that one page is enough*/	
	for(i = 0; i < mapped_sections; i++ ){
		unsigned long phys = PHYS_OFFSET + i * SECTION_SIZE;
		*pmd++ = __pmd(phys | GUEST_GPMD_KERNEL_FLAGS);
	}
	if (copy_to_user(&pgdir[pgd_index(PAGE_OFFSET)], 
						pgd, PAGE_SIZE)){
		ret = -EFAULT;
		goto error;
	}
	ret = (unsigned long)pgdir - mem_base + PHYS_OFFSET;
error:
	free_page((unsigned long)pgd);
	return ret;
#undef MAX_GUEST_MEMORY
}



/*
 * Setting up Guest's shadow page table entries according to 
 * "the Guest's virtual address".
 */
static int init_sptes(struct lguest *lg, pmd_t *pmd, unsigned long addr,
		unsigned long end)
{
	pte_t *pte;
	unsigned long pfn;
	unsigned long vpfn;
	 
	if (pmd_none(*pmd)) {
		pte = (pte_t *)get_zeroed_page(GFP_KERNEL);
		if(!pte){
			return -ENOMEM;
		}
		lguest_pmd_populate(pmd, __pa(pte) | _PAGE_KERNEL_TABLE);
	}

	/* count the virtual number of the page*/
	vpfn = ((unsigned long)lg->mem_base + addr -PAGE_OFFSET)  >> PAGE_SHIFT;

	pte = pte_offset_kernel(pmd, addr);
	do {
		/* finds the real physical number of the page*/ 
		pfn = get_pfn(vpfn, 1);
		/* Failed! The Guest cannot run. */
		if (pfn == -1UL) {
			return -EFAULT;
		}
		/* set PTEs of the Guest's shadow page table*/
		set_guest_pte(pte, pfn_pte(pfn,__pgprot(GUEST_BASE_PTE_FLAGS 
				| L_PTE_WRITE | L_PTE_DIRTY | L_PTE_EXEC)), 0);
	} while (pte++, vpfn++, addr += PAGE_SIZE, addr != end);
	return 0;
}




/*
 * Setting up Guest's shadow page table entries of direct mapped memory. 
 */
static int setup_spagetable(struct lguest *lg, unsigned long mem)
{
	pgd_t *spgd;
	unsigned long addr, end;
	int ret = 0;

	/**/
	addr = PAGE_OFFSET;
	end = addr + mem;

	/* Get the PGD entry of Shadow page table.*/
	spgd = &lg->pgdirs[0].pgdir[pgd_index(addr)];
	do {
		unsigned long next = pgd_addr_end(addr, end);
		pmd_t *pmd = pmd_offset(spgd, addr);

		ret = init_sptes(lg, pmd, addr, next);
		if(ret){
			goto error;
		}
		addr = next;
	} while (spgd++, addr != end);

error:
	return ret;
}



/*H:500
 * (vii) Setting up the page tables initially.
 *
 * When a Guest is first created, the Launcher will put the kernel initialisation 
 * parameters on a fixed a guest-physical address. 
 * please see linux/Documentation/arm/Setup and 
 *linux/arch/arm/include/asm/setup.h
 */
int init_guest_pagetable(struct lguest *lg)
{
	struct tag __user *base = (struct tag *)(lg->mem_base + BOOT_PARAMS_OFFSET);
	struct tag __user *t = base;
	u32 mem_size = 0;
	u32 tagtype;
	u32 initrd_size;
	u32 hdrsize = 0;
	int ret;

	/*Get the size of kernel initialisation parameters*/
	if(copy_from_user(&hdrsize, &t->hdr.size, sizeof(hdrsize))){
		return -EFAULT;
	}
	while(hdrsize){
		/* Read the tag type. linux/arch/arm/include/asm/setup.h */
		if(copy_from_user(&tagtype, &t->hdr.tag, sizeof(tagtype))){
			return -EFAULT;
		}
		/* Get the Gest memory size. */
		if(tagtype == ATAG_MEM){
			if(copy_from_user(&mem_size, &t->u.mem.size, sizeof(mem_size)))
				return -EFAULT;
		} 

		/* Get the Gest compressed ramdisk image size. if we use the ramdisk */
		if(tagtype == ATAG_INITRD2){
			if(copy_from_user(&initrd_size, &t->u.initrd.size, sizeof(initrd_size)))
				return -EFAULT;
		}
		
		/* Get the next tag. */
		t = (struct tag *)((__u32 *)(t) + hdrsize);
		if(copy_from_user(&hdrsize, &t->hdr.size, sizeof(hdrsize))){
			return -EFAULT;
		}
	}
	if(!mem_size)
		return -ENOMEM;

	/*
	 * We start on the first shadow page table, and give it a blank PGD page.
	 */
	lg->pgdirs[0].gpgdir = setup_gpagetables(lg, mem_size, initrd_size);
	if (IS_ERR_VALUE(lg->pgdirs[0].gpgdir))
		return lg->pgdirs[0].gpgdir;
	lg->pgdirs[0].pgdir = (pgd_t *)__get_free_pages(GFP_KERNEL, 2); 
	if (!lg->pgdirs[0].pgdir)
		return -ENOMEM;

	memset((void *)lg->pgdirs[0].pgdir, 0, PAGE_SIZE << 2);
	
	lg->mem_size = mem_size;

	/* This is the current page table. */
	lg->cpus[0].cpu_pgd = 0;

	/*setup shdow page table*/
	ret = setup_spagetable(lg, mem_size);
	if(ret){
		release_all_pagetables(lg);
		return ret;
	}
	return 0;
}


void page_table_guest_hcall_init(struct lg_cpu *cpu)
{
	
	cpu->regs->gpgdir = cpu->lg->pgdirs[0].gpgdir;
}

/* When a Guest dies, our cleanup is fairly simple. */
void free_guest_pagetable(struct lguest *lg)
{
	unsigned int i;

	/* Throw away all page table pages. */
	release_all_pagetables(lg);
	/* Now free the top levels: free_page() can handle 0 just fine. */
	for (i = 0; i < ARRAY_SIZE(lg->pgdirs); i++){
        if(lg->pgdirs[i].pgdir){
            free_pages((unsigned long)lg->pgdirs[i].pgdir, 2);
            lg->pgdirs[i].pgdir = NULL;
        }
    }
}

/*H:480
 * (vi) Mapping the Switcher when the Guest is about to run.
 *
 * The Switcher and the two pages for this CPU need to be visible in the
 * Guest (and not the pages for other CPUs).  We have the appropriate PTE pages
 * for each CPU already set up, we just need to hook them in now we know which
 * Guest is about to run on this CPU.
 */
void map_switcher_in_guest(struct lg_cpu *cpu, struct lguest_pages *pages)
{
	pte_t *switcher_pte_page = __get_cpu_var(switcher_pte_pages);
	pte_t regs_pte;
	pmd_t *pmd;
	pgd_t *pgd;
	unsigned int index;

	pgd = &cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[pgd_index(get_switcher_addr())];
	pmd = pmd_offset(pgd, get_switcher_addr());

	lguest_pmd_populate(pmd, __pa(switcher_pte_page) | _PAGE_KERNEL_TABLE);
	
	/*
	 * We also change the Switcher PTE page.  When we're running the Guest,
	 * we want the Guest's "regs" page to appear where the first Switcher
	 * page for this CPU is.  This is an optimization: when the Switcher
	 * saves the Guest registers, it saves them into the first page of this
	 * CPU's "struct lguest_pages": if we make sure the Guest's register
	 * page is already mapped there, we don't have to copy them out
	 * again.
	 */

	regs_pte = pfn_pte(__pa(cpu->regs_page) >> PAGE_SHIFT, 
				__pgprot(GUEST_BASE_PTE_FLAGS | L_PTE_WRITE | L_PTE_DIRTY));
	index = PTRS_PER_PTE + __pte_index((unsigned long)pages);
	set_guest_pte(&switcher_pte_page[index], regs_pte, 0);
}


/*H:360
 * (ii) Mapping the exception vectors when the Guest is about to run.
 *
 * Mapping the exception vectors when the Guest is about to run.
 */
void map_vectors_in_guest(struct lg_cpu *cpu, unsigned long gvector_addr)
{
	pgd_t *pgd;
	pmd_t *pmd;


	pgd = &cpu->lg->pgdirs[cpu->cpu_pgd].pgdir[pgd_index(gvector_addr)];
	pmd = pmd_offset(pgd, gvector_addr);

	/* Setting the PGD of exception vectors */
	lguest_pmd_populate(pmd, __pa(vectors_pte_page) | _PAGE_USER_TABLE);
}


/*:*/

static void free_switcher_pte_pages(void)
{
	unsigned int i;

	for_each_possible_cpu(i)
		free_page((long)switcher_pte_page(i));
}

/*H:520
 * Setting up the Switcher PTE page for given CPU is fairly easy, given
 * the CPU number and the "struct page"s for the Switcher code itself.
 *
 * Currently the Switcher is less than a page long, so "pages" is always 1.
 */
static __init void populate_switcher_pte_page(unsigned int cpu,
					      struct page *switcher_page[],
					      unsigned int pages)
{
	unsigned int i;
	unsigned int index = PTRS_PER_PTE + __pte_index((unsigned long)get_switcher_addr());
	pte_t *pte = switcher_pte_page(cpu);

	pte += index; 

	/* The first entries are easy: they map the Switcher code. */
	for (i = 0; i < pages; i++) {
		set_guest_pte(&pte[i], mk_pte(switcher_page[i],
                    __pgprot(GUEST_BASE_PTE_FLAGS | L_PTE_EXEC)),0);
	}
	/* The only other thing we map is this CPU's pair of pages. */
	i = pages + cpu*2;

	/* First page (Guest registers) is writable from the Guest */
	set_guest_pte(&pte[i], pfn_pte(page_to_pfn(switcher_page[i]),
			__pgprot(GUEST_BASE_PTE_FLAGS | L_PTE_WRITE | L_PTE_DIRTY)),0);

	/*
	 * The second page contains the "struct lguest_ro_state", and is
	 * read-only.
	 */
	set_guest_pte(&pte[i+1], pfn_pte(page_to_pfn(switcher_page[i+1]),
			__pgprot(GUEST_BASE_PTE_FLAGS)),0);

}


/* Setting up the Guest's exception vectors' PTE page */
__init int init_vectors_pte(unsigned long addr, unsigned long gvector_addr)
{
#define GUEST_VECTOR_PTE_FLAGS \
	(L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_MT_WRITETHROUGH | L_PTE_USER | L_PTE_EXEC)

	pte_t vector_pte;
	vectors_pte_page = (pte_t *)get_zeroed_page(GFP_KERNEL);
	if(!vectors_pte_page){
		return -ENOMEM;
	}

	vector_pte = pfn_pte(__pa(addr) >> PAGE_SHIFT, __pgprot(GUEST_VECTOR_PTE_FLAGS)); 
	set_guest_pte(&vectors_pte_page[PTRS_PER_PTE + __pte_index(gvector_addr)], vector_pte,0);
#undef GUEST_VECTOR_PTE_FLAGS
	return 0;
}


void free_vectors_pte(void)
{
	free_page((unsigned long)vectors_pte_page);
}




/*H:510
 * At boot or module load time, init_pagetables() allocates and populates
 * the Switcher PTE page for each CPU.
 */
__init int init_pagetables(struct page **switcher_page, unsigned int pages)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		switcher_pte_page(i) = (pte_t *)get_zeroed_page(GFP_KERNEL);
		if (!switcher_pte_page(i)) {
			free_switcher_pte_pages();
			return -ENOMEM;
		}
		populate_switcher_pte_page(i, switcher_page, pages);
	}
	return 0;
}

/*:*/

/* Cleaning up simply involves freeing the PTE page for each CPU. */
void free_pagetables(void)
{
	free_switcher_pte_pages();
}
