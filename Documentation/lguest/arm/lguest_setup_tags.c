#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lguest_params.h"


/* 
 * Before a Guest runs, the Launcher will put the kernel initialisation 
 * parameters on a fixed a guest-physical address. 
 * please see linux/Documentation/arm/Setup and 
 * linux/arch/arm/include/asm/setup.h
 */


/* The list must start with an ATAG_CORE node */
static void setup_start_tag (struct tag **tags_addr)
{
	struct tag *ptag = *tags_addr;

	ptag->hdr.tag = ATAG_CORE;
	ptag->hdr.size = tag_size (tag_core);
	ptag->u.core.flags = 0;
	ptag->u.core.pagesize = 0;
	ptag->u.core.rootdev = 0;

	ptag = tag_next (ptag);
	*tags_addr = ptag;
}


/* Setup memory tag. set the Guest memory address and size */
static void setup_memory_tag (struct tag **tags_addr, u32 start, u32 size)
{
	struct tag *ptag = *tags_addr;
	ptag->hdr.tag = ATAG_MEM;
	ptag->hdr.size = tag_size (tag_mem32);

	ptag->u.mem.start = start;
	ptag->u.mem.size = size;

	/*renew the tag pointer*/
	ptag = tag_next (ptag);
	*tags_addr = ptag;
}


static void setup_commandline_tag (struct tag **tags_addr,char *commandline)
{
	struct tag *ptag = *tags_addr;
	char *p;

	if (!commandline)
		return;

	/* eat leading white space */
	for (p = commandline; *p == ' '; p++);

	/* 
	 * skip non-existent command lines so the kernel will still
	 * use its default command line.
	 */
	if (*p == '\0')
		return;

	ptag->hdr.tag = ATAG_CMDLINE;
	ptag->hdr.size =
		(sizeof (struct tag_header) + strlen (p) + 1 + 4) >> 2;

	strcpy (ptag->u.cmdline.cmdline, p);

	/* renew the tag pointer */
	ptag = tag_next (ptag);
	*tags_addr = ptag;
}

/* Setup ramdisk tag. set compressed ramdisk address  and size */
static void setup_initrd_tag (struct tag **tags_addr,u32 start, u32 size)
{
	struct tag *ptag = *tags_addr;
	
	ptag->hdr.tag = ATAG_INITRD2;
	ptag->hdr.size = tag_size (tag_initrd);

	ptag->u.initrd.start = start;
	ptag->u.initrd.size = size;

	ptag = tag_next (ptag);
	*tags_addr = ptag;
}



/* The list ends with an ATAG_NONE node. */
static void setup_end_tag (struct tag **tags_addr)
{
	struct tag *ptag = *tags_addr;

	ptag->hdr.tag = ATAG_NONE;
	ptag->hdr.size = 0;
}


/* set up all tags for the Guest Kernel*/
int lguest_setup_tags(struct lguest_boot_params *lbp)
{
	struct tag **tags_addr = &lbp->tags_addr;

    setup_start_tag (tags_addr);
	if(lbp->mem.flag) {
	    setup_memory_tag(tags_addr, lbp->mem.mem.start, 
						lbp->mem.mem.size);
	}
	if(lbp->initrd.flag) {
		setup_initrd_tag (tags_addr, lbp->initrd.initrd.start, 
						lbp->initrd.initrd.size);
	}
	setup_commandline_tag (tags_addr, lbp->cmdline.cmdline);
    setup_end_tag (tags_addr);
    return 0;
}

