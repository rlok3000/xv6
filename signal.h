#ifndef XV6_SIGNAL
#define XV6_SIGNAL

#define SIGKILL	0
#define SIGFPE	1
#define SIGSEGV 2

typedef void (*sighandler_t)(int);

typedef struct {
	uint type;
	uint addr;
} siginfo_t;

#define PROT_NONE 0x1
#define PROT_READ 0x5
#define PROT_WRITE 0x7

#endif
