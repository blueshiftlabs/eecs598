/*P:800
 * There are three classes of interrupts:
 *
 * 1) Real hardware interrupts which occur while we're running the Guest,
 * 2) Interrupts for virtual devices attached to the Guest, and
 * 3) Data Aborts and Prefetch Aborts and Undefined Instructions from the Guest.
 *
 * Real hardware interrupts must be delivered to the Host, not the Guest.
 * Virtual interrupts must be delivered to the Guest. Data Aborts and Prefetch 
 * Aborts from the Guest should also be deliverd to the Host, if the Host cannot 
 * solve it, We should tell the Guest, and let the Guest handle it by itself.
 * As for Undefined Instructions, the Guest has to handle it by itself.
:*/
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include "../lg.h"


/*  
 *  This code is based on the linux/driver/lguest/interrupts_and_traps.c,
 *  written by Rusty Russell.
 *  Modified by Mingli Wu for Lguest of ARM version.
 */



/* And this is the routine when we want to set an interrupt for the Guest. */
void set_interrupt(struct lg_cpu *cpu, unsigned int irq)
{
	/*
	 * Next time before the Guest runs, the Host will see if there are 
	 * virtual irqs which the Guest need to be dealed with
	 */
	set_bit(irq, cpu->irqs_pending);
	
	/*
	 * Make sure it sees it; it might be asleep (eg. halted), or running
	 * the Guest right now, in which case kick_process() will knock it out.
	 */
	if (!wake_up_process(cpu->tsk))
		kick_process(cpu->tsk);

}



void send_interrupt_to_guest(struct lg_cpu *cpu)
{
	unsigned int irq;
    DECLARE_BITMAP(blk, LGUEST_IRQS);

	memcpy(blk, cpu->regs->blocked_interrupts, sizeof(blk));
	bitmap_andnot(blk, cpu->irqs_pending, blk, LGUEST_IRQS);
	
	if((irq = find_first_bit(blk, LGUEST_IRQS)) < LGUEST_IRQS){
	    if(cpu->halted){
		    cpu->halted = 0;
		}
		/* 
		 * when the Guest see cpu->regs->irq_pending = 1, the Guest 
		 * will come back to take pending virtual irqs.
		 */
		cpu->regs->irq_pending = 1;
	} else {
		cpu->regs->irq_pending = 0;
		return;
	}

	
	/* 
	 * If the Guest' local irqs are disabled, we just do othing until
	 * the Guest's local irqs are enabled.
	 */
	if(cpu->regs->irq_disabled & PSR_I_BIT){
		return;
	}


	/*
	 * when we find a virtual irq that the Guest can handle it,
	 * we set this virtual irq bit in the member irqs_pending of 
	 * "struct lguest_regs", the Guest can see it when the Guest
	 * restores to run. 
	 */
	while((irq = find_first_bit(blk, LGUEST_IRQS)) < LGUEST_IRQS){
		set_bit(irq, cpu->regs->irqs_pending);
		clear_bit(irq, cpu->irqs_pending);
		clear_bit(irq,blk);
	}
	
	/*All virtual irqs are sent to the Guest, there is no pending irqs now*/
	cpu->regs->irq_pending = 0;
}




int init_interrupts(void)
{

	return 0;
}

void free_interrupts(void)
{


}




/*H:200
 * The Guest Clock.
 *
 * There are two sources of virtual interrupts.  We saw one in lguest_user.c:
 * the Launcher sending interrupts for virtual devices.  The other is the Guest
 * timer interrupt.
 *
 * The Guest uses the LHCALL_SET_CLOCKEVENT hypercall to tell us how long to
 * the next timer interrupt (in nanoseconds).  We use the high-resolution timer
 * infrastructure to set a callback at that time.
 *
 * 0 means "turn off the clock".
 */
void guest_set_clockevent(struct lg_cpu *cpu, unsigned long delta)
{
	ktime_t expires;

	if (unlikely(delta == 0)) {
		/* Clock event device is shutting down. */
		hrtimer_cancel(&cpu->hrt);
		return;
	}

	/*
	 * We use wallclock time here, so the Guest might not be running for
	 * all the time between now and the timer interrupt it asked for.  This
	 * is almost always the right thing to do.
	 */
	expires = ktime_add_ns(ktime_get_real(), delta);
	hrtimer_start(&cpu->hrt, expires, HRTIMER_MODE_ABS);
}

/* This is the function called when the Guest's timer expires. */
static enum hrtimer_restart clockdev_fn(struct hrtimer *timer)
{
	struct lg_cpu *cpu = container_of(timer, struct lg_cpu, hrt);

	/* Remember the first interrupt is the timer interrupt. */
	set_interrupt(cpu, 0);
	return HRTIMER_NORESTART;
}

/* This sets up the timer for this Guest. */
void init_clockdev(struct lg_cpu *cpu)
{
	hrtimer_init(&cpu->hrt, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	cpu->hrt.function = clockdev_fn;
}
