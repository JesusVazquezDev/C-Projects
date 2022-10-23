# Assignment Context

### The current xv6 scheduler implements a very simple Round Robin (RR) policy.
### For example, if there are three processes A, B and C, then the xv6 round-robin
### scheduler will run the jobs in the order  A B C A B C … , where each letter 
### represents a process.  

### The xv6 scheduler runs each process for at most one timer tick (10 ms); 
### after each timer tick, the scheduler moves the previous job to the end of the
### ready queue and dispatches the next job in the list.

### The xv6 scheduler does not do anything special when a process sleeps or blocks
### (and is unable to be scheduled); the blocked job is simply skipped until it is
### ready and it is again its turn in the RR order. 

### Since RR treats all jobs equally, it is simple and fairly effective. 
### However, there are instances where this "ideal" property of RR is detrimental:
### if compute-intensive background jobs (anti-virus checks) are given equal priority
### to latency-sensitive operations, operations like music streaming (latency-sensitive)
### suffer. Lottery Scheduling is a proportional share policy which fixes this by asking
### users to grant tickets to processes. The scheduler holds a lottery every tick to 
### determine the next process. Since important processes are given more tickets, they 
### end up running more often on the CPU over a period of time.

# Objectives

### Understand existing code for performing context-switches in the xv6 kernel.
### Implement a basic lottery scheduler that boosts the priority of well-behaving processes.
### Modify how sleep is implemented in xv6 to match our scheduler. 
### Implement system calls that set/extract process state.
### Use the provided user-level programs to empirically validate the fair and proportional share properties of a lottery scheduler.
### Gain familiarity in working on a large codebase in a group.