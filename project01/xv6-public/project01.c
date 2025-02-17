#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	char *buf = "My student id is 2022028522";
	int pid, gpid;
	pid = getpid();
	gpid = getgpid(buf);
	printf(1, "My pid is %x\n", pid);
	printf(1, "My gpid is %x\n", gpid);
	exit();
}
