#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpu->id == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    lapiceoi();
    break;
   
   case T_DIVIDE:
      if (proc->handlers[SIGFPE] != (sighandler_t) -1) {
	siginfo_t siginfo;
	signal_deliver(SIGFPE, siginfo);
        break;
      }
  case T_PGFLT:
    if (proc->handlers[SIGSEGV] != (sighandler_t) -1) {
	cprintf("\ntrap enter\n");
	cprintf("eax %d\n",tf->eax);
	cprintf("eip %d\n",tf->eip);
	pte_t* addr = getpte(proc->pgdir,(void*)tf->eax,0);
	char* page;

	uint type;
	if(*addr & PTE_U && *addr & PTE_P) {
		cprintf("user and present flag are set\n");
		if(*addr & PTE_W) {
			cprintf("write flag is set\n");
			type = PROT_WRITE;
		}else{
			cprintf("write flag is not set\n");
			if(isRef((void*)addr)) {
				cprintf("isRef succeeds\n");
				type = PROT_READ;
     			}
			else {
				cprintf("isRef false\n");
				if((page = kalloc()) == 0) {
					proc->killed=1;
					return;
				}
				memmove(page, (char*)p2v(PTE_ADDR(*addr)), PGSIZE);
			        *addr = v2p(page);
				*addr = *addr | PTE_W | PTE_U | PTE_P;
				//mprotect((void*)tf->eax, PGSIZE, PROT_WRITE);
				
				deltRef(tf->eax, -1);
				deltRef(v2p(page),1);
			}
			lcr3(v2p(proc->pgdir));
		}
	}		
	else {
		cprintf("user or present flag is not set\n");
		type = PROT_NONE;
	}
	siginfo_t siginfo;
	siginfo.addr = tf->eax;
	cprintf("addr: %d\n", siginfo.addr);
	siginfo.type = type;
	cprintf("type: %d\n", siginfo.type);
	signal_deliver(SIGSEGV, siginfo);
	break;
    }

  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running 
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
