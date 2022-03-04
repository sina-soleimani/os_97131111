/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
 
	/* whether or not to use error code found here:
	 * www3.cs.stonybrook.edu/~porter/courses/cse506/f14/ref/i386/s09_10.htm
	 */
	TRAPHANDLER_NOEC(divide_handler, T_DIVIDE)
	TRAPHANDLER_NOEC(debug_handler, T_DEBUG)
	TRAPHANDLER_NOEC(nmi_handler, T_NMI)
	TRAPHANDLER_NOEC(brkpt_handler, T_BRKPT)
	TRAPHANDLER_NOEC(oflow_handler, T_OFLOW)
	TRAPHANDLER_NOEC(bound_handler, T_BOUND)
	TRAPHANDLER_NOEC(illop_handler, T_ILLOP)
	TRAPHANDLER_NOEC(device_handler, T_DEVICE)
	TRAPHANDLER(dblflt_handler, T_DBLFLT)
	TRAPHANDLER(tss_handler, T_TSS)
	TRAPHANDLER(segnp_handler, T_SEGNP)
	TRAPHANDLER(stack_handler, T_STACK)
	TRAPHANDLER(gpflt_handler, T_GPFLT)
	TRAPHANDLER(pgflt_handler, T_PGFLT)
	TRAPHANDLER_NOEC(fperr_handler, T_FPERR)
	TRAPHANDLER(align_handler, T_ALIGN)
	TRAPHANDLER_NOEC(mchk_handler, T_MCHK)
	TRAPHANDLER_NOEC(simderr_handler, T_SIMDERR)
	TRAPHANDLER_NOEC(syscall_handler, T_SYSCALL)
	TRAPHANDLER_NOEC(default_handler, T_DEFAULT)



/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:
	
	/*xorq %rax,%rax
	 *movw %ds, %ax
	 *push %ds
	 *movw %es, %ax
	 *push %es
	 */ // this is all wrong, I cannot mess rax
	 
	subq $16, %rsp
	movq $0, 0(%rsp)
	movq $0, 8(%rsp)
	movw %es, 0(%rsp)
	movw %ds, 8(%rsp)
	PUSHA
	
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	
	// @@@ move the first arg into rdi, as the struct Trapframe *tf
	// @@@ http://eli.thegreenplace.net/2011/09/06/stack-frame-layout-on-x86-64/
	
	movq %rsp, %rdi
	call trap
