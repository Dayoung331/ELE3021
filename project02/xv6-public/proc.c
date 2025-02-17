#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int monopolized = 0;      // 1이면 moq, 0이면 MLFQ

// L0, L1, L2, L3를 구현하기 위한 queue 구조체 선언
struct queue{
  int queue[NPROC + 1];        // 프로세스들의 pid를 저장하는 배열
  int timeQuantum;             // 큐의 time quantum
  int level;                   // 큐의 level(0, 1, 2, 3)
  int size;                    // 큐의 크기
  int head;
  int tail;
};

// 큐의 맨 끝에다가 프로세스 집어넣기
void enqueue(struct queue *q, int p){
  if(q->size == NPROC){
    // cprintf("ERROR: cannot enqueue because the queue is full!");
    return;
  }

  q->queue[q->tail] = p;
  q->tail = (q->tail + 1) % (NPROC + 1);
  q->size++;
}

// 해당하는 프로세스 큐에서 지우기
void dequeue(struct queue *q, int p){
  if(q->size == 0){
    // cprintf("ERROR: cannot dequeue because the queue is empty!");
  }

  for(int i = q->head; i != q->tail; i = (i+1) % (NPROC+1)){
    if(q->queue[i] == p){
      for(int j = i; j != q->tail; j = (j+1) % (NPROC+1)){
        q->queue[j] = q->queue[(j+1) % (NPROC+1)];
      }
      q->tail = (q->tail + NPROC) % (NPROC+1);
      q->size--;
      break;
    }
  }
}

// 구조체 queue를 저장해두는 배열. 차례대로 L0, L1, L2, L3, moq
struct queue queues[5];

// L1, L2, L3 큐 비우기
void clearQueues(){
  for(int i=1; i<4; i++){
    queues[i].size = 0;
    queues[i].head = 0;
    queues[i].tail = 0;
  }
}

void initQueues() {
  // mlfq init
  for(int i = 0; i < 4; i++) {
    queues[i].level = i;
    queues[i].head = 0;
    queues[i].tail = 0;
    queues[i].timeQuantum = (i + 1) * 2;
    queues[i].size = 0;
  }

  // moq init
  queues[4].level = 99;
  queues[4].head = 0;
  queues[4].tail = 0;
  queues[4].size = 0;
}

void priority_boosting(){
  // cprintf("priority boosting started.\n");

  if(monopolized){
    // cprintf("Moq mode\n");
    return;
  }

  clearQueues();

  for(int i = 0; i < NPROC; i++){
    if(ptable.proc[i].state == ZOMBIE){
      continue;
    }

    // L1, L2, L3에 있는 process들은 L0에 옮겨주기
    if(ptable.proc[i].queueLevel == 1 || ptable.proc[i].queueLevel == 2 || ptable.proc[i].queueLevel == 3){
      ptable.proc[i].queueLevel = 0;
      ptable.proc[i].tick = 0;
      enqueue(&queues[0], i);
    }
  }

  // cprintf("priority boosting ended.\n");
}

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

  p->priority = 0;
  p->queueLevel = 0;
  p->tick = 0;
  p->idx = p - ptable.proc;

  enqueue(&queues[0], p->idx);

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

int findNextProcIdx(){
  struct queue *q;
  struct proc *p;

  if(monopolized){
    q = &queues[4];
    int i;

    for(i = q->head; i != q->tail; i = (i+1) % (NPROC+1)){
      p = &ptable.proc[q->queue[i]];
      if(p->state == RUNNABLE){
        break;
      }
    }

    if(i == q->tail){
      return -1;
    }
    else{
      return q->queue[i];
    }
  }

  else{
    // L0, L1, L2
    for(int i=0; i<3; i++){
      q = &queues[i];
      for(int j = q->head; j != q->tail; j = (j+1) % (NPROC+1)){
        p = &ptable.proc[q->queue[j]];
        if(p->state == RUNNABLE){
          return q->queue[j];
        }
      }
    }

    // L3
    q = &queues[3];
    struct proc *next_p;
    int max_priority = -1;

    for(int i = q->head; i != q->tail; i = (i+1) % (NPROC+1)){
      p = &ptable.proc[q->queue[i]];
      if(p->state == RUNNABLE && max_priority < p->priority){
        max_priority = p->priority;
        next_p = p;
      }
    }

    if (max_priority != -1){
      return next_p->idx;
    }
    else{
      return -1;
    }
  }
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
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  int nextpidx;

  for(;;){
    sti();

    acquire(&ptable.lock);

    // monopolized
    if(monopolized){
      // cprintf("monopolized\n");
      nextpidx = findNextProcIdx();
      if(nextpidx == -1){
        unmonopolize();
        release(&ptable.lock);
        continue;
      }

      p = &ptable.proc[nextpidx];

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&c->scheduler, p->context);
      switchkvm();

      c->proc = 0;
    }

    // mlfq
    else{
      nextpidx = findNextProcIdx();
      if(nextpidx == -1){
        release(&ptable.lock);
        continue;
      }

      p = &ptable.proc[nextpidx];
      p->tick = 0;

      c->proc = p;

      switchuvm(p);
      p->state = RUNNING;
      swtch(&c->scheduler, p->context);
      switchkvm();

      c->proc = 0;

      // time out
      if(p->tick >= p->queueLevel * 2 + 2){
        dequeue(&queues[p->queueLevel], p->idx);
        // cprintf("moving queuelevel %d", p->queueLevel);

        if(p->queueLevel == 0 && p->pid % 2 == 1){
          p->queueLevel = 1;
        }
        else if(p->queueLevel == 0 && p->pid % 2 == 0){
          p->queueLevel = 2;
        }
        else if(p->queueLevel == 1 || p->queueLevel == 2){
          p->queueLevel = 3;
        }
        else if(p->queueLevel == 3){
          if(p->priority != 0){
            p->priority--;
          }
        }

        enqueue(&queues[p->queueLevel], p->idx);
        // cprintf("to queuelevel %d\n", p->queueLevel);
      }
      else{
        dequeue(&queues[p->queueLevel], p->idx);
        enqueue(&queues[p->queueLevel], p->idx);
      }
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  // cprintf("sched started.\n");
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
  // cprintf("sched ended.\n");
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;

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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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

int
getlev(void)
{
  struct proc *p = myproc();
  if(p == 0){
    return -1;
  }

  if(p->queueLevel >= 0 && p->queueLevel < 4){
    return p->queueLevel;
  }
  else{
    return 99;
  }
}

// Set process priority
int setpriority(int pid, int priority)
{
  int i;
  for(i = 0; i < NPROC; i++){
    if(ptable.proc[i].pid == pid){
      break;
    }
  }
  
  // 못 찾았아서 i가 NPROC이 됨.
  if (i == NPROC){
    return -1;
  }

  if (priority < 0 || priority > 10){
    return -2;
  }

  struct proc *p = &ptable.proc[i];
  p->priority = priority;

  return 0;
}


int
setmonopoly(int pid, int password)
{
  struct proc *p;

  for(p=ptable.proc; p<&ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      break;
    }
  }

  // 못 찾아서 p가 맨 마지막 프로세스가 됨.
  if(p == &ptable.proc[NPROC]){
    return -1;
  }

  if(password != 2022028522){
    return -2;
  }

  dequeue(&queues[p->queueLevel], p->idx);
  p->queueLevel = 99;
  enqueue(&queues[4], p->idx);

  return queues[4].size;
}

void
monopolize(void)
{
  monopolized = 1;
}

void
unmonopolize(void)
{
  monopolized = 0;
}