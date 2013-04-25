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
#include <kern/net.h>

// This bit mask defines the eflags bits user code is allowed to set.
#define FL_USER		(FL_CF|FL_PF|FL_AF|FL_ZF|FL_SF|FL_DF|FL_OF)

extern uint8_t net_node;

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
// If not, abort the syscall by sending a T_PGFLT to the parent,
// again as if the user program's INT instruction was to blame.
//
// Note: Be careful that your arithmetic works correctly
// even if size is very large, e.g., if uva+size wraps around!
//
static void checkva(trapframe *utf, uint32_t uva, size_t size)
{
  uint32_t end = uva + size;
  // Need to check if uva > VM_USERHI because end might wrap
  // around (if uva = 0xfffffff for example)
  if(uva < VM_USERLO || uva > VM_USERHI || end > VM_USERHI)
    systrap(utf, T_PGFLT, 0);
}


// Copy data to/from user space,
// using checkva() above to validate the address range
// and using sysrecover() to recover from any traps during the copy.
void usercopy(trapframe *utf, bool copyout,
			void *kva, uint32_t uva, size_t size) {
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
  char tmp[CPUTS_MAX];
  usercopy(tf, 0, tmp, tf->regs.ebx, CPUTS_MAX);
  // Make sure it's null terminated (though it may be less than CPUTS_MAX long)
  //tmp[CPUTS_MAX] = 0;
	cprintf("%s", tmp);
	trap_return(tf);	// syscall completed
}

static void
do_put(trapframe *tf, uint32_t cmd)
{
	proc *curr = proc_cur();
  spinlock_acquire(&curr->lock);

  uint32_t child_index = tf->regs.edx;
  uint8_t node_number  = child_index >> 8 & 0xff;  // First 8 bits are the node number
  uint8_t child_number = child_index & 0xff;// The last 8 bits for child number

  // cprintf("node %d put: dest node: %d, child: %d, home node: %d\n", 
  //   net_node, node_number, child_number, RRNODE(curr->home));

  // When migrating, make sure to adjust eip! => entry == 0
  // Trying to migrate home and this is not its home
  if(node_number == 0) {
    node_number = RRNODE(curr->home);
  }
  if (net_node != node_number) {
    // cprintf("sys_put: %p migrating to %d\n", curr, RRNODE(curr->home));
    spinlock_release(&curr->lock);
    net_migrate(tf, node_number, 0);
  }

  proc *child = curr->child[child_number];

  if(!child) 
    child = proc_alloc(curr, child_number);
  if(child->state != PROC_STOP)
    proc_wait(curr, child, tf);
  
  spinlock_release(&curr->lock);

  // cprintf("do_put: current proc: %p, cpu_cur proc: %p\n", curr, cpu_cur()->proc);
	if(cmd & SYS_REGS) {
		usercopy(tf, 0, &child->sv, tf->regs.ebx, sizeof(procstate));
    child->sv.tf.ds = CPU_GDT_UDATA | 3;
		child->sv.tf.es = CPU_GDT_UDATA | 3;
		child->sv.tf.cs = CPU_GDT_UCODE | 3;
		child->sv.tf.ss = CPU_GDT_UDATA | 3;
		child->sv.tf.eflags &= FL_USER;
		child->sv.tf.eflags |= FL_IF;
  }
  uint32_t dest = tf->regs.edi; //syscall.h
  uint32_t size = tf->regs.ecx;
  uint32_t src = tf->regs.esi;

  if(cmd & SYS_MEMOP) {
    int op = cmd & SYS_MEMOP;
    // Check if the destination range is okay
    if(dest < VM_USERLO || dest > VM_USERHI || dest + size > VM_USERHI)
        systrap(tf, T_GPFLT, 0);
    if(op == SYS_COPY) {
      // we have to check the source too
      if(src < VM_USERLO || src > VM_USERHI || src + size > VM_USERHI)
          systrap(tf, T_GPFLT, 0);
      pmap_copy(curr->pdir, src, child->pdir, dest, size);
    } else
      pmap_remove(child->pdir, dest, size);
  }

	if(cmd & SYS_PERM)
		pmap_setperm(child->pdir, dest, size, cmd & SYS_RW);

	if(cmd & SYS_SNAP)
    // copy pdir to rpdir
    pmap_copy(child->pdir, VM_USERLO, child->rpdir, VM_USERLO, VM_USERHI-VM_USERLO);

	if(cmd & SYS_START)
		proc_ready(child);

	trap_return(tf);	// syscall completed
}

static void
do_get(trapframe *tf, uint32_t cmd)
{ 
  proc *curr = proc_cur();
  spinlock_acquire(&curr->lock);
  // Find child index (includes node number and child number)
  int child_index = tf->regs.edx;
  uint8_t node_number  = child_index >> 8;  // First 8 bits are the node number
  uint8_t child_number = child_index & 0xff;// The last 8 bits for child number

  // cprintf("node %d get: dest node: %d, child: %d, home node: %d\n", 
  //   net_node, node_number, child_number, RRNODE(curr->home));

  // When migrating, make sure to adjust eip! => entry == 0
  // Trying to migrate home and this is not its home
  if(node_number == 0) {
    node_number = RRNODE(curr->home);
  } 
  if (net_node != node_number) {
    // cprintf("sys_get: %p migrating to %d\n", curr, node_number);
    spinlock_release(&curr->lock);
    net_migrate(tf, node_number, 0);
  }

  proc *child = curr->child[child_number];
  if(!child)
    child = &proc_null;
    // cprintf("No child process %d\n", child_index);
  if(child->state != PROC_STOP)
	proc_wait(curr, child, tf);

  spinlock_release(&curr->lock);
  // cprintf("do_get: current proc: %p, cpu_cur proc: %p\n", curr, cpu_cur()->proc);
  uint32_t dest = tf->regs.edi; //syscall.h
  uint32_t size = tf->regs.ecx;
  uint32_t src = tf->regs.esi;

  if(cmd & SYS_MEMOP) {
    int op = cmd & SYS_MEMOP;
    // Check if the destination range is okay
    if(dest < VM_USERLO || dest > VM_USERHI || dest + size > VM_USERHI)
        systrap(tf, T_GPFLT, 0);
    if(op == SYS_COPY) {
      // we have to check the source too
      if(src < VM_USERLO || src > VM_USERHI || src + size > VM_USERHI)
          systrap(tf, T_GPFLT, 0);
      pmap_copy(child->pdir, src, curr->pdir, dest, size);
    } else if(op == SYS_MERGE) {
        pmap_merge(child->rpdir, child->pdir, src, curr->pdir, dest, size);
    } else
        pmap_remove(curr->pdir, dest, size);
  }

	if(cmd & SYS_PERM)
		pmap_setperm(curr->pdir, dest, size, cmd & SYS_RW);

    if(cmd & SYS_REGS)
		usercopy(tf, 1, &child->sv, tf->regs.ebx, sizeof(procstate));

	trap_return(tf);	// syscall completed
}

static void
do_ret(trapframe *tf, uint32_t cmd) {
  proc *curr = proc_cur();

  // cprintf("node %d return: home: %d\n", net_node, RRNODE(curr->home));
  // This is not this node's home
  if(RRNODE(curr->home) != net_node) {
    // cprintf("sys_ret: %p returning home to %d\n", curr, RRNODE(curr->home));
    // spinlock_release(&curr->lock);
    net_migrate(tf, RRNODE(curr->home), 0);
  }
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

