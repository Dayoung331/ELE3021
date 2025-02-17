#include "types.h"
#include "stat.h"
#include "user.h"

int initial_data = 0;

int
main(int argc, char *argv[])
{
  printf(1, "[Test 3] Make Copies\n");
  int pid;

  int parent_initial_fp = countfp();

  for(int i = 0; i < 10; i++){
    pid = fork();
    if(pid == 0){
      sleep(100 + 50 * i);
      int child_initial_fp = countfp();

      initial_data = 1;
  
      int child_modified_fp = countfp();

      printf(1, "child [%d]'s result: %d\n", i,child_initial_fp - child_modified_fp);
      if((child_initial_fp - child_modified_fp) == 0)
        printf(1, "[Test 3] fail\n\n");
      exit();
    }
  }

  for(int i = 0; i < 10; i++){
    wait();
  }

  if(parent_initial_fp - countfp() != 0)
    printf(1, "[Test 3] fail\n\n");
  else
    printf(1, "[Test 3] pass\n\n");

  exit();
}
