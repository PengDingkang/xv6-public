// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
/*
 为内核上下文切换保存寄存器。
 不需要保存所有的段寄存器(%cs等)，因为它们在不同的内核上下文中是不变的。
 不需要保存%eax, %ecx, %edx，因为x86的惯例是调用者已经保存了它们。
 上下文存储在它们所描述的栈的底部；栈指针是上下文的地址。
 上下文的布局与swtch.S中 "Switch stacks "注释处的栈布局一致。
 Switch并没有显式地保存eip，但它是在堆栈上的，并且allocproc()会对它进行操作。
*/
struct context {
  // edi 和 esi 是变址寄存器
  uint edi;
  uint esi;
  uint ebx;  // ebx 是基地址寄存器，在内存寻址时存放基地址
  uint ebp;  // ebp 基址指针寄存器，存放指针指向栈底
  uint eip;  // eip 寄存器存放 cpu 要读取指令的地址
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
