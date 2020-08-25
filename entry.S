# The xv6 kernel starts executing in this file. This file is linked with
# the kernel C code, so it can refer to kernel symbols such as main().
# The boot block (bootasm.S and bootmain.c) jumps to entry below.
        
# Multiboot header, for multiboot boot loaders like GNU Grub.
# http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
#
# Using GRUB 2, you can boot xv6 from a file stored in a
# Linux file system by copying kernel or kernelmemfs to /boot
# and then adding this menu entry:
#
# menuentry "xv6" {
# 	insmod ext2
# 	set root='(hd0,msdos1)'
# 	set kernel='/boot/kernel'
# 	echo "Loading ${kernel}..."
# 	multiboot ${kernel} ${kernel}
# 	boot
# }

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# Multiboot header.  Data to direct multiboot loader.
.p2align 2
.text
.globl multiboot_header
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# By convention, the _start symbol specifies the ELF entry point.
# Since we haven't set up virtual memory yet, our entry point is
# the physical address of 'entry'.
# 虚拟地址转换物理地址
.globl _start
_start = V2P_WO(entry)

# Entering xv6 on boot processor, with paging off.
.globl entry
# 开启分页机制
entry:
  # 开启 4MB 内存分页支持
  movl    %cr4, %eax
  # CR4_PSE 为 Page Size Extensions 位，将其置 1 以实现 4MBytes 页大小
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Set page directory
  # entrypgdir 在 main.c 中，添加了两个页表项
  movl    $(V2P_WO(entrypgdir)), %eax
  # cr3 寄存器存放一级页表的地址
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  # 将 cr0 寄存器的 Write Protect 和 Paging 位置 1，开启内存分页机制
  orl     $(CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # esp 寄存器存放了一个指针指向系统栈最上面一个栈的栈顶
  movl $(stack + KSTACKSIZE), %esp

  # 跳转到 main 函数开始执行
  mov $main, %eax
  jmp *%eax

# 用 .comm 指令，在内核 bss 段定义一个 4096 大小的内核栈
.comm stack, KSTACKSIZE
