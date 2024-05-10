Implementing User-level Memory Encryption in XV6 which by default is susceptible to a Rowhammer attack.
Project involved implementing page encryption, decryption, and handling page-faults. 
In addition, involved creating syscalls to derive statistics related to systems memory. 
The crux of the project was to allow the kernel to manage page encryption and decryption.

Reference: 
https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf ( XV6 - Page Tables, Traps and Interupts ) 

Files: 
vm.c (e.g., walkpgdir(), uva2ka(), and copyout()) 
trap.c
exec.c
proc.c
Makefile



