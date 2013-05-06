# PIOS
### Rafi Khan and Akshay Nathan, Final Project OS 422
#### Yale University, May 6, 2013

For our final project, we extended the capabilities of the basic shell `user/sh.c` provided to us. Specifically, our goals were to implement output append redirection, piping, linking, and directory navigation commands (`mkdir`, `cd`, `pwd`). 

## Start up

    ssh me@node.zoo.cs.yale.edu
    git clone https://github.com/rafi-khan/pios.git
    cd pios
    make qemu

This should run the tests and start the shell

## Directory navigation

The PIOS files system keeps a reference to the current directory via files->cwd. However this is simply a inode number, and doesn’t give the full path as we'd like through a typical `pwd`. For that to be useful though, we need a way to make directories. We've done that by doing a mkdir.c, which implements a lot of the UNIX mkdir. 

    mkdir os
But that's slow so let’s just go ahead and make lots of them  
    mkdir -pv os/is/awesome/yay

Verify:  
    ls -l

We've written a 