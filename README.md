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

We've written a `cd` that takes relative or absolute pathnames

    cd os
    ls 
    cd is
    cwd # => Courtesy of Prof. Ford, should return 'is'
    cd /os/is/awesome 
    cwd # => 'awesome'

But who’s to know where `awesome` is? We’d like the full path, which can be gotten from by tracing dinos on `files->cwd`. The string manipulation, however, is non-trivial and can be tricky. In the end, we have `pwd`:

    pwd # => '/os/is/awesome'

That’s pretty good. Now let’s get to appending. Normally, we can echo out to files, but we've added the standard `>>` append as well:

    echo hi > out
    cat out # => 'hi'
    echo hi >> out
    cat out # => 'hihi'

We would have added things like standard input, output and error redirection, but most of it was repetitive drudgery--plus we did all that in CS 323 anyway. Most of our efforts were devoted to doing command history, linking and pipe.

## Command history

#### *A quick note about using the arrow keys in QEMU*  
After `make qemu`, please make sure to not click on any windows (the bash console or the x11 console), as this makes the arrow key codes mess up. I've only gotten this error running on Mac OS, so if you see question marks or white blocks in the bash/x11 console, this may be why. Please recompile with `make qemu` and type the commands without clicking anywhere.

Our command history is a step above bash because bash keeps all history (not just distinct, non-consecutive commands) and doesn’t consider what’s already been typed when scrolling through command history. For example, if you type `ls` and press up, you’d expect only commands that begin with `ls` to show up. That’s exactly what our history does.

    type e, then press up 

Pressing up with `e` typed will skip the previous cats and go on to the echos
(you should see `echo hi >> out`, and if you press up again should see echo hi > outt)

You may notice that there are two t’s at the end of out in the bash console --  that’s a byproduct of bash not properly handling the \b character. If you look at the x11 console (cmd-tab over to x11 on Mac), however, you can see the history worked properly. 

Switch back to bash, and press enter: this does `cat hi > out`, which we can verify with 

    cat out # => should show just “hi”

For demonstration purposes, we have programmed the down key to show the entire history -- in practice they would obviously change this to scroll the other way through the previous commands. This is simply a matter of changing `++` to a `--` in `readline.c`.
If you enter the same command twice, and press down (press enter to return to the shell), you'll see that the history only includes the command once. This only works with consecutive commands. 

## Pipesfds
