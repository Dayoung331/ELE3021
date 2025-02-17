#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define NUM_SLEEP 500

#define NUM_THREAD 8
#define MAX_LEVEL 5
int parent;

int fork_children()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
    if ((p = fork()) == 0)
    {
      sleep(10);
      return getpid();
    }
  return parent;
}


int fork_children2()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      sleep(10);
      return getpid();
    }
    else
    {
      int r = setpriority(p, i + 1);
      if (r < 0)
      {
        printf(1, "setpriority returned %d\n", r);
        exit();
      }
    }
  }
  return parent;
}

int fork_children3()
{
  int i,p;
  for(i=0;i<=NUM_THREAD;i++){
    if((p = fork()) == 0)
    {
      sleep(10);
      return getpid();
    }else{
      int r = 0;
      if(p % 2 == 1)
      {
        r = setmonopoly(p, 2022028522); // input your student number
        printf(1, "Number of processes in MoQ: %d\n",r);
      }
      if(r < 0)
      {
        printf(1, "setmonopoly returned %d\n", r);
        exit();
      }
    }
  }
  return parent;
}
void exit_children()
{
  if (getpid() != parent)
    exit();
  while (wait() != -1);
}

int main(int argc, char *argv[])
{
  int i, pid;
  int count[MAX_LEVEL] = {0};

  parent = getpid();

  printf(1,"MLFQ test start\n");

  printf(1, "[Test 1] default\n");
  pid = fork_children();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getlev();
      if (x < 0 || x > 3)
      {
	if(x != 99){
          printf(1, "Wrong level: %d\n", x);
          exit();
	} 
      }
      if(x == 99) count[4]++;
      else count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL - 1; i++)
      printf(1, "L%d: %d\n", i, count[i]);
    printf(1, "MoQ: %d\n", count[4]);
  }
  exit_children();
  printf(1, "[Test 1] finished\n");

  printf(1, "[Test 2] priorities\n");
  pid = fork_children2();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getlev();
      if (x < 0 || x > 3)
      {
	if(x != 99){
          printf(1, "Wrong level: %d\n", x);
          exit();
	}
      }
      if(x == 99) count[4]++;
      else count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL - 1; i++)
      printf(1, "L%d: %d\n", i, count[i]);
    printf(1, "MoQ: %d\n", count[4]);
  }
  exit_children();
  printf(1, "[Test 2] finished\n");
  
  printf(1, "[Test 3] sleep\n");
  pid = fork_children2();

  if (pid != parent)
  {
    for (i = 0; i < NUM_SLEEP; i++)
    {
      int x = getlev();
      if (x < 0 || x > 3)
      {
	if(x != 99){
          printf(1, "Wrong level: %d\n", x);
          exit();
	}
      }
      if(x == 99) count[4]++;
      else count[x]++;
      sleep(1);
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL - 1; i++)
      printf(1, "L%d: %d\n", i, count[i]);
    printf(1, "MoQ: %d\n", count[4]);
  }
  exit_children();
  printf(1, "[Test 3] finished\n");

  printf(1, "[Test 4] MoQ\n");
  pid = fork_children3();

  if(pid != parent)
  {
    if(pid == 36)
    {
      monopolize();
      exit();
    }
    for(i = 0; i < NUM_LOOP; i++)
    {
      int x = getlev();
      if(x < 0 || x > 3)
      {
        if(x != 99)
        {
	  printf(1, "Wrong level: %d\n", x);
	  exit();
	}
      }
      if(x == 99) count[4]++;
      else count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for(i = 0; i < MAX_LEVEL - 1; i++)
      printf(1, "L%d: %d\n",i,count[i]);
    printf(1, "MoQ: %d\n", count[i]);
  }
  exit_children();
  printf(1, "[Test 4] finished\n");

  exit();
}


