// Mutual exclusion lock.
struct spinlock {
  uint locked;       //����״̬��0����δ������1����������

  //�����õĲ���
  char *name;        //������
  struct cpu *cpu;   //�������� CPU
  uint pcs[10];      //ϵͳ����ջ
};

