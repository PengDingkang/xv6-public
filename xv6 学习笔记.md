# xv6 学习笔记

xv6 是一个完整的，运行在多核环境下的简单操作系统。

## Makefile 学习

`make` 命令执行时，需要一个 `makefile` 文件，以告诉 `make` 命令需要怎么样的去编译和链接程序。

### Makeflie 的书写规则

``` makefile
target ... : prerequisites ...
	command
	...
```

#### target
可以是一个 `object file` （目标文件），也可以是一个执行文件，还可以是一个标签 (`label`)。

#### prerequisites
生成该 `target` 所依赖的文件和/或 `target`。

#### command
该 `target` 要执行的命令（任意的 shell 命令）。
注意 `command` 需要以 `tab` 开头。

#### 其他书写注意事项

当依赖的最后更新日期比目标文件更新或目标文件不存在时，则 make 会执行其之后的命令。
注意，如果 `target` 的冒号之后没有指定依赖，那么 make 指令不会自动寻找依赖也不会自动执行之后的命令。
需要在 `make` 之后显式指出 `label` 名称，如 `make clean`。
这种特性可以允许我们定义一些编译无关的命令，如程序打包。
或者下面的 dist，产生发布软件包文件（即 distribution package）。这个命令将会将可执行文件及相关文件打包成一个 `tar.gz`压缩的文件用来作为发布软件的软件包。

``` makefile
dist:
	rm -rf dist
	mkdir dist
	for i in $(FILES); \
	do \
		grep -v PAGEBREAK $$i >dist/$$i; \
	done
	sed '/CUT HERE/,$$d' Makefile >dist/Makefile
	echo >dist/runoff.spec
	cp $(EXTRA) dist
```

定义一个变量，类似 C 语言的宏，便于生成对象的增删改。反斜杠为换行符。

``` makefile
OBJS = \
	bio.o\
	console.o\
	exec.o\
	file.o\
	fs.o\
	ide.o\
```

命令之前加 `-`，代表无视无法读取的文件继续执行 。

``` makefile
-include *.d
```

### make 的工作方式

1. 读入所有的`Makefile`。
2. 读入被 `include` 的其它 `Makefile`。
3. 初始化文件中的变量。
4. 推导隐晦规则，并分析所有规则。
5. 为所有的目标文件创建依赖关系链。
6. 根据依赖关系，决定哪些目标要重新生成。
7. 执行生成命令。

## 引导流程与内核初始化

在 `Makefile` 文件中，我们从第一个 `target` 开始看

``` makefile
xv6.img: bootblock kernel
	dd if=/dev/zero of=xv6.img count=10000
	dd if=bootblock of=xv6.img conv=notrunc
	dd if=kernel of=xv6.img seek=1 conv=notrunc
```

然后我们去寻找 `bootblock`, 开始启动引导流程

### 引导流程

``` makefile
bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock
```

从其依赖的源文件 `bootasm.S` 开始

从 `bootblock` 生成的一段中
可以找到这么一句

``` makefile
$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
```

这段说明 `bootblock` 的代码段加载到内存 `0x7C00` 处，代码从 `start` 处开始执行
我们再看 `start` 段

``` asm
.code16                       # 指代目前的运行模式：16 位实模式
.globl start
start:
  cli                         # 关中断指令

  xorw    %ax,%ax             # 将 ax 寄存器置零
  movw    %ax,%ds             # 接下来用 ax 的值去分别给以下 3 个寄存器置零 -> 数据段寄存器 Data Segment
  movw    %ax,%es             # -> 附加段寄存器 Extra Segment
  movw    %ax,%ss             # -> 堆栈段寄存器 Stack Segment
```

接下来要做的就是重新设置 A20 地址线并进入保护模式
由于 x86 CPU 初始运行时处于实模式状态, 只能使用 20 条地址线
要突破这个限制, 需要打开 A20 gate

``` asm
  # 通过键盘控制器端口打开 A20 地址
  # CPU 处于实模式时，只能使用 20 条地址线，因此需要重新设置地址线来使用全部地址线
seta20.1:
  inb     $0x64,%al               # IO 端口输入
  testb   $0x2,%al                # 测试键盘缓冲区是否有数据
  jnz     seta20.1                # 非零跳出，即等待键盘缓冲区为空

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64               # 向 804x 的控制器 P2 写数据

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60               # 将 0xdf 写入 P2 端口，就打开了 A20
```

然后就是进入保护模式
可以看到第一步是准备全局描述符表 GDT

``` asm
  lgdt    gdtdesc
```

这里 CPU 提供了一个 GDTR 寄存器用来保存 GDT 在内存中的位置和长度
共 48 位, 高 32 位代表其地址, 其余用于保存 GDT 的段描述符数量
此处用到了 `asm.h`

``` asm
gdt:
  SEG_NULLASM                             # 空
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # 代码段, STA_X 指代可执行, STA_R指代可读, 从 0x0 开始到 0xffffffff 结束
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # 数据段, STA_W 指代可写, 从 0x0 开始到 0xffffffff 结束

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # 16 位, 指示 长度
  .long   gdt                             # 32 位, 指示 GDT 地址
```

然后打开保护模式
此处用到了 `mmu.h` 中的 `CR0_PE`

``` c
#define CR0_PE          0x00000001      // Protection Enable
```

``` asm
  movl    %cr0, %eax       # 把 CR0 寄存器的值复制给 eax 寄存器
  # 将寄存器 cr0 的 PE 位置 1，以开启保护模式
  orl     $CR0_PE, %eax    
  movl    %eax, %cr0
```

打开保护模式后, 此时指令仍然是 16 位代码, 需要通过长跳转跳转至 32 位

``` asm
  ljmp    $(SEG_KCODE<<3), $start32
  
.code32  # Tell assembler to generate 32-bit code now.
```

进入保护模式运行, 做一些初始化

``` asm
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # 将 ax 寄存器赋值为段选择子 Our data segment selector
  movw    %ax, %ds                # 接下来用 ax 寄存器依次初始化数据段寄存器 -> DS: Data Segment
  movw    %ax, %es                # 扩展段寄存器 -> ES: Extra Segment
  movw    %ax, %ss                # 堆栈段寄存器 -> SS: Stack Segment
  movw    $0, %ax                 # ax 寄存器置零 Zero segments not ready for use
  movw    %ax, %fs                # 两个辅助寄存器置零 -> FS
  movw    %ax, %gs                # -> GS
```

然后就可以开始调用 `bootmain.c` 了

``` asm
  # Set up the stack pointer and call into C.
  # 初始化栈，并调用 C 函数
  movl    $start, %esp
  call    bootmain
```

首先我们知道 `bootasm.S` 将 CPU 转为 32 位保护模式后, `bootmain` 函数要做的事情就是将 `ELF` 格式的内核从硬盘中加载到内存里
我们不妨来看一下 `makefile` 中内核部分 `kernel`

### 内核初始化

``` makefile
kernel: $(OBJS) entry.o entryother initcode kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel entry.o $(OBJS) -b binary initcode entryother
	$(OBJDUMP) -S kernel > kernel.asm
	$(OBJDUMP) -t kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym
```

除去 `OBJS` 中的各种组件, 剩下的源文件有 3 个: `entryother.S` `initcode.S` `kernel.ld`
其中 `kernel.ld` 是链接内核二进制文件的桥梁, 负责生成 `ELF` 内核文件

``` asm
/*
OUTPUT_FORMAT 指定输出文件使用的三种格式, 分别指代默认和大小端
OUTPUT_ARCH 指定目标体系结构
ENTRY 是程序开始执行点, 这里可以看到开始点是 _start
*/
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
```

再回到 `bootmain` , 我们就可以大致清楚其运作机制

``` c
  elf = (struct elfhdr*)0x10000;  //指定 elf 内核文件头的物理地址
  void (*entry)(void);  //定义一个 entry 指针
  entry = (void(*)(void))(elf->entry);  //将 entry 指针定位到 elf 内核中所指的 entry 入口位置, 即 _start
  entry();  //开始执行 entry
```

在代码中寻找 `_start` 函数, 其位于 `entry.S` 中

``` asm
.globl _start
_start = V2P_WO(entry)
```

接下来执行的便是 `entry`, 在此之前, 我们先看一下 `V2P_WO` 这个宏, 其定义在 `memlayout.h` 中

``` c
#define KERNBASE 0x80000000         // First kernel virtual address
#define V2P_WO(x) ((x) - KERNBASE)    // same as V2P, but without casts
```

其作用便是将虚拟地址转换为物理地址
而在 `kernel.ld` 中

``` 		asm
	/*指定内核开始的虚拟地址*/
	. = 0x80100000;
	/*
	设置代码段定位器在 0x100000
	*/
	.text : AT(0x100000) {
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}
```

此时内核的加载地址在 `0x100000` , 虚拟地址在 `0x80100000` , 通过这个宏来完成转换

接下来进入 `entry` 部分, 开始运行内核代码

``` asm
# 开启分页机制
entry:
  # 开启 4MB 内存分页支持
  movl    %cr4, %eax
  # CR4_PSE 为 Page Size Extensions 位，将其置 1 以实现 4MBytes 页大小
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # 建立页表, 见下
  movl    $(V2P_WO(entrypgdir)), %eax
  # cr3 寄存器存放一级页表的地址
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  # 将 cr0 寄存器的 Write Protect 和 Paging 位置 1，开启内存分页机制
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0
```

关于 `entrypgdir` 部分见下

``` c
//将内核高位的虚拟地址映射到内核实际在的小端物理内存地址
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  //在物理内存 [0, 4MB) 建立一个页表, 对应虚拟内存 [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  //在物理内存 [0, 4MB) 建立一个页表, 对应虚拟内存 [0 + 0x80000000, 4MB + 0x80000000)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};
```

开启内存分页后, 再设置内核栈顶, 就能跳转进入 `main` 函数了

``` asm
  # esp 寄存器存放了一个指针指向系统栈最上面一个栈的栈顶
  movl $(stack + KSTACKSIZE), %esp

  # 跳转到 main 函数开始执行
  mov $main, %eax
  jmp *%eax

# 用 .comm 指令，在内核 bss 段定义一个 4096 大小的内核栈
.comm stack, KSTACKSIZE
```

### 总结

在从启动到引导进入 `main` 函数这段过程里, 主要经历了以下阶段

- 开机运行在实模式下, 内存寻址限制在 1MB
- 通过键盘控制器端口方式打开 A20 gate, 突破寻址限制
- 准备全局描述符表, 并进入保护模式
- 将内核从磁盘加载到内存
- 运行内核初始化, 设置页表并开始分页机制
- 跳转到 `main` 函数



## 自旋锁

xv6 中的自旋锁定义在 `spinlock.h` 中, 结构如下

```c
// Mutual exclusion lock.
struct spinlock {
  uint locked;       //锁的状态，0代表未上锁，1代表已上锁

  //调试用的参数
  char *name;        //锁名称
  struct cpu *cpu;   //持有锁的 CPU
  uint pcs[10];      //系统调用栈
};
```

接下来我们看 `spinlock.c` 中的实现过程

### 获得锁

```c
//请求获得锁
//循环（自旋）直至获得锁
//如果一个 CPU 占有锁时间过长，可能造成其他 CPU 等待时间过长
//因此不适合多线程抢占一个资源的场景
void
acquire(struct spinlock *lk)
{
  pushcli(); //关中断，避免死锁
```

首先是关中断

```c
void
pushcli(void)
{
  int eflags;

  //调用汇编指令将 eflag (program status and control) 寄存器状态值保存到 EFLAGS 堆栈，再将堆栈中的值给 eflasg 变量
  eflags = readeflags();
  //cli() 函数直接调用汇编关中断指令 cli
  cli();
```

然后检查 CPU 是否已经获得锁,

```c
  if(holding(lk)) //检查当前 CPU 是否已经获得该锁
    panic("acquire"); //若已获得则调用 exit();
```

接下来是核心操作, 调用内联汇编函数 `xchg` 改变上锁状态

```c
  //原子操作 xchg
  // xchgl 锁总线操作是非阻塞性的，因此不断尝试实现循环旋锁
  while(xchg(&lk->locked, 1) != 0)
    ;
```

```c
static inline uint
xchg(volatile uint *addr, uint newval)
{
  uint result;

  // The + in "+m" denotes a read-modify-write operand.
  // 交换两个变量，并返回第一个。操作具有原子性。
  asm volatile("lock; xchgl %0, %1" :
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc");
  return result;
}
```

由于加锁的过程是一个严格的时序依赖过程, 需要避免编译器和 CPU 使用乱序来对指令执行进行并行优化

``` c
  //内存屏障指令
  //保证 read-modify-write 顺序执行
  __sync_synchronize();
```

获得锁之后, 更新 Debug 信息

```c
  //获得锁之后更新 CPU 和调用栈信息
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
```

### 释放锁

释放锁的过程原理和获得锁一样

```c
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
  
  __sync_synchronize();

  //释放锁
  //使用一个内联汇编指令来完成以保证原子性
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  //开中断
  popcli();
}
```

其中开中断的过程和关中断类似

```c
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
```

### 总结

xv6 实现自旋锁的主要流程如下

- 