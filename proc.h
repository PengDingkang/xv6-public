// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC �Ĵ��� ID������Ĵ������� local �� external �жϵĲ��������ͺͽ��յ�
  struct context *scheduler;   // ����������ָ�룬���� swtch() �������ý��н����л�
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // �ж�Ƕ�ײ���
  int intena;                  // �� pushcli ����Ƿ��Ѿ��������ж�
  struct proc *proc;           // �ڵ�ǰ CPU �����еĽ���
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
 Ϊ�ں��������л�����Ĵ�����
 ����Ҫ�������еĶμĴ���(%cs��)����Ϊ�����ڲ�ͬ���ں����������ǲ���ġ�
 ����Ҫ����%eax, %ecx, %edx����Ϊx86�Ĺ����ǵ������Ѿ����������ǡ�
 �����Ĵ洢��������������ջ�ĵײ���ջָ���������ĵĵ�ַ��
 �����ĵĲ�����swtch.S�� "Switch stacks "ע�ʹ���ջ����һ�¡�
 Switch��û����ʽ�ر���eip���������ڶ�ջ�ϵģ�����allocproc()��������в�����
*/
struct context {
  // edi �� esi �Ǳ�ַ�Ĵ���
  uint edi;
  uint esi;
  uint ebx;  // ebx �ǻ���ַ�Ĵ��������ڴ�Ѱַʱ��Ż���ַ
  uint ebp;  // ebp ��ַָ��Ĵ��������ָ��ָ��ջ��
  uint eip;  // eip �Ĵ������ cpu Ҫ��ȡָ��ĵ�ַ
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };  //���̵�����״̬

// Per-process state
struct proc {
  uint sz;                     // ����ռ�õ��ڴ��С (bytes)
  pde_t* pgdir;                // ҳ��
  char *kstack;                // �������ں�ջ�еײ���λ��
  enum procstate state;        // ����״̬
  int pid;                     // ���� PID
  struct proc *parent;         // ������
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // ����������ָ�룬���� swtch() �������ý��н����л�
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
