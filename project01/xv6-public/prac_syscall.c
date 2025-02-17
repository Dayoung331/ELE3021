#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

//Simple system call
int
getgpid(char *str)
{
	cprintf("%s\n", str);
	return myproc()->parent->parent->pid;
}

//Wrapper for my_syscall
int
sys_getgpid(void)
{
	char *str;
	//Decode argument using argstr
	if(argstr(0, &str) < 0)
		return -1;
	return getgpid(str);
}
