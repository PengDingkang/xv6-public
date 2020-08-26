// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

//请求获得锁
//循环（自旋）直至获得锁
//如果一个 CPU 占有锁时间过长，可能造成其他 CPU 等待时间过长
//因此不适合多线程抢占一个资源的场景
void
acquire(struct spinlock *lk)
{
  pushcli(); //关中断，避免死锁
  if (holding(lk)) //检查当前 CPU 是否已经获得该锁
      panic("acquire"); //若已获得则调用 exit();

  //原子操作 xchg
  // xchgl 锁总线操作是非阻塞性的，因此不断尝试实现循环旋锁
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  //内存屏障指令
  //保证 read-modify-write 顺序执行
  __sync_synchronize();

  //获得锁之后更新 CPU 和调用栈信息
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

//释放锁
void
release(struct spinlock *lk)
{
    //与获得锁相同的方法检查是否有锁
  if(!holding(lk))
    panic("release");

  //清空 CPU 和调用栈信息
  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  //释放锁
  //使用一个内联汇编指令来完成以保证原子性
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  //开中断
  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

//检查当前 CPU 是否已经获得该锁
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();//关中断
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void
pushcli(void)
{
  int eflags;

  //调用汇编指令将 eflag (program status and control) 寄存器状态值保存到 EFLAGS 堆栈，再将堆栈中的值给 eflasg 变量
  eflags = readeflags();
  // cli() 函数直接调用汇编关中断指令 cli
  cli();
  // 检查 CPU 的 cli 嵌套数量是否为 0
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void
popcli(void)
{
  if(readeflags()&FL_IF)  //如果堆栈中的 eflags 不等于中断常量, 则中断已经打开
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)  //如果 cli 嵌套层数小于零, 则有异常的开中断操作
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)  //当 CPU 的嵌套标志位和 cli 嵌套层数都正确, 则调用开中断指令
    sti();
}

