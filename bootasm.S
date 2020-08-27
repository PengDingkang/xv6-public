#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# Start the first CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

# 从 16 位实模式切换到 32 位保护模式

.code16                       # 指代目前的运行模式：16 位实模式
.globl start
start:
  cli                         # 关中断指令

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax             # 将 ax 寄存器置零
  movw    %ax,%ds             # 接下来用 ax 的值去分别给以下 3 个寄存器置零 -> 数据段寄存器 Data Segment
  movw    %ax,%es             # -> 附加段寄存器 Extra Segment
  movw    %ax,%ss             # -> 堆栈段寄存器 Stack Segment

  # Physical address line A20 is tied to zero so that the first PCs 
  # with 2 MB would run software that assumed 1 MB.  Undo that.
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

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  # 第一步是准备全局描述符表 GDT
  # 这里 CPU 提供了一个 GDTR 寄存器用来保存 GDT 在内存中的位置和长度
  # 共 48 位, 高 32 位代表其地址, 其余用于保存 GDT 的段描述符数量
  lgdt    gdtdesc
  # 打开保护模式
  movl    %cr0, %eax              # 把 CR0 寄存器的值保存在 eax 寄存器中
  orl     $CR0_PE, %eax           # 与 CR0_PE 做按位或, 相当于给 0 位置 1
  movl    %eax, %cr0              # 将 eax 寄存器中的值写回到 cr0 中

//PAGEBREAK!
  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now.
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # 将 ax 寄存器赋值为段选择子 Our data segment selector
  movw    %ax, %ds                # 接下来用 ax 寄存器依次初始化数据段寄存器 -> DS: Data Segment
  movw    %ax, %es                # 扩展段寄存器 -> ES: Extra Segment
  movw    %ax, %ss                # 堆栈段寄存器 -> SS: Stack Segment
  movw    $0, %ax                 # ax 寄存器置零 Zero segments not ready for use
  movw    %ax, %fs                # 两个辅助寄存器置零 -> FS
  movw    %ax, %gs                # -> GS

  # Set up the stack pointer and call into C.
  # 初始化栈，并调用 C 函数
  movl    $start, %esp
  call    bootmain

  # If bootmain returns (it shouldn't), trigger a Bochs
  # breakpoint if running under Bochs, then loop.
  movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
  outw    %ax, %dx
spin:
  jmp     spin

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # 空
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # 代码段, STA_X 指代可执行, STA_R指代可读, 从 0x0 开始到 0xffffffff 结束
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # 数据段, STA_W 指代可写, 从 0x0 开始到 0xffffffff 结束

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # 16 位, 指示 长度
  .long   gdt                             # 32 位, 指示 GDT 地址

