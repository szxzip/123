## 模块: codegen.h — 目标代码生成接口

## 简明解释

- Single function `codegen_generate()`. Comment lists the quad→x86-64 mapping for each opcode.

---

# codegen.h 逐行讲解

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef CODEGEN_H` | 头文件保护宏的开始，`#ifndef` 即 "if not defined"，若 `CODEGEN_H` 尚未定义则编译后续内容，防止头文件被多次包含。 |
| 2 | `#define CODEGEN_H` | 定义宏 `CODEGEN_H`，标记此头文件已被包含，第二次 `#include` 时 `#ifndef` 检查将失败，跳过整个文件。这是 C 语言标准头文件保护写法。 |
| 3 | `#include "grammar.h"` | 包含 `grammar.h`，因为本文件中的函数声明用到了 `Compiler` 类型（定义在 `grammar.h` 中）。同时也间接引入了所有四元式操作码、符号表等核心数据结构。 |
| 4 | （空行） | 空白行，提高可读性。 |
| 5 | `// * codegen.h  --  x86-64 Linux target code generation (AT&T asm, GCC/GAS)` | 注释：说明本模块负责 x86-64 Linux 目标代码生成，输出格式为 AT&T 语法的汇编代码，可用 GCC 汇编器和 GAS (GNU Assembler) 处理。AT&T 语法特点：源操作数在左、目标在右（与 Intel 语法相反）；寄存器前加 `%`；立即数前加 `$`。 |
| 6 | `// *` | 空注释行，分隔符。 |
| 7 | `// * Quadruple to asm mapping:` | 注释：下列每一行描述一种四元式操作码到汇编指令的映射关系。 |
| 8 | `// *   program      -> .globl main / push rbp / ...` | `OP_PROGRAM` 四元式映射为：声明 `main` 为全局符号、压栈帧指针 `rbp`、建立栈帧等标准函数序言 (prologue)。 |
| 9 | `// *   assign       -> mov src, dst` | `OP_ASSIGN`（赋值 `:=`）映射为两条 `movl` 指令：先将源操作数加载到 `%eax`，再将 `%eax` 存入目标变量。 |
| 10 | `// *   add/sub/etc  -> mov a1,rax; op a2,rax; mov rax,r` | 加减乘除等算术运算映射为三步骤：①将第一操作数 `a1` 加载到 `%eax`；②对 `%eax` 执行算术运算（加/减/乘/除 `a2`）；③将 `%eax` 结果写回目标 `r`。这是经典的双地址指令单累加器模式。 |
| 11 | `// *   jmp          -> jmp L` | `OP_JMP`（无条件跳转）直接映射为 `jmp L` 指令，其中 `L` 是目标标号名称。 |
| 12 | `// *   jnz          -> cmpl $0,t; jne L` | `OP_JNZ`（条件为真时跳转）映射为：将条件变量 `t` 与立即数 `0` 比较 (`cmpl`)，然后若不等 (`jne`) 则跳转到标号 `L`。C 语言中非零即真，所以用 `jne` 来实现 "jump if not zero"。 |
| 13 | `// *   comparison   -> cmp a2,a1; setX al; movzbl al,eax; mov eax,t` | 比较运算映射为 `cmp + setX + movzbl` 模式：①用 `cmp` 比较 `a2` 和 `a1`（AT&T 语法 `cmp S, D` 计算 `D - S` 并设置标志位）；②用条件置位指令 `setX`（如 sete、setl 等）根据标志位将 `%al` 设为 0 或 1；③用 `movzbl` 将 `%al` 零扩展到 `%eax`（清除高 24 位）；④将 `%eax` 存入目标变量 `t`。这样比较结果被规整化为 0/1 布尔值。 |
| 14 | `// *   write        -> mov id,esi; lea fmt(rip),rdi; xor eax,eax; call printf` | `OP_WRITE` 映射为：①将变量值 `id` 加载到 `%esi`（printf 第二个参数，格式中的 `%d` 对应 int 值）；②用 `lea` 将格式串地址加载到 `%rdi`（printf 第一个参数）；③`xor eax,eax` 将 `%eax` 清零（x86-64 ABI 要求调用变参函数前 `%al` 指示向量寄存器使用个数，此处为 0）；④`call printf` 调用 C 标准库函数。 |
| 15 | `// *   end          -> xor eax,eax; leave; ret` | `OP_END` 映射为：①`xor eax,eax` 将返回值设为 0；②`leave` 等价于 `movq %rbp, %rsp; popq %rbp`，恢复栈帧；③`ret` 从函数返回。这是 x86-64 标准函数尾声 (epilogue)。 |
| 16 | `// *` | 空注释行。 |
| 17 | `// * Output: assemblable .s file, run with: gcc -no-pie x.s -o x` | 注释：输出为可汇编的 `.s` 文件，编译命令为 `gcc -no-pie x.s -o x`。`-no-pie` 禁止生成位置无关可执行文件，因为本编译器使用 `%rip` 相对寻址访问数据段（`.fmt(%rip)` 等），在不启用 PIE 的模式下更容易处理。 |
| 18 | （空行） | 空白行。 |
| 19 | `void codegen_generate(Compiler *c, const char *outfile);` | 函数声明：`codegen_generate` 接收两个参数 —— `Compiler *c` 是指向编译器全局上下文的指针（包含符号表、四元式列表等所有数据）；`const char *outfile` 是输出汇编文件的路径。函数无返回值。这是代码生成阶段的唯一入口函数。 |
| 20 | （空行） | 空白行。 |
| 21 | `#endif` | 头文件保护宏的结尾，与第 1 行的 `#ifndef` 配对。所有需要保护的内容必须写在 `#ifndef` 和 `#endif` 之间。 |
