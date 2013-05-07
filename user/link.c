/* 
 * PIOS shell program to create a symbolic link 
 */

#include <inc/stdio.h>
#include <inc/dirent.h>
#include <inc/errno.h>
#include <inc/stdlib.h> 
#include <inc/cdefs.h>
#include <inc/unistd.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/args.h>
#include <inc/stat.h>
#include <inc/file.h>

#define usage() \
  fprintf(stderr, "link:link file target\n"), \
  exit(EXIT_FAILURE)

int main(int argc, char **argv) {
  if(argc != 3)
      usage();
  struct stat st;
  char *target = argv[2];
  if(stat(target, &st) < 0) {
    printf("Error: target does not exist or cannot be opened\n");
    exit(EXIT_FAILURE);
  }
  int flags = O_CREAT | O_RDWR | O_EXCL;
  int fd = open(argv[1] /*file*/, flags, S_IFSYML);
  // Write the target to the data, this should be a separate function in file.c for symlinks
  link(fd, target);
  filedesc *fde = files->fd + fd;
  return 0;
}
