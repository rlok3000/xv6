#include "types.h"
#include "user.h"

int *p;

void handler(int signum, siginfo_t info)
{
	printf(1, "\n");
	printf(1,"Handler called, error address is 0x%x\n", info.addr);
	printf(1, "signum is %d\n", signum);
	printf(1, "info type is %d\n", info.type);
	if(info.type == PROT_READ)
	{
		printf(1,"ERROR: Writing to a page with insufficient permission.\n");
		mprotect((void *) info.addr, sizeof(int), PROT_READ | PROT_WRITE);
	}
	else
	{
		printf(1, "ERROR: Didn't get proper exception, this should not happen.\n");
		exit();
	}
} 
int main(void)
{
	signal(SIGSEGV, handler);
 	p = (int *) sbrk(1);
	printf(1, "p pure: %d\n", p);
	printf(1, "p void ptr: %d\n", (void *)p);
	printf(1, "p deref: %d\n", *p);
	printf(1, "\n");
 	mprotect((void *)p, sizeof(int), PROT_READ);
	*p=100;
 	printf(1, "COMPLETED: value is %d, expecting 100!\n", *p);
 	
 	exit();
}
