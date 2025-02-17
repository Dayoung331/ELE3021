#include "types.h"
#include "stat.h"
#include "user.h"

int initial_data = 0;

int
main(int argc, char *argv[])
{
  printf(1, "[Test 2] Make a Copy\n");
  int pid;

  pid = fork();
  if(pid == 0){
    int child_initial_fp = countfp();

    initial_data = 1;
  
    int child_modified_fp = countfp();

    if((child_initial_fp - child_modified_fp) == 1)
      printf(1, "[Test 2] pass\n\n");
    else
      printf(1, "[Test 2] fail\n\n");
  }
  else{
    wait();
  }

  exit();
}
