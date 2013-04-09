/*
 * PIOS user-space implementation of Unix-style executable loading (exec).
 *
 * Copyright (C) 2010 Yale University.
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Primary author: Bryan Ford
 */

#include <inc/cdefs.h>
#include <inc/mmu.h>
#include <inc/assert.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/dirent.h>
#include <inc/unistd.h>
#include <inc/elf.h>
#include <inc/vm.h>


// Maximum size of executable image we can load -
// must fit in our scratch area for loading purposes.
#define EXEMAX  MIN(VM_SHAREHI-VM_SHARELO,VM_SCRATCHHI-VM_SCRATCHLO)

extern void start(void);
extern void exec_start(intptr_t esp) gcc_noreturn;

int exec_readelf(const char *path);
intptr_t exec_copyargs(char *const argv[]);

int
execl(const char *path, const char *arg0, ...)
{
  return execv(path, (char *const *) &arg0);
}

int
execv(const char *path, char *const argv[])
{
  // We'll build the new program in child 0,
  // which never represents a forked child since 0 is an invalid pid.
  // First clear out the new program's entire address space.
  sys_put(SYS_ZERO, 0, NULL, NULL, (void*)VM_USERLO, VM_USERHI-VM_USERLO);

  // Load the ELF executable into child 0.
  if (exec_readelf(path) < 0)
    return -1;

  // Setup child 0's stack with the argument array.
  intptr_t esp = exec_copyargs(argv);

  // Copy our Unix file system and process state into the child.
  sys_put(SYS_COPY, 0, NULL, (void*)VM_FILELO, (void*)VM_FILELO,
    VM_FILEHI-VM_FILELO);

  // Copy child 0's entire memory state onto ours
  // and start the new program.  See lib/entry.S for details.
  exec_start(esp);
}

int
exec_readelf(const char *path)
{
  // We'll load the ELF image into a scratch area in our address space.
  sys_get(SYS_ZERO, 0, NULL, NULL, (void*)VM_SCRATCHLO, EXEMAX);

  // Open the ELF image to load.
  filedesc *fd = filedesc_open(NULL, path, O_RDONLY, 0);
  if (fd == NULL)
    return -1;
  void *imgdata = FILEDATA(fd->ino);
  size_t imgsize = files->fi[fd->ino].size;

  // Make sure it looks like an ELF image.
  elfhdr *eh = imgdata;
  if (imgsize < sizeof(*eh) || eh->e_magic != ELF_MAGIC) {
    warn("exec_readelf: ELF header not found");
    goto err;
  }

  // Load each program segment into the scratch area
  proghdr *ph = imgdata + eh->e_phoff;
  proghdr *eph = ph + eh->e_phnum;
  if (imgsize < (void*)eph - imgdata) {
    warn("exec_readelf: ELF program header truncated");
    goto err;
  }
  for (; ph < eph; ph++) {
    if (ph->p_type != ELF_PROG_LOAD)
      continue;

    // The executable should fit in the first 4MB of user space.
    intptr_t valo = ph->p_va;
    intptr_t vahi = valo + ph->p_memsz;
    if (valo < VM_USERLO || valo > VM_USERLO+EXEMAX ||
        vahi < valo || vahi > VM_USERLO+EXEMAX) {
      warn("exec_readelf: executable image too large "
        "(%d bytes > %d max)", vahi-valo, EXEMAX);
      goto err;
    }

    // Map all pages the segment touches in our scratch region.
    // They've already been zeroed by the SYS_ZERO above.
    intptr_t scratchofs = VM_SCRATCHLO - VM_USERLO;
    intptr_t pagelo = ROUNDDOWN(valo, PAGESIZE);
    intptr_t pagehi = ROUNDUP(vahi, PAGESIZE);
    sys_get(SYS_PERM | SYS_READ | SYS_WRITE, 0, NULL, NULL,
      (void*)pagelo + scratchofs, pagehi - pagelo);

    // Initialize the file-loaded part of the ELF image.
    // (We could use copy-on-write if SYS_COPY
    // supports copying at arbitrary page boundaries.)
    intptr_t filelo = ph->p_offset;
    intptr_t filehi = filelo + ph->p_filesz;
    if (filelo < 0 || filelo > imgsize
        || filehi < filelo || filehi > imgsize) {
      warn("exec_readelf: loaded section out of bounds");
      goto err;
    }
    memcpy((void*)valo + scratchofs, imgdata + filelo,
      filehi - filelo);

    // Finally, remove write permissions on read-only segments.
    if (!(ph->p_flags & ELF_PROG_FLAG_WRITE))
      sys_get(SYS_PERM | SYS_READ, 0, NULL, NULL,
        (void*)pagelo + scratchofs, pagehi - pagelo);
  }

  // Copy the ELF image into its correct position in child 0.
  sys_put(SYS_COPY, 0, NULL, (void*)VM_SCRATCHLO,
    (void*)VM_USERLO, EXEMAX);

  // The new program should have the same entrypoint as we do!
  if (eh->e_entry != (intptr_t)start) {
    warn("exec_readelf: executable has a different start address");
    goto err;
  }

  filedesc_close(fd); // Done with the ELF file
  return 0;

err:
  filedesc_close(fd);
  return -1;
}

intptr_t
exec_copyargs(char *const argv[])
{
  // Give the process a nice big 4MB, zero-filled stack.
  sys_get(SYS_ZERO | SYS_PERM | SYS_READ | SYS_WRITE, 0, NULL,
    NULL, (void*)VM_SCRATCHLO, PTSIZE);

  // Rafi start

  // Eventually, all of SCRATCHLO to SCRATCHLO + PTSIZE will be copied to
  // STACKHI - PTSIZE to STACKHI. Therefore, we want to start our stack
  // at SCRATCHLO + PTSIZE
  intptr_t mv_esp = VM_SCRATCHLO + PTSIZE;
  // Keep track of different between mv_esp and esp (add offset)
  int offset = VM_STACKHI - (VM_SCRATCHLO + PTSIZE);

  // We need to copy the physical strings and store them somewhere.
  // It can't be below the stack because that'll be overwritten, so it must be
  // above the stack. For simplicity, we'll store the strings in reverse order:
  /*
                    <--- SCRATCHLO+PTSIZE
    |*argv[0] cont| char 
    |*argv[0]     | char [label 0]
    | ...         | char
    |*argv[1]     | char
    | ...         | char
    | ...         | char
    |*argv[argc-1]| char  (these will be copied over by the sys_put)
    | NULL        |
    |argv[argc-1] | char *
    | ...         | char *
    |argv[1]      | char * 
    |argv[0]      | char * (in this case address of label 0 + offset)
    |argv         | char **
    |argc         |
                    <--- mv_esp
  */
  // Calculate number of args.
  int argc;
  for(argc=0; argv[argc]; argc++)
    /* nothing */;
  // Copy the strings
  char *mv_argv[argc+1];                    // +1 for null termination
  int i;
  for(i=0; i<argc; i++) {
    int len = sizeof(char)*(strlen(argv[i])+1);
    // Make room for the string
    mv_esp -= len;
    // Copy string
    strncpy((char *)mv_esp, argv[i], len);
    // Store the pointer as it will be in the new stack
    mv_argv[i] = (char *)mv_esp + offset;
  }
  mv_argv[argc] = NULL;                     // Null terminate
  mv_esp = ROUNDDOWN(mv_esp, sizeof(int));  // Word-align mv_esp
  mv_esp -= sizeof(char*)*(argc + 1);       // Room to move in mv_argv
  strcpy((char *)mv_esp, (char *)mv_argv);  // Copy over arg pointers
  mv_esp -= sizeof(int);                    // Room for argv pointer
  *(int *)mv_esp = mv_esp + sizeof(int) + offset;   // Pointer to argv
  mv_esp -= sizeof(int);                    // Room for argc
  *(int *)mv_esp = argc;                    // Copy argc

  // Copy the stack into its correct position in child 0.
  sys_put(SYS_COPY, 0, NULL, (void*)VM_SCRATCHLO,
    (void*)VM_STACKHI-PTSIZE, PTSIZE);

  // We eventually return the esp that starts at STACKHI, so
  // add the offset
  return offset + mv_esp;
}

