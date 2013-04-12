/*
 * Trap handler entry and exit code, written in assembly language.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/mmu.h>
#include <inc/trap.h>

#include <kern/cpu.h>



/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
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
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


TRAPHANDLER_NOEC(tdivide, T_DIVIDE)
TRAPHANDLER_NOEC(tdebug, T_DEBUG)
TRAPHANDLER_NOEC(tnmi, T_NMI)
TRAPHANDLER_NOEC(tbrkpt, T_BRKPT)
TRAPHANDLER_NOEC(toflow, T_OFLOW)
TRAPHANDLER_NOEC(tbound, T_BOUND)
TRAPHANDLER_NOEC(tillop, T_ILLOP)
TRAPHANDLER_NOEC(tdevice, T_DEVICE)
TRAPHANDLER     (tdblflt, T_DBLFLT)
TRAPHANDLER     (ttss, T_TSS)
TRAPHANDLER     (tsegnp, T_SEGNP)
TRAPHANDLER     (tstack, T_STACK)
TRAPHANDLER     (tgpflt, T_GPFLT)
TRAPHANDLER     (tpgflt, T_PGFLT)
TRAPHANDLER_NOEC(tfperr, T_FPERR)
TRAPHANDLER     (talign, T_ALIGN)
TRAPHANDLER_NOEC(tmchk, T_MCHK)
TRAPHANDLER_NOEC(tsimd, T_SIMD)
TRAPHANDLER_NOEC(tsecev, T_SECEV)
TRAPHANDLER_NOEC(tirq0, T_IRQ0)
TRAPHANDLER_NOEC(tirqkbd, (T_IRQ0+IRQ_KBD))
TRAPHANDLER_NOEC(tirqser, (T_IRQ0+IRQ_SERIAL))
TRAPHANDLER_NOEC(tsystem, T_SYSCALL)
TRAPHANDLER_NOEC(tltimer, T_LTIMER)

/*
 * Lab 1: Your code here for _alltraps
 */
.text
.globl _alltraps
.type _alltraps,@function
_alltraps:
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    movw $CPU_GDT_KDATA, %ax
    movw %ax, %ds
    movw %ax, %es
  
    pushl %esp
    call trap

//
// Trap return code.
// C code in the kernel will call this function to return from a trap,
// providing the 
// Restore the CPU state from a given trapframe struct
// and return from the trap using the processor's 'iret' instruction.
// This function does not return to the caller,
// since the new CPU state this function loads
// replaces the caller's stack pointer and other registers.
//
.globl	trap_return
.type	trap_return,@function
.p2align 4, 0x90		/* 16-byte alignment, nop filled */
trap_return:
  movl	4(%esp),%esp // Point esp to the trapframe *
  popal
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $8, %esp 
  iret

1:	jmp	1b		// just spin

