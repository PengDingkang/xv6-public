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

## 从 xv6.img 开始

在 `Makefile` 文件中，我们从第一个 `target` 开始看

``` makefile
xv6.img: bootblock kernel
	dd if=/dev/zero of=xv6.img count=10000
	dd if=bootblock of=xv6.img conv=notrunc
	dd if=kernel of=xv6.img seek=1 conv=notrunc
```

然后我们再去寻找 `bootblock`

### bootblock

``` makefile
bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJDUMP) -S bootblock.o > bootblock.asm
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock
```

再从其依赖的源文件 bootasm.S 开始

#### bootasm.S

从 `bootblock` 生成的一段中
可以找到这么一句

``` makefile
$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
```

这段说明 `bootblock` 的代码段加载到内存 `0x7C00` 处，代码从 `start` 处开始执行
我们再看 `start` 段

``` makefile
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

``` makefile
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

``` makefile
  lgdt    gdtdesc
```

这里 CPU 提供了一个 GDTR 寄存器用来保存 GDT 在内存中的位置和长度
共 48 位, 高 32 位代表其地址, 其余用于保存 GDT 的段描述符数量
此处用到了 `asm.h`

``` makefile
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

``` makefile
  movl    %cr0, %eax       # 把 CR0 寄存器的值复制给 eax 寄存器
  orl     $CR0_PE, %eax    # 与 CR0_PE 做或运算，打开保护模式
  movl    %eax, %cr0
```

打开保护模式后, 此时指令仍然是 16 位代码, 需要通过长跳转跳转至 32 位

``` makefile
  ljmp    $(SEG_KCODE<<3), $start32
  
.code32  # Tell assembler to generate 32-bit code now.
```

进入保护模式运行, 做一些初始化

``` makefile
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

``` makefile
  # Set up the stack pointer and call into C.
  # 初始化栈，并调用 C 函数
  movl    $start, %esp
  call    bootmain
```

#### bootmain.c

`bootasm.S` 将 CPU 转为 32 位保护模式后, `bootmain` 函数要做的事情就是将 `ELF` 格式的内核从硬盘中加载到内存里



