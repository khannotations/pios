/* 
 * PIOS shell command to print current working directory
 * 
 */

#include <inc/stdio.h>
#include <inc/dirent.h>
#include <inc/errno.h>
#include <inc/stdlib.h> 

int main(int argc, char **argv) {
  if(argc > 1) {
    fprintf(stderr, "usage: pwd\n");
    exit(EXIT_FAILURE);
  }
  fileinode fi = files->fi[files->cwd];
  printf("%s\n", fi.de.d_name);

  return 0;
}
