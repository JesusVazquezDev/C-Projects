#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "pstat.h"

// This syscall is used to set the number of tickets alloted to a process.
// It returns 0 if the pid value is valid and n_tickets is positive.
// Else, it returns -1.
int sys_settickets(void){
    int pid;
    int n_tickets;
    // Is number of tickets positive? If not, return -1
    if (argint(1,&n_tickets) < 0) {
        return -1;
    }
    if (argint(0,&pid) < 0) {
        return -1;
    }
    // Number of tickets is valid. PID is not validated going into func.
    return  settickets(pid,n_tickets);
}

// This sets the rseed variable defined in proc.c.
void sys_srand(void){
    uint seed = 0;
    if (argint(0,(int*)&seed) < 0) {
        // doesn't matter here... i think..
    }
    srand(seed);
}

// Extracts useful information from/for each process.
// This system call returns 0 on success and -1 on failure.
// (e.g., the argument is not a valid pointer).
// If successful, some basic information about each running process will be returned:
int sys_getpinfo (void) {
    struct pstat* stat;
    if(argptr(0,(void*)&stat, sizeof(*stat)) < 0){
        return -1;
    }
    return getpinfo(stat);
}



int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    //cprintf("[sys_sleep] Process: [%s] PID [%d] called sleep.\n", myproc()->name, myproc()->pid);
    myproc()->ticks = n;
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

