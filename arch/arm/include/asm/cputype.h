#ifndef __ASM_ARM_CPUTYPE_H
#define __ASM_ARM_CPUTYPE_H

#include <linux/stringify.h>
#include <linux/kernel.h>
#include <asm/lguest-native.h>

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3
#define CPUID_MPIDR	5

#define CPUID_EXT_LOC_PFR0	"c1, 0"
#define CPUID_EXT_LOC_PFR1	"c1, 1"
#define CPUID_EXT_LOC_DFR0	"c1, 2"
#define CPUID_EXT_LOC_AFR0	"c1, 3"
#define CPUID_EXT_LOC_MMFR0	"c1, 4"
#define CPUID_EXT_LOC_MMFR1	"c1, 5"
#define CPUID_EXT_LOC_MMFR2	"c1, 6"
#define CPUID_EXT_LOC_MMFR3	"c1, 7"
#define CPUID_EXT_LOC_ISAR0	"c2, 0"
#define CPUID_EXT_LOC_ISAR1	"c2, 1"
#define CPUID_EXT_LOC_ISAR2	"c2, 2"
#define CPUID_EXT_LOC_ISAR3	"c2, 3"
#define CPUID_EXT_LOC_ISAR4	"c2, 4"
#define CPUID_EXT_LOC_ISAR5	"c2, 5"

#define CPUID_EXT_ID_PFR0  0x1
#define CPUID_EXT_ID_PFR1  0x2
#define CPUID_EXT_ID_DFR0  0x3
#define CPUID_EXT_ID_AFR0  0x4
#define CPUID_EXT_ID_MMFR0 0x5
#define CPUID_EXT_ID_MMFR1 0x6
#define CPUID_EXT_ID_MMFR2 0x7
#define CPUID_EXT_ID_MMFR3 0x8
#define CPUID_EXT_ID_ISAR0 0x9
#define CPUID_EXT_ID_ISAR1 0xA
#define CPUID_EXT_ID_ISAR2 0xB
#define CPUID_EXT_ID_ISAR3 0xC
#define CPUID_EXT_ID_ISAR4 0xD
#define CPUID_EXT_ID_ISAR5 0xE

extern unsigned int processor_id;

#ifdef CONFIG_CPU_CP15
#define _read_cpuid(reg)						\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, c0, " __stringify(reg)	\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#define _read_cpuid_ext(ext_reg)					\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, " ext_reg		\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})

#ifdef CONFIG_ARM_LGUEST_GUEST

/* Define extended CPUIDs as an enum. */
#define CPUID_EXT_PFR0  CPUID_EXT_ID_PFR0
#define CPUID_EXT_PFR1  CPUID_EXT_ID_PFR1
#define CPUID_EXT_DFR0  CPUID_EXT_ID_DFR0
#define CPUID_EXT_AFR0  CPUID_EXT_ID_AFR0
#define CPUID_EXT_MMFR0 CPUID_EXT_ID_MMFR0
#define CPUID_EXT_MMFR1 CPUID_EXT_ID_MMFR1
#define CPUID_EXT_MMFR2 CPUID_EXT_ID_MMFR2
#define CPUID_EXT_MMFR3 CPUID_EXT_ID_MMFR3
#define CPUID_EXT_ISAR0 CPUID_EXT_ID_ISAR0
#define CPUID_EXT_ISAR1 CPUID_EXT_ID_ISAR1
#define CPUID_EXT_ISAR2 CPUID_EXT_ID_ISAR2
#define CPUID_EXT_ISAR3 CPUID_EXT_ID_ISAR3
#define CPUID_EXT_ISAR4 CPUID_EXT_ID_ISAR4
#define CPUID_EXT_ISAR5 CPUID_EXT_ID_ISAR5

/* 
 * Since the read_cpu/read_cpu_ext macros generate assembly based
 * on their constant parameter, we need to decode our non-constant
 * parameter into a constant that can go into the generated assembly.
 */
static unsigned int LGUEST_NATIVE_NAME(read_cpuid) (unsigned long reg)
{
	switch (reg) {
		case CPUID_ID:
			return _read_cpuid(CPUID_ID);
		case CPUID_CACHETYPE:
			return _read_cpuid(CPUID_CACHETYPE);
		case CPUID_TCM:
			return _read_cpuid(CPUID_TCM);
		case CPUID_TLBTYPE:
			return _read_cpuid(CPUID_TLBTYPE);
		case CPUID_MPIDR:
			return _read_cpuid(CPUID_MPIDR);
		default:
			return 0;
	}
}
lguest_define_hook(read_cpuid);

/* 
 * Decode the enum back into the string constant we need.
 * We can't use a lookup table for the same reasons as above.
 */
static unsigned int LGUEST_NATIVE_NAME(read_cpuid_ext) (unsigned long ext_reg)
{
	switch (ext_reg) {
		case CPUID_EXT_ID_PFR0:
			return _read_cpuid_ext(CPUID_EXT_LOC_PFR0);
		case CPUID_EXT_ID_PFR1:
			return _read_cpuid_ext(CPUID_EXT_LOC_PFR1);
		case CPUID_EXT_ID_DFR0:
			return _read_cpuid_ext(CPUID_EXT_LOC_DFR0);
		case CPUID_EXT_ID_AFR0:
			return _read_cpuid_ext(CPUID_EXT_LOC_AFR0);
		case CPUID_EXT_ID_MMFR0:
			return _read_cpuid_ext(CPUID_EXT_LOC_MMFR0);
		case CPUID_EXT_ID_MMFR1:
			return _read_cpuid_ext(CPUID_EXT_LOC_MMFR1);
		case CPUID_EXT_ID_MMFR2:
			return _read_cpuid_ext(CPUID_EXT_LOC_MMFR2);
		case CPUID_EXT_ID_MMFR3:
			return _read_cpuid_ext(CPUID_EXT_LOC_MMFR3);
		case CPUID_EXT_ID_ISAR0:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR0);
		case CPUID_EXT_ID_ISAR1:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR1);
		case CPUID_EXT_ID_ISAR2:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR2);
		case CPUID_EXT_ID_ISAR3:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR3);
		case CPUID_EXT_ID_ISAR4:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR4);
		case CPUID_EXT_ID_ISAR5:
			return _read_cpuid_ext(CPUID_EXT_LOC_ISAR5);
		default:
			return 0;
	}
}
lguest_define_hook(read_cpuid_ext);

#else	//!CONFIG_ARM_LGUEST_GUEST

#define CPUID_EXT_PFR0  CPUID_EXT_LOC_PFR0
#define CPUID_EXT_PFR1  CPUID_EXT_LOC_PFR1
#define CPUID_EXT_DFR0  CPUID_EXT_LOC_DFR0
#define CPUID_EXT_AFR0  CPUID_EXT_LOC_AFR0
#define CPUID_EXT_MMFR0 CPUID_EXT_LOC_MMFR0
#define CPUID_EXT_MMFR1 CPUID_EXT_LOC_MMFR1
#define CPUID_EXT_MMFR2 CPUID_EXT_LOC_MMFR2
#define CPUID_EXT_MMFR3 CPUID_EXT_LOC_MMFR3
#define CPUID_EXT_ISAR0 CPUID_EXT_LOC_ISAR0
#define CPUID_EXT_ISAR1 CPUID_EXT_LOC_ISAR1
#define CPUID_EXT_ISAR2 CPUID_EXT_LOC_ISAR2
#define CPUID_EXT_ISAR3 CPUID_EXT_LOC_ISAR3
#define CPUID_EXT_ISAR4 CPUID_EXT_LOC_ISAR4
#define CPUID_EXT_ISAR5 CPUID_EXT_LOC_ISAR5

#define read_cpuid(reg)     _read_cpuid(reg)
#define read_cpuid_ext(reg) _read_cpuid_ext(reg)

#endif // CONFIG_ARM_LGUEST_GUEST	

#else /* !CONFIG_CPU_CP15 */
#define read_cpuid(reg) (processor_id)
#define read_cpuid_ext(reg) 0
#endif

/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return read_cpuid(CPUID_ID);
}

static inline unsigned int __attribute_const__ read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int __attribute_const__ read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

static inline unsigned int __attribute_const__ read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}

/*
 * Intel's XScale3 core supports some v6 features (supersections, L2)
 * but advertises itself as v5 as it does not support the v6 ISA.  For
 * this reason, we need a way to explicitly test for this type of CPU.
 */
#ifndef CONFIG_CPU_XSC3
#define cpu_is_xsc3()	0
#else
static inline int cpu_is_xsc3(void)
{
	unsigned int id;
	id = read_cpuid_id() & 0xffffe000;
	/* It covers both Intel ID and Marvell ID */
	if ((id == 0x69056000) || (id == 0x56056000))
		return 1;

	return 0;
}
#endif

#if !defined(CONFIG_CPU_XSCALE) && !defined(CONFIG_CPU_XSC3)
#define	cpu_is_xscale()	0
#else
#define	cpu_is_xscale()	1
#endif

#endif
