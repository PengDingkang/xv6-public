# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
  # 利用上面的状态图, 我们可以理解这一步操作的意义, 即令 edx 和 eax 分别指向 old 和 new 以便之后使用
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # Save old callee-saved registers
  # 正如 context 定义中提到的, eip 并没有在这里被显式处理, 因为已经有别的函数去处理它了
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  # 在上面我们已经知道 eax 中存放的是指向 old 的指针，这里要做的便是将这个指针作为栈顶
  # 这样我们就完成了旧进程的保存
  movl %esp, (%eax)
  # 接下来就是恢复要运行的新进程，其本质就是将这个进程 context 中的各种寄存器值弹出到对应寄存器里
  # 要实现这一点，只需要把当前栈顶设置为 new 进程，就完成了新进程的切换
  movl %edx, %esp

  # Load new callee-saved registers
  # 和压栈的顺序反过来弹出
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
