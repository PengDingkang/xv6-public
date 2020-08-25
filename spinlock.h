// Mutual exclusion lock.
struct spinlock {
  uint locked;       //锁的状态，0代表未上锁，1代表已上锁

  //调试用的参数
  char *name;        //锁名称
  struct cpu *cpu;   //持有锁的 CPU
  uint pcs[10];      //系统调用栈
};

