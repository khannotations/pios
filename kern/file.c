/*
 * Initial file system and file-based I/O support for the root process.
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/x86.h>
#include <inc/stat.h>
#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/dirent.h>

#include <inc/file.h>

#include <kern/cpu.h>
#include <kern/trap.h>
#include <kern/proc.h>
#include <kern/file.h>
#include <kern/init.h>
#include <kern/cons.h>


// Build a table of files to include in the initial file system.
#define INITFILE(name)	\
	extern char _binary_obj_user_##name##_start[]; \
	extern char _binary_obj_user_##name##_end[];
#include <obj/kern/initfiles.h>
#undef INITFILE

#define INITFILE(name)	\
	{ #name, _binary_obj_user_##name##_start, \
		_binary_obj_user_##name##_end },
char *initfiles[][3] = {
	#include <obj/kern/initfiles.h>
};
#undef INITFILE


// Although 'files' itself could be a preprocessor symbol like FILES,
// that makes this symbol invisible to and unusable under GDB,
// which is a bit of a pain for debugging purposes.
// This way 'files' is a real pointer variable that GDB knows about.
filestate *const files = FILES;

static spinlock file_lock;	// Lock to protect file I/O state
static size_t file_consout;	// Bytes written to console so far



void
file_init(void)
{
	if (!cpu_onboot())
		return;

	spinlock_init(&file_lock);
}

void
file_initroot(proc *root)
{
	// Only one root process may perform external I/O directly -
	// all other processes do I/O indirectly via the process hierarchy.
	assert(root == proc_root);

	// Make sure the root process's page directory is loaded,
	// so that we can write into the root process's file area directly.
	cpu_cur()->proc = root;
	lcr3(mem_phys(root->pdir));

	// Enable read/write access on the file metadata area
	cprintf("Calling pmap set perm\n");
	pmap_setperm(root->pdir, FILESVA, ROUNDUP(sizeof(filestate), PAGESIZE),
				SYS_READ | SYS_WRITE);
	memset(files, 0, sizeof(*files));

	// Set up the standard I/O descriptors for console I/O
	files->fd[0].ino = FILEINO_CONSIN;
	files->fd[0].flags = O_RDONLY;
	files->fd[1].ino = FILEINO_CONSOUT;
	files->fd[1].flags = O_WRONLY | O_APPEND;
	files->fd[2].ino = FILEINO_CONSOUT;
	files->fd[2].flags = O_WRONLY | O_APPEND;

	// Setup the inodes for the console I/O files and root directory
	strcpy(files->fi[FILEINO_CONSIN].de.d_name, "consin");
	strcpy(files->fi[FILEINO_CONSOUT].de.d_name, "consout");
	strcpy(files->fi[FILEINO_ROOTDIR].de.d_name, "/");
	files->fi[FILEINO_CONSIN].dino = FILEINO_ROOTDIR;
	files->fi[FILEINO_CONSOUT].dino = FILEINO_ROOTDIR;
	files->fi[FILEINO_ROOTDIR].dino = FILEINO_ROOTDIR;
	files->fi[FILEINO_CONSIN].mode = S_IFREG | S_IFPART;
	files->fi[FILEINO_CONSOUT].mode = S_IFREG;
	files->fi[FILEINO_ROOTDIR].mode = S_IFDIR;

	// Set the whole console input area to be read/write,
	// so we won't have to worry about perms in cons_io().
	pmap_setperm(root->pdir, (uintptr_t)FILEDATA(FILEINO_CONSIN),
				PTSIZE, SYS_READ | SYS_WRITE);

	// Set up the initial files in the root process's file system.
	// Some script magic in kern/Makefrag creates obj/kern/initfiles.h,
	// which gets included above (twice) to create the 'initfiles' array.
	// For each initial file numbered 0 <= i < ninitfiles,
	// initfiles[i][0] is a pointer to the filename string for that file,
	// initfiles[i][1] is a pointer to the start of the file's content, and
	// initfiles[i][2] is a pointer to the end of the file's content
	// (i.e., a pointer to the first byte after the file's last byte).
	int ninitfiles = sizeof(initfiles)/sizeof(initfiles[0]);
	// Lab 4: your file system initialization code here.
	int i;
	for(i=0; i<ninitfiles; i++) {
		int ino = i + FILEINO_GENERAL;
		int fsize = initfiles[i][2] - initfiles[i][1];
		// Need to set name, dino, mode, size, permissions, just like above.
		// We also need to copy the file data into the FILEDATA for that inode
		strcpy(files->fi[ino].de.d_name, initfiles[i][0]);
		files->fi[ino].dino = FILEINO_ROOTDIR;					// In the root directory
		files->fi[ino].mode = S_IFREG;									// Regular file
		files->fi[ino].size = fsize;										// The size, as calculated above.
		// Read - write permission for the system (we need to ROUNDUP because pmap_setperm)
		// expects sizes in the multiple of PTSIZE
		pmap_setperm(root->pdir, (uintptr_t)FILEDATA(ino), 
			ROUNDUP(fsize, PTSIZE), SYS_READ | SYS_WRITE);
		// Copy the data over
		memcpy(FILEDATA(ino), initfiles[i][1], fsize);
	}
	// warn("file_initroot: file system initialization not done\n");

	// Set root process's current working directory
	files->cwd = FILEINO_ROOTDIR;

	// Child process state - reserve PID 0 as a "scratch" child process.
	files->child[0].state = PROC_RESERVED;
}

// Called from proc_ret() when the root process "returns" -
// this function performs any new output the root process requested,
// or if it didn't request output, puts the root process to sleep
// waiting for input to arrive from some I/O device.
void
file_io(trapframe *tf)
{
	proc *cp = proc_cur();
	assert(cp == proc_root);	// only root process should do this!

	// Note that we don't need to bother protecting ourselves
	// against memory access traps while accessing user memory here,
	// because we consider the root process a special, "trusted" process:
	// the whole system goes down anyway if the root process goes haywire.
	// This is very different from handling system calls
	// on behalf of arbitrary processes that might be buggy or evil.

	// Perform I/O with whatever devices we have access to.
	bool iodone = 0;
	iodone |= cons_io();

	// Has the root process exited?
	if (files->exited) {
		cprintf("root process exited with status %d\n", files->status);
		done();
	}

	// We successfully did some I/O, let the root process run again.
	if (iodone)
		trap_return(tf);

	// No I/O ready - put the root process to sleep waiting for I/O.
	spinlock_acquire(&file_lock);
	cp->state = PROC_STOP;		// we're becoming stopped
	cp->runcpu = NULL;		// no longer running
	proc_save(cp, tf, 1);		// save process's state
	spinlock_release(&file_lock);

	proc_sched();			// go do something else
}

// Check to see if any input is available for the root process
// and if the root process is waiting for it, and if so, wake the process.
void
file_wakeroot(void)
{
	spinlock_acquire(&file_lock);
	if (proc_root && proc_root->state == PROC_STOP)
		proc_ready(proc_root);
	spinlock_release(&file_lock);
}

