# Linux Userspace Process Scheduler

## Introduction
This project contains the implementation of a round-robin scheduler for userspace processes. The process scheduler is supervised by a shell with a bunch of process monitoring commands. Two distinct priority levels are supported to satiate the scheduling needs of critical applications.

## Implementation
### Round-robin Scheduler
Each process is represented by a Process Control Block (PCB) and scheduler maintains a circular double-linked list of PCBs. This design option enables the scheduler to dispatch the process switch via circular list traversing.  Additionally, the double-linkage facilitates the removal of processes but tradeoffs memory space.  
  

Scheduler executes as a parent process and distributes the processing resources among its child processes. The signals SIGSTOP and SIGCONT are utilized by parent to monitor its children scheduling. Each process executes for a short interval and is removed by the process list in case of termination prior to its interval timeout. Scheduler operates on asynchronous manner and leverages SIGALRM and SIGCHLD to learn when a process terminates and when its time quantum timeouts.
![Alt text](https://i.imgur.com/iJblS51.png)

### Scheduler Monitoring via Command Interpreter (Shell)
A command interpreter is implemented to accept commands from the user and afterwards conveys the appropriate operation to process scheduler and awaits for its response. The interprocess communication is achieved via unix pipes.  
The following commands are supported by shell:
```
  p               ->  Prints the running processes along with their IDs, PIDs and name
  k <id>          ->  Terminates the process with the specified ID
  h <id>          ->  Upgrade the process to be of 'HIGH' priority
  l <id>          ->  Downgrade the process to be of 'LOW' priority
  e <prog-name>   ->  Creates a new process and schedules it accordingly
  q               ->  Terminates shell
```
![Alt text](https://i.imgur.com/P2CtNXZ.png)
### Priority Scheduling Schema
The scheduler suppors two distinct priorities 'LOW' and 'HIGH'. The scheduling occurs primarily on processes with 'HIGH' priority and the rest are ignored until no privileged processes participate in scheduling. This implementation is susceptible to starvation, though an aging mechanism could easily address this issue.

## Build
To build the entire project simply execute:
```
make
```
## Usage
The scheduler is initiated and schedules his arguments' processes accordingly. For interactive usage of the shell refer to the Scheduler Monitoring via Command Interpreter (Shell) section.
```
cd bin
./scheduler <prog1> <prog2> ... <progn>
```

## Todos
 - Implement an aging mechanism for priorities
 - Experiment with FIFO queues for priority implementation




