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
/*
.text
TRAPHANDLER(trap0, 0)
TRAPHANDLER(trap1, 1)
TRAPHANDLER(trap2, 2)
TRAPHANDLER(trap3, 3)
TRAPHANDLER(trap4, 4)
TRAPHANDLER(trap5, 5)
TRAPHANDLER(trap6, 6)
TRAPHANDLER(trap7, 7)
TRAPHANDLER(trap8, 8)
TRAPHANDLER(trap9, 9)
TRAPHANDLER(trap10, 10)
TRAPHANDLER(trap11, 11)
TRAPHANDLER(trap12, 12)
TRAPHANDLER(trap13, 13)
TRAPHANDLER(trap14, 14)
TRAPHANDLER(trap15, 15)
TRAPHANDLER(trap16, 16)
TRAPHANDLER(trap17, 17)
TRAPHANDLER(trap18, 18)
TRAPHANDLER(trap19, 19)
TRAPHANDLER(trap20, 20)
TRAPHANDLER(trap21, 21)
TRAPHANDLER(trap22, 22)
TRAPHANDLER(trap23, 23)
TRAPHANDLER(trap24, 24)
TRAPHANDLER(trap25, 25)
TRAPHANDLER(trap26, 26)
TRAPHANDLER(trap27, 27)
TRAPHANDLER(trap28, 28)
TRAPHANDLER(trap29, 29)
TRAPHANDLER(trap30, 30)
TRAPHANDLER(trap31, 31)
TRAPHANDLER(trap32, 32)

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
*/

.globl vector0
vector0:
  pushl $0
  pushl $0
  jmp _alltraps
.globl vector1
vector1:
  pushl $0
  pushl $1
  jmp _alltraps
.globl vector2
vector2:
  pushl $0
  pushl $2
  jmp _alltraps
.globl vector3
vector3:
  pushl $0
  pushl $3
  jmp _alltraps
.globl vector4
vector4:
  pushl $0
  pushl $4
  jmp _alltraps
.globl vector5
vector5:
  pushl $0
  pushl $5
  jmp _alltraps
.globl vector6
vector6:
  pushl $0
  pushl $6
  jmp _alltraps
.globl vector7
vector7:
  pushl $0
  pushl $7
  jmp _alltraps
.globl vector8
vector8:
  pushl $8
  jmp _alltraps
.globl vector9
vector9:
  pushl $0
  pushl $9
  jmp _alltraps
.globl vector10
vector10:
  pushl $10
  jmp _alltraps
.globl vector11
vector11:
  pushl $11
  jmp _alltraps
.globl vector12
vector12:
  pushl $12
  jmp _alltraps
.globl vector13
vector13:
  pushl $13
  jmp _alltraps
.globl vector14
vector14:
  pushl $14
  jmp _alltraps
.globl vector15
vector15:
  pushl $0
  pushl $15
  jmp _alltraps
.globl vector16
vector16:
  pushl $0
  pushl $16
  jmp _alltraps
.globl vector17
vector17:
  pushl $17
  jmp _alltraps
.globl vector18
vector18:
  pushl $0
  pushl $18
  jmp _alltraps
.globl vector19
vector19:
  pushl $0
  pushl $19
  jmp _alltraps
.globl vector20
vector20:
  pushl $0
  pushl $20
  jmp _alltraps
.globl vector21
vector21:
  pushl $0
  pushl $21
  jmp _alltraps
.globl vector22
vector22:
  pushl $0
  pushl $22
  jmp _alltraps
.globl vector23
vector23:
  pushl $0
  pushl $23
  jmp _alltraps
.globl vector24
vector24:
  pushl $0
  pushl $24
  jmp _alltraps
.globl vector25
vector25:
  pushl $0
  pushl $25
  jmp _alltraps
.globl vector26
vector26:
  pushl $0
  pushl $26
  jmp _alltraps
.globl vector27
vector27:
  pushl $0
  pushl $27
  jmp _alltraps
.globl vector28
vector28:
  pushl $0
  pushl $28
  jmp _alltraps
.globl vector29
vector29:
  pushl $0
  pushl $29
  jmp _alltraps
.globl vector30
vector30:
  pushl $0
  pushl $30
  jmp _alltraps
.globl vector31
vector31:
  pushl $0
  pushl $31
  jmp _alltraps
.globl vector32
vector32:
  pushl $0
  pushl $32
  jmp _alltraps



# vector table
.data
.globl vectors
vectors:
  .long vector0
  .long vector1
  .long vector2
  .long vector3
  .long vector4
  .long vector5
  .long vector6
  .long vector7
  .long vector8
  .long vector9
  .long vector10
  .long vector11
  .long vector12
  .long vector13
  .long vector14
  .long vector15
  .long vector16
  .long vector17
  .long vector18
  .long vector19
  .long vector20
  .long vector21
  .long vector22
  .long vector23
  .long vector24
  .long vector25
  .long vector26
  .long vector27
  .long vector28
  .long vector29
  .long vector30
  .long vector31
  .long vector32


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

    movw $(CPU_GDT_KDATA<<3), %ax
    movw %ax, %ds
    movw %ax, %es
    movw $(CPU_GDT_KCODE<<3), %ax
    movw %ax, %fs
    movw %ax, %gs 
   
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
  movl %eax, %esp  // Move the argument (the trapframe * to the stack pointer)
  popal
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $0x8, %esp 
  iret

1:	jmp	1b		// just spin

