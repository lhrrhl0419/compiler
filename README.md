# 北京大学编译原理Lab课程实践报告

## 一、编译器概述

### 1.1 基本功能

本编译器实现了将sysy语言（简化版的c语言）翻译为koopa ir，之后再翻译为riscv汇编语言的功能。

sysy语言与koopa ir的定义见[lab的官方文档](https://pku-minic.github.io)

### 1.2 主要特点

本编译器的主要特点是能够正确处理sysy语言输入，做了一定程度的性能优化，性能测试结果为253.45s。

## 二、编译器设计

### 2.1 主要模块组成

编译器由2个主要模块组成：第一个负责从sysy到koopa ir的翻译，第二个负责从koopa ir到riscv的翻译。

### 2.2 主要数据结构

本编译器第一部分最核心的数据结构是`PartIR`与`IRINFO`。`PartIR`是大多数AST翻译成IR所对应的结构，包含三个部分。第一个部分是进入这段程序时所处的基本块在该AST中的部分，第二个部分是（在该AST跨多个块的情况下）结束时所处的基本块的开头部分，第三个部分是（如果有的话）完全包含在该AST中的各基本块。`IRINFO`起到作为符号表，设置不重复的变量名等功能。

第二部分最核心的数据结构是`Controller`，负责进行寄存器的调度。

### 2.3 主要设计考虑及算法选择

#### 2.3.1 符号表的设计考虑

在`IRINFO`中使用`unordered_map`来存储各变量被定义了几次（从而给同名变量分配不同编号），在哪一层等信息，使用栈来存储每个变量名当前的编号序列，栈顶是当前起作用的编号，大括号结束时弹出该层的所有编号。

#### 2.3.2 寄存器分配策略

寄存器分配分为三个部分。

* `t5,t6`专门用于临时计算。
* `s*` 用于存储较长时间使用的变量（详见2.3.3）。
* `a*,t0-t4`用于存储其他变量。

#### 2.3.3 采用的优化策略

对函数体和while循环，为了防止基本块跳转导致大量的不必要的寄存器存储和读取，在转为riscv之前统计其内部各变量出现在几个基本块中，按照顺序将其存在`s*`中（放在这里面是为了防止函数调用导致值被改变），不需要内存读写操作。

对其他的采取LRU策略。

在编译期对所有表达式都试图求值。

将乘除法在可能的情况下转化为移位操作等来提高效率。

## 三、编译器实现

### 3.1 各阶段编码细节

#### Lv1. main函数和Lv2. 初试目标代码生成

设置`BaseAST`，`BaseIR`，`RISCV`及前两个的子类。

#### Lv3. 表达式, Lv4. 常量和变量, Lv5. 语句块和作用域

实现对可求值的表达式的求值操作
设置临时变量与其他变量，常量的命名方式以保证不会重名
设置符号表
实现LRU策略

#### Lv6. if语句

将Stmt拆成SealedStmt和OpenStmt来处理二义性问题。

#### Lv7. while语句

对continue和break使用回填的方法
优化部分见2.3.3

#### Lv8. 函数和全局变量

将全局变量作为符号表的第0层，函数内的作为符号表第1层，函数内的while循环内的作为符号表第2层，以此类推。

#### Lv9. 数组

把所有数组当作一维数组处理，将

```c++
int a[10][10];
int b=a[x][y];
```

翻译成相当于

```c++
int a[100];
int t=x*10+y;
int b=a[t];
```

的形式。
