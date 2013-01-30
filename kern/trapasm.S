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


.text
TRAPHANDLER_NOEC(trap0, 0)
TRAPHANDLER(trap1, 1)
TRAPHANDLER_NOEC(trap2, 2)
TRAPHANDLER_NOEC(trap3, 3)
TRAPHANDLER_NOEC(trap4, 4)
TRAPHANDLER_NOEC(trap5, 5)
TRAPHANDLER_NOEC(trap6, 6)
TRAPHANDLER_NOEC(trap7, 7)
TRAPHANDLER(trap8, 8)
TRAPHANDLER_NOEC(trap9, 9)
TRAPHANDLER(trap10, 10)
TRAPHANDLER(trap11, 11)
TRAPHANDLER(trap12, 12)
TRAPHANDLER(trap13, 13)
TRAPHANDLER(trap14, 14)
TRAPHANDLER_NOEC(trap15, 15)
TRAPHANDLER(trap16, 16)
TRAPHANDLER(trap17, 17)
TRAPHANDLER_NOEC(trap18, 18)
TRAPHANDLER_NOEC(trap19, 19)
TRAPHANDLER_NOEC(trap20, 20)
TRAPHANDLER_NOEC(trap21, 21)
TRAPHANDLER_NOEC(trap22, 22)
TRAPHANDLER_NOEC(trap23, 23)
TRAPHANDLER_NOEC(trap24, 24)
TRAPHANDLER_NOEC(trap25, 25)
TRAPHANDLER_NOEC(trap26, 26)
TRAPHANDLER_NOEC(trap27, 27)
TRAPHANDLER_NOEC(trap28, 28)
TRAPHANDLER_NOEC(trap29, 29)
TRAPHANDLER_NOEC(trap30, 30)
TRAPHANDLER_NOEC(trap31, 31)
TRAPHANDLER_NOEC(trap32, 32)

.data
.globl handlers
handlers:
	.long trap0
	.long trap1
	.long trap2
	.long trap3
	.long trap4
	.long trap5
	.long trap6
	.long trap7
	.long trap8
	.long trap9
	.long trap10
	.long trap11
	.long trap12
	.long trap13
	.long trap14
	.long trap15
	.long trap16
	.long trap17
	.long trap18
	.long trap19
	.long trap20
	.long trap21
	.long trap22
	.long trap23
	.long trap24
	.long trap25
	.long trap26
	.long trap27
	.long trap28
	.long trap29
	.long trap30
	.long trap31
	.long trap32

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
  addl $0x4, %esp // Point esp to the trapframe *
  popal
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $0x8, %esp 
  iret

1:	jmp	1b		// just spin

