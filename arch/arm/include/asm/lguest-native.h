#ifndef __ASM_LGUEST_NATIVE_H
#define __ASM_LGUEST_NATIVE_H

/* The name of the native version of a function. */
#define LGUEST_NATIVE_NAME(fn) __lguest_native_##fn
/* The type of a hook function. */
#define LGUEST_HOOK_TYPE(fn)   typeof(&LGUEST_NATIVE_NAME(fn))

#ifdef CONFIG_ARM_LGUEST_GUEST
/* Define a function that can be hooked by lguest. */
#define LGUEST_NATIVE(fn)      LGUEST_NATIVE_NAME(fn)
/* Define the hook point for an LGUEST_NATIVE function. */
#define lguest_define_hook(fn) extern LGUEST_HOOK_TYPE(fn) fn
#else
#define LGUEST_NATIVE(fn)      fn
#define lguest_define_hook(fn) 
#endif

#endif //__ASM_LGUEST_NATIVE_H

