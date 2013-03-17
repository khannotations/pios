/*
 * System call handling.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the xv6 instructional operating system from MIT.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <inc/x86.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/trap.h>
#include <inc/syscall.h>

#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/syscall.h>





// This bit mask defines the eflags bits user code is allowed to set.
#define FL_USER		(FL_CF|FL_PF|FL_AF|FL_ZF|FL_SF|FL_DF|FL_OF)


// During a system call, generate a specific processor trap -
// as if the user code's INT 0x30 instruction had caused it -
// and reflect the trap to the parent process as with other traps.
static void gcc_noreturn
systrap(trapframe *utf, int trapno, int err)
{
    utf->trapno = trapno;
    utf->err = err;
    proc_ret(utf, 0);
}

// Recover from a trap that occurs during a copyin or copyout,
// by aborting the system call and reflecting the trap to the parent process,
// behaving as if the user program's INT instruction had caused the trap.
// This uses the 'recover' pointer in the current cpu struct,
// and invokes systrap() above to blame the trap on the user process.
//
// Notes:
// - Be sure the parent gets the correct trapno, err, and eip values.
// - Be sure to release any spinlocks you were holding during the copyin/out.
//
static void gcc_noreturn
sysrecover(trapframe *ktf, void *recoverdata)
{
    trapframe *utf = (trapframe*)recoverdata;
    cpu *c = cpu_cur();
    c->recover = NULL;
    systrap(utf, ktf->trapno, ktf->err);
}

// Check a user virtual address block for validity:
// i.e., make sure the complete area specified lies in
// the user address space between VM_USERLO and VM_USERHI.
// If not, abort the syscall by sending a T_GPFLT to the parent,
// again as if the user program's INT instruction was to blame.
//
// Note: Be careful that your arithmetic works correctly
// even if size is very large, e.g., if uva+size wraps around!
//
static void checkva(trapframe *utf, uint32_t uva, size_t size)
{
    uint32_t end = uva + size;
    if(uva < VM_USERLO || end > VM_USERHI)
        systrap(utf, T_PGFLT, 0);
}

// Copy data to/from user space,
// using checkva() above to validate the address range
// and using sysrecover() to recover from any traps during the copy.
void usercopy(trapframe *utf, bool copyout,
			void *kva, uint32_t uva, size_t size)
{
	checkva(utf, uva, size);
    cpu *c = cpu_cur();
    c->recover = sysrecover;

    if(copyout)
        memmove((void*)uva, kva, size);
    else
        memmove(kva, (void*)uva, size);

    c->recover = NULL;
}

static void
do_cputs(trapframe *tf, uint32_t cmd)
{
	// Print the string supplied by the user: pointer in EBX
  // Add one for null termination
  char tmp[CPUTS_MAX+1];
  usercopy(tf, 0, tmp, tf->regs.ebx, CPUTS_MAX);
  // Make sure it's null terminated (though it may be less than CPUTS_MAX long)
  tmp[CPUTS_MAX] = 0;
	cprintf("%s", tmp);
	trap_return(tf);	// syscall completed
}

static void
do_put(trapframe *tf, uint32_t cmd)
{
	proc *curr = proc_cur();
  spinlock_acquire(&curr->lock);

  uint32_t child_index = tf->regs.edx;
  proc *child = curr->child[child_index];

  if(!child) 
      child = proc_alloc(curr, child_index);

  if(child->state != PROC_STOP)
	proc_wait(curr, child, tf);
  
  spinlock_release(&curr->lock);

	if(cmd & SYS_REGS) {
		usercopy(tf, 0, &child->sv, tf->regs.ebx, sizeof(procstate));
        child->sv.tf.ds = CPU_GDT_UDATA | 3;
		child->sv.tf.es = CPU_GDT_UDATA | 3;
		child->sv.tf.cs = CPU_GDT_UCODE | 3;
		child->sv.tf.ss = CPU_GDT_UDATA | 3;
		child->sv.tf.eflags &= FL_USER;
		child->sv.tf.eflags |= FL_IF;
  }
  uint32_t source_va = tf->regs.esi; // 1073741824
  uint32_t dest_va = tf->regs.edi;   // 1073741824
  uint32_t copy_size = tf->regs.ecx; // 2952790016 
  switch(cmd & SYS_MEMOP){
    case 0:
      // No memop.
      break;
    case SYS_COPY:
      // We need to check:
      // 1. That source va, dest va and size are page aligned
      // 2. That the entire region falls between VM_USERLO AND VM_USERHI
      // cprintf("source: %d, dest: %d, size: %ld\n", source_va, dest_va, copy_size);
      // cprintf("VM_USERHI: %ld, VM_USERLO: %ld, offsets: %d, %d, %d\n", 
      //          VM_USERHI, VM_USERLO, PTOFF(source_va), PTOFF(dest_va), PTOFF(copy_size));
      if(PTOFF(source_va) || PTOFF(dest_va) || PTOFF(copy_size) 
        || source_va < VM_USERLO || source_va > VM_USERHI
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || source_va + copy_size > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
        // Invalid, so issue page fault as per instructions
        systrap(tf, T_PGFLT, 0);
      // If all good...
      pmap_copy(curr->pdir, source_va, child->pdir, dest_va, copy_size);
      break;
    case SYS_ZERO:
      // Here we only need to validate the destination
      if(PTOFF(dest_va) || PTOFF(copy_size) 
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
        // Invalid, so issue page fault as per instructions
        systrap(tf, T_PGFLT, 0);
      // All good!
      pmap_remove(child->pdir, dest_va, copy_size);
      break;
    case SYS_MERGE:
      // Do nothing...only available in GET
    default:
      // Invalid MEMOP flag...
      systrap(tf, T_PGFLT, 0);
  }

  if(cmd & SYS_PERM) {
    // Validate destination
    if(PTOFF(dest_va) || PTOFF(copy_size) 
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
      // Invalid, so issue page fault as per instructions
      systrap(tf, T_PGFLT, 0);
    // Get the right permission from cmd, since they correspond in SYS_RW
    pmap_setperm(curr->pdir, dest_va, copy_size, cmd & SYS_RW);
  }

  if(cmd & SYS_SNAP)
    // Copy entire user space from child's pdir to rpdir
    pmap_copy(child->pdir, VM_USERLO, child->rpdir, VM_USERLO, VM_USERHI - VM_USERLO);

	if(cmd & SYS_START)
		proc_ready(child);

	trap_return(tf);	// syscall completed
}

static void
do_get(trapframe *tf, uint32_t cmd)
{ 
  proc *curr = proc_cur();

  spinlock_acquire(&curr->lock);

  int child_index = tf->regs.edx;
  proc *child = curr->child[child_index];

  if(!child)
      cprintf("No child process %d\n", child_index);

  if(child->state != PROC_STOP)
	proc_wait(curr, child, tf);

  spinlock_release(&curr->lock);

  if(cmd & SYS_REGS)
	  usercopy(tf, 1, &child->sv, tf->regs.ebx, sizeof(procstate));

  uint32_t source_va = tf->regs.esi;
  uint32_t dest_va = tf->regs.edi;
  uint32_t copy_size = tf->regs.ecx;
  switch(cmd & SYS_MEMOP){
    case 0:
      break;
    case SYS_COPY:
      // We need to check:
      // 1. That source va, dest va and size are page aligned
      // 2. That the entire region falls between VM_USERLO AND VM_USERHI
      if(PTOFF(source_va) || PTOFF(dest_va) || PTOFF(copy_size) 
        || source_va < VM_USERLO || source_va > VM_USERHI
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || source_va + copy_size > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
        // Invalid, so issue page fault as per instructions
        systrap(tf, T_PGFLT, 0);
      // If all good...
      pmap_copy(curr->pdir, source_va, child->pdir, dest_va, copy_size);
      break;
    case SYS_ZERO:
      // Here we only need to validate the destination
      if(PTOFF(dest_va) || PTOFF(copy_size) 
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
        // Invalid, so issue page fault as per instructions
        systrap(tf, T_PGFLT, 0);
      // All good!
      pmap_remove(child->pdir, dest_va, copy_size);
      break;
    case SYS_MERGE:
      // Validate both
      if(PTOFF(source_va) || PTOFF(dest_va) || PTOFF(copy_size) 
        || source_va < VM_USERLO || source_va > VM_USERHI
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || source_va + copy_size > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
        // Invalid, so issue page fault as per instructions
        systrap(tf, T_PGFLT, 0);
      // Merge
      pmap_merge(child->rpdir, child->pdir, source_va, curr->pdir, dest_va, copy_size);
    default:
      // Invalid MEMOP flag...
      systrap(tf, T_PGFLT, 0);
  }

  if(cmd & SYS_PERM) {
    // Validate destination
    if(PTOFF(dest_va) || PTOFF(copy_size) 
        || dest_va < VM_USERLO || dest_va > VM_USERHI
        || dest_va + copy_size > VM_USERHI )
      // Invalid, so issue page fault as per instructions
      systrap(tf, T_PGFLT, 0);
    // Get the right permission from cmd, since they correspond in SYS_RW
    pmap_setperm(curr->pdir, dest_va, copy_size, cmd & SYS_RW);
  }

  if(cmd & SYS_SNAP)
    systrap(tf, T_PGFLT, 0); // Only available in PUT

	trap_return(tf);	// syscall completed
}

static void
do_ret(trapframe *tf, uint32_t cmd) {
    proc_ret(tf, 1);
}

// Common function to handle all system calls -
// decode the system call type and call an appropriate handler function.
// Be sure to handle undefined system calls appropriately.
void
syscall(trapframe *tf)
{
	// EAX register holds system call command/flags
	uint32_t cmd = tf->regs.eax;
	switch (cmd & SYS_TYPE) {
  	case SYS_CPUTS:	return do_cputs(tf, cmd);
  	case SYS_PUT: return do_put(tf, cmd);
  	case SYS_GET: return do_get(tf, cmd);
  	case SYS_RET: return do_ret(tf, cmd);
  	default:	return;		// handle as a regular trap
	}
}

