typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;


#define PHYS_SDRAM      0x80000000
#define BOOT_PARAMS     (PHYS_SDRAM + 0x100)
#define MACH_TYPE_OMAP3_BEAGLE         1546


#define tag_size(type)	((sizeof(struct tag_header) + sizeof(struct type)) >> 2)
#define tag_next(t)	((struct tag *)((u32 *)(t) + (t)->hdr.size))

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE	0x00000000

struct tag_header {
	u32 size;
	u32 tag;
};

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE	0x54410001

struct tag_core {
	u32 flags;		/* bit 0 = read-only */
	u32 pagesize;
	u32 rootdev;
};

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_mem32 {
	u32	size;
	u32	start;	/* physical start address */
};


/* command line: \0 terminated string */
#define ATAG_CMDLINE	0x54410009

struct tag_cmdline {
	char	cmdline[1];	/* this is the minimum size */
};


/* describes how the ramdisk will be used in kernel */
#define ATAG_RAMDISK	0x54410004

struct tag_ramdisk {
	u32 flags;	/* bit 0 = load, bit 1 = prompt */
	u32 size;	/* decompressed ramdisk size in _kilo_ bytes */
	u32 start;	/* starting block of floppy-based RAM disk image */
};

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * its depreciated.
 */
#define ATAG_INITRD	0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2	0x54420005

struct tag_initrd {
	u32 start;	/* physical start address */
	u32 size;	/* size of compressed ramdisk image in bytes */
};




struct tag {
	struct tag_header hdr;
	union {
		struct tag_core		core;
		struct tag_mem32	mem;
		struct tag_ramdisk	ramdisk;
		struct tag_initrd	initrd;
		struct tag_cmdline	cmdline;
	} u;
};




struct l_mem32_tag {
	u32 flag;	/* 0 don't setup this tag, else do it*/
	struct tag_mem32 mem;
};

struct l_initrd_tag {
	u32 flag;
	struct tag_initrd initrd;
};

struct l_cmdline_tag {
    u32 flag;
    char *cmdline;
};


/* lguest launcher will use this struct to setup tags for guest kernel*/
struct lguest_boot_params
{
	struct tag *tags_addr;		/* tags_addr = (struct tag *)BOOT_PARAMS */
	struct l_mem32_tag mem;		/* mem.mem.start = PHYS_SDRAM*/
	struct l_initrd_tag initrd;
	struct l_cmdline_tag cmdline;
};




