# List of changes made to add a new SYSCALL in XV6
#### Coming soon... 

# The objectives of this project are:

#### Gain comfort looking through more substantial code bases written by others in which you do not need to understand every line
#### Obtain familiarity with the xv6 code base in particular
#### Learn how to add a system call to xv6 
#### Add a user-level application that can be used within xv6
#### Become familiar with a few of the data structures in xv6 (e.g., process table)
#### Use the gdb debugger on xv6 

#### After you have obtained the source files, you can run make qemu-nox to compile all the code and run it using the QEMU emulator. Test out the unmodified code by running a few of the existing user-level applications, like ls  and forktest. With ls inside the emulator, you'll be able to see a few other applications that are available (as well as files that have been created within the xv6 environment).

#### To quit the emulator, type Ctl-a x.

####  You will want to be familiar with the Makefile and comfortable modifying it. In particular, see the list of existing UPROGS. See the different ways of running the environment through make (e.g., qemu-nox or qemu-nox-gdb). 

#### Find where the number of CPUS is set and change this to be 1.

# Debugging
#### Your first task is to demonstrate that you can use gdb to debug xv6 code. You should show the integer value that fdalloc() (in sysfile.c) returns the first time it is called after a process has been completely initialized.

#### To do this, you can follow these steps:

#### In one window, start up qemu-nox-gdb (using make). This will automatically create a .gdbinit file under your current working directory.
#### Open another window on the same machine and go to the same working directory. Traditionally gdb will automatically load the .gdbinit file under the current working directory and debug xv6 in QEMU. However, in recent versions, this feature is removed for security reasons (e.g. you do not want to load .gdbinit file left by someone else). You have two options to manually enable this:
#### echo "add-auto-load-safe-path $(pwd)/.gdbinit" >> ~/.gdbinit. This enables the autoloading of the .gdbinit in the current working directory.
#### echo "set auto-load safe-path /" >> ~/.gdbinit. This enables the autoloading of all .gdbinit.
#### Start up gdb and continue it until xv6 finishes its bootup process and gives you a prompt.
#### Now, interrupt (Ctrl+C) gdb and set a breakpoint in the fdalloc() routine.
#### Continue gdb, and run the stressfs user application at the xv6 prompt since this will cause fdalloc() to be called. Your gdb process should now have stopped in fdalloc.
#### step (or, probably next) through the C code until gdb reaches the point just before fdalloc() returns and print the value that will be returned (i.e., the value of fd).
#### Immediately quit gdb and run whoami to display your login name.
#### If gdb gives you the error message that fd has been optimized out and cannot be displayed, make sure that your Makefile uses the flag "-Og" instead of "-O2". Debugging is also a lot easier with a single CPU, so if you didn't do this already: in your Makefile find where the number of CPUS is set and change this to be 1.

#### Take a screenshot showing your gdb session with the returned value of fd printed and your login name displayed. Submit this screenshot.

# System Calls and User-Level Application
#### You will add a new system call named int getiocounts(struct iostat* iostat) to xv6. When it is called, it finds the numbers of times read() and write() are called by the current running process, and fills in the iostat struct pointed to by the parameter passed into this system call with these numbers . It should return 0 on success and -1 if the iostat pointer is invalid. In particular, struct iostat is defined as: 

#### struct iostat {
####     int readcount;  // total number of times read() is invoked by the current running process (including failed calls)
####     int writecount; // total number of times write() is invoked by the current running process (including failed calls)
#### }
#### So that you can test your system calls, you should also create a user-level application with the following behavior:

#### sysios m n. This program takes two arguments: m, which is the total number of read() that it should call, and n, which is the number of write() it should call. You can perform those I/O system calls in any order you choose and you can read() or write() to any file you like (or even pass in invalid parameters to make them fail intentionally). After this work has been done, it should print out three values: the value returned by getiocounts(struct iostat* iostat) and the two integer values in iostat indicating the number of times read() and write() are invoked. For example, if you run the program as "sysios 5 6" the output should be exactly "0 5 6\n". We will not test it with invalid arguments. 
#### You must use the names of the system call and the application exactly as specified! 

# Note
#### In xv6, functions that output data to console (e.g., printf()) internally calls write(). Be careful when you are implementing sysios.

# Implementation Hints and Details
#### The primary files you will want to examine in detail include syscall.c, proc.c, proc.h, and sysfile.c. You might also want to take a look at usys.S, which (unfortunately) is written in assembly.

#### To add a system call, find some other very simple system call like sys_fstat that also takes a pointer parameter, copy it in all the ways you think are needed, and modify it so it doesn't do anything and has the new name. Compile the code to see if you found everything you need to copy and change. You probably won't find everything the first time you try.

#### Then think about the changes that you will need to make so your system call act like itself.

#### How to find the current running process? Many other system calls also need to figure out which process is the current running process. An easy one to look at is sys_getpid.
#### How will you track the number of read() and write() made by the current running process? Since xv6 performs context switch for fair scheduling, this means you need to keep two counters for each individual process, one for read() and one for write(). These two counters will be added to a structure that already exists for each process, and getiocounts() simply fills these two counters into iostat. 
#### Remember to zero-initialize the counters when a process is created. Check out proc.c to see when and how other fields (e.g., pid) are initialized for each process. 
#### The reason getiocounts() takes a pointer to a struct iostat instead of returning a pointer to struct iostat is that memory in kernel space cannot be accessed directly from userspace. Hence the user is responsible for memory allocation and kernel can fill in the allocated memory with meaningful data.
#### Make sure you increment the appropriate counter at the beginning of read() and write() so that it also counts the failed calls.
#### Remember that you will need to include struct iostat in both kernel space and userspace. A good example you can look at is struct stat used by fstat().
#### For this project, you do not need to worry about concurrency and locking (think about why). Concurrency means running and managing multiple processes at the same time, and locking provides isolation so that certain resources are only accessed by the process holding the lock. You do not need to fully understand these as we will discuss concurrency and locking later in the semester.

#### To create the user-level application sysios we again suggest copying one of the straight-forward utilities that already exist. If you cannot find your user-level application inside xv6, one possible reason is that your application is not compiled into xv6. One easy way to check that is to write some invalid code and see if xv6 still compiles. 
