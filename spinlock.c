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

//��������
//ѭ����������ֱ�������
//���һ�� CPU ռ����ʱ������������������ CPU �ȴ�ʱ�����
//��˲��ʺ϶��߳���ռһ����Դ�ĳ���
void
acquire(struct spinlock *lk)
{
  pushcli(); //���жϣ���������
  if (holding(lk)) //��鵱ǰ CPU �Ƿ��Ѿ���ø���
      panic("acquire"); //���ѻ������� exit();

  //ԭ�Ӳ��� xchg
  // xchgl �����߲����Ƿ������Եģ���˲��ϳ���ʵ��ѭ������
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  //�ڴ�����ָ��
  //��֤ read-modify-write ˳��ִ��
  __sync_synchronize();

  //�����֮����� CPU �͵���ջ��Ϣ
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

//�ͷ���
void
release(struct spinlock *lk)
{
    //��������ͬ�ķ�������Ƿ�����
  if(!holding(lk))
    panic("release");

  //��� CPU �͵���ջ��Ϣ
  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  //�ͷ���
  //ʹ��һ���������ָ��������Ա�֤ԭ����
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  //���ж�
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

//��鵱ǰ CPU �Ƿ��Ѿ���ø���
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();//���ж�
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

  //���û��ָ� eflag (program status and control) �Ĵ���״ֵ̬���浽 EFLAGS ��ջ���ٽ���ջ�е�ֵ�� eflasg ����
  eflags = readeflags();
  // cli() ����ֱ�ӵ��û����ж�ָ�� cli
  cli();
  // ��� CPU �� cli Ƕ�������Ƿ�Ϊ 0
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void
popcli(void)
{
  if(readeflags()&FL_IF)  //�����ջ�е� eflags �������жϳ���, ���ж��Ѿ���
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)  //��� cli Ƕ�ײ���С����, �����쳣�Ŀ��жϲ���
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)  //�� CPU ��Ƕ�ױ�־λ�� cli Ƕ�ײ�������ȷ, ����ÿ��ж�ָ��
    sti();
}

