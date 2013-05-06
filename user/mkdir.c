/* 
 * PIOS shell command to create a directory
 * 
 */

#include <inc/stat.h>
#include <inc/errno.h>
#include <inc/stdio.h>
#include <inc/stdlib.h>
#include <inc/string.h>
#include <inc/dirent.h>
#include <inc/assert.h>
#include <inc/args.h>

#define usage() \
  fprintf(stderr, "mkdir: mkdir [-pv] dir\n"), \
  exit(EXIT_FAILURE)

int debug = 0;

int mkdir(const char *path, bool rec, bool verb) {
  if(!rec) {
    if(dir_walk(path, 0) == -1) {     // Error with dir_walk
      if(errno == ENOENT) {           // Directory doesn't exist, good to go!
        errno = 0;
        int ino = dir_walk(path, S_IFDIR);
        if(ino == -1) {
          printf("mkdir: couldn't create directory %s\n", path);
          return 0;
        }
        // Indicate that this has changed
        files->fi[ino].ver++;
        if(verb)
          printf("%s\n", path);
      } else {
        return 0; // Some other error, reflected by errno
      }
    } else {      // Directory already exists!
      printf("mkdir: %s already exists\n", path);
      errno = EINVAL;
      return 0;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  if(debug)
    cprintf("mkdir: start\n");
  if(argc < 2) {
    if(debug)
      cprintf("mkdir: not enough args (%d: %s)\n", argc, argv[0]);
    usage();
  }
  int i;
  bool recursive, verbose;
  recursive = verbose = false;
  int ret = 0;
  ARGBEGIN{
  default:
    if(debug)
      cprintf("mkdir: failing on flags\n");
    usage();
  case 'v':
    verbose = true;
    break;
  case 'p':
    recursive = true;
    break;
  }ARGEND

  if (argc == 1) {
    mkdir(argv[i], recursive, verbose);
  } else {
    if(debug)
      cprintf("mkdir: not enough args after flags\n");
    usage();
  }
  return errno;
}
