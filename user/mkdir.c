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
int strnchr(const char *s, int index, char c);

int mkdir(const char *path, bool verb) {
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
    errno = EINVAL;
    return -1;
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
    if(recursive) {
      int index = 1;
      while(1) {
        index = strnchr(argv[i], index, '/');
        if(index == -1)
          break;
        char small[index+1];
        strncpy(small, argv[i], index);
        small[index] = '\0';
        mkdir(small, verbose);
        index++;
      }
    }
    if (mkdir(argv[i], verbose) == -1) {
      printf("mkdir: %s already exists\n", argv[i]);
      return errno;
    }
  } else {
    if(debug)
      cprintf("mkdir: not enough args after flags\n");
    usage();
  }
  return errno;
}

// Returns the index of the first occurance of c, starting at index
// Or -1 if not found
int strnchr(const char *s, int index, char c) {
  if(index > strlen(s) || index < 0)
    return -1;
  char *start = (char *)(s+index);
  int count = 0;
  while (start[count] != c) {
    if (start[count++] == 0)
      return -1;
  }
  return count + index;
}

/*
strnchr tests (run separately)
assert(strnchr("/a/b/c", 1, '/') == 2);
assert(strnchr("/a/b/c", 2, '/') == 2);
assert(strnchr("/a/b/c", 3, '/') == 4);
assert(strnchr("/a/b/c", 5, '/') == -1);
assert(strnchr("/a/b/c", 10, '/') == -1);
assert(strnchr("/a/b/c", -10, '/') == -1);
*/