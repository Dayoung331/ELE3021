#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int parent_fp, child_fp;
  int pid;

  printf(1, "[Test 1] initial sharing\n");
	
  parent_fp = countfp();

  pid = fork();
  if(pid == 0){
    child_fp = countfp();
    
    if(parent_fp - child_fp == 68)
      printf(1, "[Test 1] pass\n\n");
    else
      printf(1, "[Test 1] fail\n\n");
    
    exit();
  }
  else{
    wait();
  }

  exit();
}

