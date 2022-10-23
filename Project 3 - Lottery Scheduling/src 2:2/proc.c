#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

#define RAND_MAX 0x7fffffff
uint rseed = 0;

// https://rosettacode.org/wiki/Linear_congruential_generator
uint rand() {
    return rseed = (rseed * 1103515245 + 12345) & RAND_MAX;
}

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->tickets = 0;
  p->ticks = 0;
  p->runticks = 0;
  p->boostsleft = 0;
  p->actualRunTicks = 0;
 
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
  // Setting first process ticket to 1. 
  p->tickets = 1;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  
  // New processes take on parent tickets
  np -> tickets = curproc -> tickets;

 // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);
 
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  // Scheduling Event
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Total tickets iterates through the process table lookng for all RUNNABLE processes.
// Returns the sum of all RUNNALE processes tickets for lottery scheduling.
int totalTickets() {
    int total = 0;
   
    // Ptable lock is acquired already 
    for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        // Iterate through each RUNNABLE process and sum all respective tickets. 
	if(p->state != RUNNABLE){
           continue;
        }
	//If runnable process has boosts, increment totalTickets accordingly	
	if(p->boostsleft > 0){
           total += (2 * p -> tickets);
	}
	else{
	   total += p -> tickets;
	}

    }
    return total;
}

// returns a pointer to the lottery winner.
struct
proc* hold_lottery(int total_tickets) {
    //cprintf("Holding lottery.\n");
    //cprintf("Total tickets in lottery is [%d] \n", total_tickets);
    
    if (total_tickets <= 0) {
        cprintf("this function should only be called when at least 1 process is RUNNABLE");
        return 0;
    }

    uint random_number = rand();
    
    // Winning ticket is less than total number of tickets.
    uint winner_ticket_number = random_number % total_tickets;

    int ticketAmount = 0;
    int ticketIndex = 0;
    
    struct proc* p;
	
    // Track PID of all runnable processes
    // Checking if runnable via proc
    for(p = ptable.proc; p < &ptable.proc[NPROC];p++) {
           
	// Checking to see if process is RUNNABLE
        if(p -> state != RUNNABLE){
            continue;
        }

        // Extracting processes ticket amount
	ticketAmount = p -> tickets;

	// Process needs to be boosted if true. 
	if(p->boostsleft > 0) {
	   //cprintf("Boosting Process [%d]. Boost Remaining [%d] \n",p->pid,  p->boostsleft -1);

	   // Artificially increment processes tickets
	   ticketAmount = ticketAmount * 2;

	   // Decrement boostsleft
	   p->boostsleft -= 1;
	}
 
        // Corner case? 	
	if(ticketAmount == total_tickets){
           return p;
	}

	// TicketIndex keeps track of current ticket
	while(ticketAmount != 0){ 
            // Checks if ticket Index is winning ticket
	    //cprintf("WINNING NUMBER IS [%d]\n",(int)winner_ticket_number);
	    //cprintf("Process [%d] tickets is [%d]\n", p->pid, ticketAmount);
	    //cprintf("Total Tickets [%d]\n", total_tickets);
	    if(ticketIndex == (int)winner_ticket_number) {
               //cprintf("FOUND ticket matching winner_ticket_number\n");
               return p;
	    }
	    // Increment ticket Index
	    ticketIndex += 1;
	    // Decrements ticket amount
	    ticketAmount -= 1;
        }
    }

    // Should not return here.
    cprintf("[hold_lottery] implementation isn't correct. \n");
    return p;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc * winner = 0;
  struct cpu *c = mycpu();
  c->proc = 0;

  int total_tickets = 0;
    
    for(;;){
      // Enable interrupts on this processor.
      sti();
     
      // Acquire Page Table Lock
      acquire(&ptable.lock);
      
      // Need to find how many tickets are currently in the pool
      total_tickets = totalTickets();

      // if no processes are currently in queue
      if(total_tickets == 0){
	 release(&ptable.lock);
	 continue;
      }
      // Retrieves the process that should run next
      winner = hold_lottery(total_tickets);

      //cprintf("Winning Process ID: [%d]\n", winner->pid);

      // If winner is NULL, don't send to CPU
      if(winner == 0){ 
         // Release Page Table Lock
         release(&ptable.lock);
         continue;
      }
      
      // Tracking the amount of ticks this process has run for
      winner->actualRunTicks += 1;

      // Sets CPU process 
      c->proc = winner;
             
      // Perpares kernel-stack of process and loads process's Page Table.
      switchuvm(winner);
      
      // Sets the processes state to RUNNING
      winner->state = RUNNING;
	     
      // Saves current register context in old, then loads the register context from new.
      // In other words, gives control to new process.
      swtch(&(c->scheduler), winner->context);
	     
      // Process returned, switch back to kernel registers and page table 
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.

      // Setting CPU process to NULL again. 
      c->proc = 0;
       
      // Release Page Table Lock
      release(&ptable.lock);
    }
}

// This syscall is used to set the number of tickets alloted to a process.
// It returns 0 if the pid value is valid and n_tickets is positive.
// Else, it returns -1.
int settickets(int pid, int n_tickets){
    //cprintf("PID: [%d]\n", pid);	
    //cprintf("TICKETS: [%d]\n", n_tickets);	
    // Checking if n_tickets is valid
    if(n_tickets <= 0){
      return -1;
    }

    //Checking if pid is valid
    int valid = 0;
    acquire(&ptable.lock);
    for (struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        // Is provided pid in ptable?
        if (p->pid == pid) {
            // pid is valid. Lets set those tickets.
            p->tickets = n_tickets;
	    //cprintf("Successfully set [%d] tickets for PID: [%d]\n",p->tickets, p->pid);
            valid += 1; // Adding 1 to verify it only increments once
            break;
        }
        else{
            continue;
        }
    }
    release(&ptable.lock);
    
    // If PID is not in ptable
    if (valid != 1) {
        return -1;
    }
    // Pid is valid and n_tickets is positive.
    return 0;
}

// Possible solution
void srand(int seed){
    rseed = seed;
}

// Extracts useful information from/for each process.
// This system call returns 0 on success and -1 on failure.
// (e.g., the argument is not a valid pointer).
// If successful, some basic information about each running process will be returned:
int getpinfo (struct pstat* stat) {
    // How do I check if stat is a valid pointer?
    if(stat == 0){
        return -1;
    }

    int i = 0 ;
    acquire(&ptable.lock);
    for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC];p++) {
        if(p->pid > 0){
	   if (p->state == UNUSED) {
              stat->inuse[i] = 0;
           }
           // p -> state == USED 
           else {
              stat->inuse[i] = 1;
           }
              stat -> pid[i] = p -> pid;
	      // cprintf("PID: [%d] ", stat -> pid[i]);
              stat -> tickets[i] = p -> tickets;
	      //cprintf("TICKETS: [%d] ", stat -> tickets[i]);
              stat -> runticks[i] = p -> actualRunTicks;
	      //cprintf("RUNTICKS: [%d] ", stat -> runticks[i]);
              stat -> boostsleft[i] = p -> boostsleft;
	      //cprintf("BOOSTSLEFT: [%d] ", stat -> boostsleft[i]);

	      //cprintf("INUSE: [%d] \n", stat -> inuse[i]);
	}
	i += 1;
    }
    release(&ptable.lock);
    return 0;
}



// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
// Switches back scheduler to return from a process that had enough running.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  // Gives up the CPU from a running process.
  myproc()->state = RUNNABLE;
  //cprintf("Yielding CPU.\n");
  myproc()->runticks += 1;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  //cprintf("Scheduling fork?.\n");
  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  // run ticks tracks the amount of sleep ticks
  p->runticks += 1;

  // Scheduling event
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
     // checks every tick
     if(p->state == SLEEPING && p->chan == chan){
       // p->chan equal to wait channel
       // chan is the current amount of clock ticks. Always starts at 0 ( preliminary ) .
     
     	// if sleep was not explictly called ( or amount of sleep ticks are exhausted ) 
     	if(p->ticks <= 0){
	   p->state = RUNNABLE;
	   continue;
	}
        else{
	   // Process needs to continue to sleep
	   //cprintf("BOOSTING. PID [%d]. Ticks [%d]. \n", p->pid, p->ticks);
    	   p->boostsleft += 1;
	   // decrementing Sleep tick
    	   p->ticks -= 1;
	   p->runticks += 1;
	   continue;
       }
    }
    // Process is blocked.
    else if(p->state == SLEEPING && p->pid > 2){
	    p->boostsleft += 1;
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

