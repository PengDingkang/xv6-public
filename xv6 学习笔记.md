# xv6 学习笔记

xv6 是一个完整的，运行在多核环境下的简单操作系统。

## Makefile 学习

make 命令执行时，需要一个 makefile 文件，以告诉 make 命令需要怎么样的去编译和链接程序。

### Makeflie 的书写规则

``` makefile
target ... : prerequisites ...
	command
	...
```

#### target
可以是一个 object file （目标文件），也可以是一个执行文件，还可以是一个标签 (label)。

#### prerequisites
生成该 target 所依赖的文件和/或 target。

#### command
该 target 要执行的命令（任意的 shell 命令）。
注意 command 需要以 tab 开头。

#### 其他书写注意事项

当依赖的最后更新日期比目标文件更新或目标文件不存在时，则 make 会执行其之后的命令。
注意，如果 target 的冒号之后没有指定依赖，那么 make 指令不会自动寻找依赖也不会自动执行之后的命令。
需要在 make 之后显式指出 label 名称，如 make clean。
这种特性可以允许我们定义一些编译无关的命令，如程序打包。
或者下面的 dist，产生发布软件包文件（即 distribution package）。这个命令将会将可执行文件及相关文件打包成一个tar.gz压缩的文件用来作为发布软件的软件包。

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

1. 读入所有的Makefile。
2. 读入被include的其它Makefile。
3. 初始化文件中的变量。
4. 推导隐晦规则，并分析所有规则。
5. 为所有的目标文件创建依赖关系链。
6. 根据依赖关系，决定哪些目标要重新生成。
7. 执行生成命令。



