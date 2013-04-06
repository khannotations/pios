/*
 * Kernel initialization.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/cdefs.h>
#include <inc/elf.h>
#include <inc/vm.h>

#include <kern/init.h>
#include <kern/cons.h>
#include <kern/debug.h>
#include <kern/mem.h>
#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/spinlock.h>
#include <kern/mp.h>
#include <kern/proc.h>
#include <kern/file.h>

#include <dev/pic.h>
#include <dev/lapic.h>
#include <dev/ioapic.h>


// User-mode stack for user(), below, to run on.
static char gcc_aligned(16) user_stack[PAGESIZE];

// Lab 3: ELF executable containing root process, linked into the kernel
#ifndef ROOTEXE_START
#define ROOTEXE_START _binary_obj_user_testfs_start
#endif
extern char ROOTEXE_START[];


// Called first from entry.S on the bootstrap processor,
// and later from boot/bootother.S on all other processors.
// As a rule, "init" functions in PIOS are called once on EACH processor.
void
init(void)
{
	extern char start[], edata[], end[];

	// Before anything else, complete the ELF loading process.
	// Clear all uninitialized global data (BSS) in our program,
	// ensuring that all static/global variables start out zero.
	if (cpu_onboot())
		memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	// Lab 1: test cprintf and debug_trace
	cprintf("1234 decimal is %o octal!\n", 1234);
   	unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);
	debug_check();

	// Initialize and load the bootstrap CPU's GDT, TSS, and IDT.
	cpu_init();
	trap_init();

	// Physical memory detection/initialization.
	// Can't call mem_alloc until after we do this!
	mem_init();

	// Lab 2: check spinlock implementation
	if (cpu_onboot())
		spinlock_check();

	// Initialize the paged virtual memory system.
	pmap_init();

	// Find and start other processors in a multiprocessor system
	mp_init();		// Find info about processors in system
	pic_init();		// setup the legacy PIC (mainly to disable it)
	ioapic_init();		// prepare to handle external device interrupts
	lapic_init();		// setup this CPU's local APIC
	cpu_bootothers();	// Get other processors started
    cprintf("CPU %d (%s) has booted\n", cpu_cur()->id,
		cpu_onboot() ? "BP" : "AP");

	// Initialize the I/O system.
	file_init();		// Create root directory and console I/O files
	// Lab 4: uncomment this when you can handle IRQ_SERIAL and IRQ_KBD.
	cons_intenable();	// Let the console start producing interrupts

	// Initialize the process management code.
	proc_init();

  if(!cpu_onboot())
    proc_sched();
  
  proc_root = proc_alloc(NULL, 0);
  elfhdr *elf = (elfhdr*)ROOTEXE_START;

	proghdr *prog = (proghdr*)((void*)elf + elf->e_phoff);
  uint32_t count = elf->e_phnum;
  int k;
	for (k = 0; k < count; k++) {
    int perms = PTE_P | PTE_U;
    if(prog->p_flags & ELF_PROG_FLAG_WRITE)
      perms |= PTE_W | SYS_WRITE | SYS_READ;
    else
      perms |= SYS_READ;

    void *off = (void*)elf + ROUNDDOWN(prog->p_offset, PAGESIZE);
		uint32_t start = ROUNDDOWN(prog->p_va, PAGESIZE);
		uint32_t end = ROUNDUP(prog->p_va + prog->p_memsz, PAGESIZE);
		while(start < end) {
			pageinfo *p = mem_alloc();
			if (start < ROUNDDOWN(prog->p_va + prog->p_filesz, PAGESIZE)) // complete page
				memmove(mem_pi2ptr(p), off, PAGESIZE);
			else {
				memset(mem_pi2ptr(p), 0, PAGESIZE);
        int remainder = (prog->p_va + prog->p_filesz) - start;
        if(remainder > 0)
          memmove(mem_pi2ptr(p), off, remainder);
			}
			pmap_insert(proc_root->pdir, p, start, perms);
      start += PAGESIZE;
      off += PAGESIZE;
		}
    prog++;
	}
  pageinfo *p = mem_alloc();
  pte_t *pte = pmap_insert(proc_root->pdir,
          p, VM_STACKHI - PAGESIZE, PTE_P | PTE_W | PTE_U |
          SYS_READ | SYS_WRITE);  // Nomimally read-write
  proc_root->sv.tf.eip = elf->e_entry;
  proc_root->sv.tf.esp = VM_STACKHI;
  proc_root->sv.tf.eflags |= FL_IF;
  // Initialize file system
  file_initroot(proc_root);
  proc_ready(proc_root);
  proc_sched();
}

// This is the first function that gets run in user mode (ring 3).
// It acts as PIOS's "root process",
// of which all other processes are descendants.
void
user()
{
	cprintf("in user()\n");
	assert(read_esp() > (uint32_t) &user_stack[0]);
	assert(read_esp() < (uint32_t) &user_stack[sizeof(user_stack)]);
	done();
}

// This is a function that we call when the kernel is "done" -
// it just puts the processor into an infinite loop.
// We make this a function so that we can set a breakpoints on it.
// Our grade scripts use this breakpoint to know when to stop QEMU.
void gcc_noreturn
done()
{
	while (1)
		;	// just spin
}

