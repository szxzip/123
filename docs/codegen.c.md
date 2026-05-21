## 模块: codegen.c — 目标代码生成实现 (x86-64 AT&T)

## 简明解释

- `is_const_operand()`: 区分常数字符串（→ `$立即数`）与变量名（→ `变量名(%rip)`）
- `write_cmp_set()`: 生成 cmp+setX+movzbl 模式——比较两个值，将 0/1 布尔结果存入变量。用于 JE/JL/JG 等。
- `codegen_generate()`: 主函数，生成完整的 `.s` 文件：
  1. `.section .rodata`：printf 的格式字符串 `"%d\n"`
  2. `.section .bss`：零初始化变量（所有非程序符号）
  3. `.section .text`：`main:` 函数，带标准序言（`pushq rbp; movq rsp rbp`）
  4. 逐条四元式翻译循环：每个 OP_* 分支生成对应的 x86-64 AT&T 指令。Assign=movl, Add/Sub/Mul/Div=算术运算, Comparison=cmp+setX, JMP=jmp, JNZ=cmp+je, WRITE=printf 调用, END=xorl+leave+ret, LABEL=标号：
  5. 末尾的 `.size` 指令
- 编译器原理：代码生成是最后阶段，将平台无关的 IR 翻译为目标机器指令。AT&T 语法使用 `$` 表示立即数，`%` 表示寄存器，`(%rip)` 表示 RIP 相对寻址（位置无关代码）。printf 的调用约定：esi=值（第二参数），rdi=格式字符串（第一参数），eax=0（变参中无浮点参数）。

---

# codegen.c 逐行讲解

## 文件头部

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#include "codegen.h"` | 包含自身头文件 `codegen.h`，获取 `codegen_generate` 的函数声明以及间接引入 `grammar.h` 中的所有定义（`Compiler`、`Symbol`、`Quadruple`、`op_names[]`、操作码枚举等）。这是该模块唯一的依赖，因为 `grammar.h` 已经被 `codegen.h` 包含。 |
| 2 | （空行） | 空白行。 |

---

## is_const_operand() — 区分常数 ($) 与变量 (%rip)

这是代码生成中最重要的辅助函数。x86-64 AT&T 汇编中，**立即数（常数）**前面要加 `$` 前缀，**内存操作数（变量）**用 `变量名(%rip)` 的 RIP 相对寻址方式。该函数负责判断一个操作数字符串究竟是常数还是变量名，从而决定生成哪种汇编语法。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 3 | `// 判断操作数是否为常数 (字符串以数字或负号开头)` | 注释：说明判断规则——如果字符串以数字字符 (`0`-`9`) 或负号 (`-`) 开头，则认为是常数。 |
| 4 | `static int is_const_operand(const char *s) {` | 函数定义：`static` 限制作用域在本文件内；`int` 返回类型，1 表示是常数，0 表示不是；`const char *s` 是操作数字符串，可能的值如 `"5"`、`"-3"`、`"x"`、`"t1"`、`"_"`。 |
| 5 | `if (!s || s[0] == '\0' || s[0] == '_') return 0;` | 三种快速排除情况，直接返回 0（不是常数）：①空指针 `!s`——防御性编程；②空字符串 `s[0] == '\0'`——无内容当然不是常数；③下划线开头 `s[0] == '_'`——四元式中 `_` 表示"未使用"的占位符，如 `(jmp, _, _, L1)` 中的 `_`。变量名和标签名以字母开头，不会以 `_` 开头。 |
| 6 | `if (s[0] == '-' && s[1] >= '0' && s[1] <= '9') return 1;` | 处理负数常数：如果第一个字符是 `-` 且第二个字符是数字 (`0`-`9`)，则认定为常数。例如 `"-5"`，这样生成汇编时写为 `$-5`。注意这里只检查了两个字符，所以负号紧跟数字即判断为常数（实际源码中的常数不会出现 `-x` 这种形式）。 |
| 7 | `return (s[0] >= '0' && s[0] <= '9');` | 处理正数常数：如果第一个字符是数字 `0`-`9`，返回 1（常数）；否则返回 0（变量名）。因为变量名由词法分析器产生，必定以字母开头（如 `x`、`count`、`t1`），而常数以数字或负号开头。返回值就是条件表达式的结果（0 或 1），简洁高效。 |
| 8 | `}` | 函数结束右大括号。 |

**核心逻辑总结**：操作数的字符串表示来自四元式结构。词法分析阶段将整数常数转换为十进制字符串（存入 `arg1`/`arg2`/`result` 字段），变量名保持原样。因此通过判断首字符即可区分：首字符为 `0-9` 或 `-` 是常数，需要 `$` 前缀；首字符为字母是变量/标签，需要 `(%rip)` 后缀。

---

## write_cmp_set() — cmp + setX + movzbl 比较模式

x86-64 没有将两个值比较后直接得到 0/1 布尔结果的单条指令。需要用多条指令组合实现：先 `cmp` 比较设置标志位，再用条件置位指令 `setX` 根据标志位将字节寄存器设为 0 或 1，最后零扩展到 32 位。这个函数封装了完整的比较-置位-写回流程，被 6 种条件跳转操作码（OP_JE~OP_JGE）共享。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 10 | `// 将操作数字符串写入为汇编操作数` | 注释：说明下文给操作数加上适当的汇编语法前缀/后缀。 |
| 11 | `// * 常数 → $value ; 变量 → varname(%rip)` | 注释：常数输出时加 `$` 前缀（如 `$5`），变量输出时加 `(%rip)` 后缀（如 `x(%rip)`），后者表示 RIP 相对寻址——以当前指令指针 `%rip` 为基址的 32 位偏移访问数据段。 |
| 12 | `static void write_cmp_set(FILE *f, const char *set_instr,` | 函数定义：`static` 作用域限制；`void` 无返回值；参数分别为输出文件指针 `f`、条件置位指令字符串 `set_instr`（如 `"sete"`、`"setl"`）以及三个四元式字段 `a1`、`a2`、`r`。 |
| 13 | `                           const char *a1, const char *a2, const char *r) {` | 续行：`a1` 和 `a2` 是参与比较的两个操作数，`r` 是存放比较结果（0 或 1）的目标变量。 |
| 14 | `    // 将 a1 加载到 %eax` | 注释：步骤一——将第一个操作数加载到累加器 `%eax`。 |
| 15 | `    if (is_const_operand(a1)) {` | 判断 `a1` 是否为常数。 |
| 16 | `        fprintf(f, "    movl    $%s, %%eax\n", a1);` | 若 `a1` 是常数：生成 `movl $常数, %eax`。`movl` 是 32 位数据传送指令（"l" 后缀表示 long/32-bit）；`$%s` 输出 `$` + 常数值；`%%eax` 在 `printf` 格式串中双写 `%` 以输出一个字面 `%`。最终生成如 `movl $5, %eax`。 |
| 17 | `    } else {` | 若 `a1` 不是常数…… |
| 18 | `        fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 生成 `movl 变量名(%rip), %eax`。`(%rip)` 是 RIP 相对寻址，访问位于数据段的全局变量。最终生成如 `movl x(%rip), %eax`，含义为：将内存地址 `x` 处的 32 位值加载到 `%eax`。 |
| 19 | `    }` | 内层 if-else 结束。 |
| 20 | `    // 与 a2 比较` | 注释：步骤二——将 `%eax` 与 `a2` 进行比较。`cmpl S, D` 在 AT&T 语法中计算 `D - S`（即 `%eax - a2`），根据结果设置 EFLAGS 寄存器的标志位（ZF=零标志, SF=符号标志, OF=溢出标志, CF=进位标志）。比较之后不保留差值，只影响标志位。 |
| 21 | `    if (is_const_operand(a2)) {` | 判断 `a2` 是否为常数。 |
| 22 | `        fprintf(f, "    cmpl    $%s, %%eax\n", a2);` | 若 `a2` 是常数：生成 `cmpl $常数, %eax`。例如 `cmpl $10, %eax` 计算 `%eax - 10`，设置标志位。 |
| 23 | `    } else {` | 若 `a2` 不是常数…… |
| 24 | `        fprintf(f, "    cmpl    %s(%%rip), %%eax\n", a2);` | 生成 `cmpl 变量名(%rip), %eax`。例如 `cmpl y(%rip), %eax` 计算 `%eax - [y]`。 |
| 25 | `    }` | 内层 if-else 结束。 |
| 26 | `    fprintf(f, "    %s    %%al\n", set_instr);` | 生成条件置位指令，如 `sete %al`。`setX` 是一族 x86-64 指令，根据 EFLAGS 标志位将 8 位目标寄存器设为 0 或 1。`%al` 是 `%rax` 的最低 8 位（字节寄存器）。例如 `sete`：若 ZF=1（上一条 cmp 结果相等），则 `%al=1`；否则 `%al=0`。`setl`：若 SF≠OF（有符号小于），则 `%al=1`。这是比较结果 0/1 布尔化的关键步骤。 |
| 27 | `    fprintf(f, "    movzbl  %%al, %%eax\n");` | 生成 `movzbl %al, %eax`。`movzbl` 是 "move zero-extend byte to long"——将 `%al`（8 位）零扩展到 `%eax`（32 位），即将高 24 位全部清零。这一步很关键：`setX` 只修改 `%al`，`%eax` 的高 24 位可能是之前遗留的脏数据。零扩展后 `%eax` 中要么是 0，要么是 1（干净的 32 位值）。 |
| 28 | `    fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将 `%eax`（现在存储着干净的 0 或 1）写入目标变量 `r`。例如 `movl %eax, t1(%rip)`。到此 `r` 中存储的就是两个操作数比较后的布尔结果（1=true, 0=false），上层代码用 `OP_JNZ` 检查这个值来决定分支。 |
| 29 | `}` | 函数结束。 |

**cmp + setX + movzbl 模式要解决的问题**：高级语言中 `x > y` 的结果应该是一个布尔值 (0 或 1)，可以赋值给变量。但 x86-64 的比较指令 `cmp` 只设置标志位，不产生数值结果；`setX` 产生字节结果；`movzbl` 将字节结果扩展到 32 位使其可以被安全地当作 int 使用。这三条指令配合实现了语义正确的布尔化比较。

---

## codegen_generate() — 主代码生成函数

这是整个代码生成模块的核心函数，遍历四元式列表，为每个四元式生成对应的汇编指令并写入输出文件。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 31 | `void codegen_generate(Compiler *c, const char *outfile) {` | 函数定义：`void` 无返回值；`Compiler *c` 是编译器上下文指针，包含符号表、四元式列表等所有编译数据；`const char *outfile` 是输出汇编文件的路径名。 |
| 32 | `    FILE *f = fopen(outfile, "w");` | 以写入模式 (`"w"`) 打开输出文件。`fopen` 返回 `FILE*` 指针，后续用 `fprintf` 向该文件写入汇编代码。若文件已存在会被截断重写。 |
| 33 | `    if (!f) {` | 错误检查：若 `fopen` 返回 `NULL`（文件无法打开，如权限不足或路径不存在）…… |
| 34 | `        fprintf(stderr, "无法写入文件: %s\n", outfile);` | 向标准错误输出打印错误信息，告知用户哪个文件无法写入。 |
| 35 | `        return;` | 提前返回，不继续执行后续代码生成逻辑。 |
| 36 | `    }` | if 块结束。 |
| 37 | （空行） | 空白行。 |

---

### .section .rodata — 只读数据段（格式字符串）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 38 | `    // ---- 数据段: 格式串与变量 ----` | 注释：分隔线，标志数据段部分开始。 |
| 39 | `    fprintf(f, "    .section .rodata\n");` | 输出汇编伪指令 `.section .rodata`，声明其后内容放入只读数据段 (read-only data)。`.rodata` 通常存放字符串常量和只读全局变量，操作系统会将此段映射为只读内存页，写入会触发段错误，有助于安全。 |
| 40 | `    fprintf(f, ".fmt:\n");` | 定义标号 `.fmt`，这是格式字符串 `"%d\n"` 的地址标号。注意标号以 `.` 开头（局部标号风格），在同一个汇编文件中通过 `.fmt(%rip)` 引用。 |
| 41 | `    fprintf(f, "    .string \"%%d\\n\"\n\n");` | 用 `.string` 伪指令定义一个以 NUL 结尾的 C 字符串。`%%d` 输出字面 `%d`：在 `printf` 格式串中 `%%d` 的第一个 `%` 转义第二个 `%`，最终汇编器输出 `%d` 字节。`\\n` 输出 `\n`（换行符）。所以格式串实际内容为 `"%d\n"`，用于 `printf` 格式化输出整数后换行。末尾的 `\n\n` 追加两个空行，提升汇编输出的可读性。 |

---

### .section .bss — 未初始化数据段（变量分配）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 43 | `    // 收集需要在 .bss 中分配的变量 (kind != KIND_PROGRAM)` | 注释：遍历符号表，为非程序名（`KIND_PROGRAM`）的符号在 BSS 段中分配空间。`KIND_PROGRAM=0` 表示程序名符号，不需要分配空间。 |
| 44 | `    fprintf(f, "    .section .bss\n");` | 输出伪指令 `.section .bss`，声明后续内容放入 BSS 段（Block Started by Symbol）。BSS 段存放未初始化的全局变量和静态变量，在可执行文件中不占实际空间（只记录大小），程序加载时由操作系统清零。 |
| 45 | `    int i, max_offset = 0;` | 声明循环变量 `i` 和 `max_offset`（最大偏移量）。`max_offset` 会累加变量长度以找出整个数据段需要的最大字节数（虽然在当前实现中未实际使用 `max_offset`，但它记录了 .bss 段的理论总大小，方便调试和未来可能的栈帧优化）。 |
| 46 | `    for (i = 0; i < c->sym_count; i++) {` | 遍历符号表中的所有条目。`sym_count` 是符号表实际条目数，上限为 `MAX_SYMBOLS`。 |
| 47 | `        Symbol *s = &c->sym_table[i];` | 取第 `i` 个符号的指针 `s`，方便后续访问其字段 `name`、`kind`、`offset`、`len`。 |
| 48 | `        if (s->kind != KIND_PROGRAM) {` | 过滤掉程序名符号：`KIND_PROGRAM=0` 表示程序名（如 `program foo` 中的 `foo`），它不需要在数据段分配空间。其余符号（`KIND_VARIABLE=1` 变量、`KIND_TEMP=2` 临时变量）都需要内存空间。 |
| 49 | `            fprintf(f, "%s:\n", s->name);` | 为变量定义标号，标号名就是变量名本身（如 `x:`）。GAS 汇编中冒号结尾的标识符是标号定义。因为使用了 `.globl main` 但未声明这些变量为 `.globl`，所以它们默认为局部符号，仅在当前汇编文件内可见。 |
| 50 | `            fprintf(f, "    .zero %d\n", s->len);` | 用 `.zero N` 伪指令为变量分配 `N` 个字节并全部填零。`.zero` 等价于在 BSS 段中预留空间的 `.space`。`s->len` 是变量的类型宽度：`int` 为 4 字节，`real` 为 8 字节，`char` 为 1 字节（见 `grammar.h` 中 `type_len()` 函数）。虽然 BSS 段本身在加载时就被操作系统清零，显式写 `.zero` 可以让 GAS 更加明确分配意图。 |
| 51 | `            if (s->offset + s->len > max_offset)` | 比较当前变量的 `offset + len` 是否超过 `max_offset`。`offset` 是变量在模拟活动记录中的偏移（由语义分析阶段分配），`len` 是类型宽度。`offset + len` 表示该变量占用的末尾位置。 |
| 52 | `                max_offset = s->offset + s->len;` | 更新 `max_offset` 为当前最大末尾偏移。循环结束后 `max_offset` 就是所有变量中最远的 `offset + len`，即数据区所需的总字节数。这个值在当前实现中未直接用于生成汇编，但可以用来确认或设置 `.bss` 段大小。 |
| 53 | `        }` | 内层 if 结束。 |
| 54 | `    }` | for 循环结束。 |
| 55 | `    fprintf(f, "\n");` | 输出一个额外空行，分隔数据段和接下来的代码段，使生成的 `.s` 文件更易读。 |

**为什么计算 max_offset**：虽然当前实现没有在输出中使用 `max_offset`，但理论上它可以用来生成 `.comm` 伪指令的统一分配，或者为未来的栈帧优化提供信息——比如确认所有变量的总大小是否超过某个阈值。在 BSS 段中逐个用 `.zero` 分配的做法简单直观，每个变量有独立的标号方便引用。

---

### .section .text — 代码段与 main 函数序言

| 行号 | 代码 | 讲解 |
|------|------|------|
| 57 | `    // ---- 代码段 ----` | 注释：分隔线，表示代码段部分开始。 |
| 58 | `    fprintf(f, "    .section .text\n");` | 输出伪指令 `.section .text`，声明后续内容放入代码段 (text section)。`.text` 段存放可执行的机器指令，通常映射为只读+可执行内存页。 |
| 59 | `    fprintf(f, "    .globl  main\n");` | 声明 `main` 为全局符号 (global)，这样链接器 `ld` 才能找到程序的入口点。x86-64 Linux 下，C 运行时初始化代码 (`_start`) 会调用 `main`，所以 `main` 必须是全局可见的。`globl` 是 GAS 中 `global` 的简写形式（缺少一个 `a` 是 GAS 的历史遗留/许可拼写）。 |
| 60 | `    fprintf(f, "    .type   main, @function\n");` | 声明 `main` 是函数类型 (`@function`)。这个伪指令帮助调试器和链接器正确识别符号类型（函数 vs 数据对象），对生成正确的调试信息和 ELF 符号表很重要。 |
| 61 | `    fprintf(f, "main:\n");` | 定义 `main` 函数入口标号。`main:` 由此开始，冒号表示此处是一个代码地址，后续指令都位于该地址处。 |
| 62 | `    fprintf(f, "    pushq   %%rbp\n");` | **函数序言第一步**：将调用者的帧指针 `%rbp` 压栈保存。`pushq` 是 64 位压栈指令（"q"=quadword）。`%rbp` (Base Pointer) 指向调用者的栈帧基址，压栈后 `%rsp` (Stack Pointer) 减去 8。这是 x86-64 System V ABI 约定的标准函数序言第一步。 |
| 63 | `    fprintf(f, "    movq    %%rsp, %%rbp\n\n");` | **函数序言第二步**：将当前栈指针 `%rsp` 复制到 `%rbp`。从此 `%rbp` 指向当前函数的栈帧基址，`%rbp` 到 `%rsp` 之间的栈空间就是本函数的栈帧。`movq` 是 64 位数据传送。此后在函数内可以通过 `%rbp` 加偏移访问局部变量和参数。结尾的 `\n\n` 多输出一个空行。 |

**x86-64 函数序言标准模式**：
```
pushq %rbp      ; 保存旧的帧指针
movq %rsp, %rbp ; 建立新的帧指针
```
这两条指令在所有函数开头固定出现（编译器也可能省略优化）。虽然此编译器把全局变量放在 BSS 段而不用栈分配局部变量，但保留标准序言确保生成的代码遵循 ABI 约定，与 `leave` / `ret` 形成对称。

---

### 遍历四元式生成汇编指令

| 行号 | 代码 | 讲解 |
|------|------|------|
| 65 | `    // 遍历四元式, 生成汇编` | 注释：核心生成逻辑——逐个处理四元式列表。 |
| 66 | `    for (i = 0; i < c->quad_count; i++) {` | 循环遍历所有四元式。`quad_count` 是四元式个数，由解析器在语义分析阶段填充。 |
| 67 | `        Quadruple *q = &c->quads[i];` | 取第 `i` 个四元式的指针 `q`，避免反复用数组下标访问。 |
| 68 | `        const char *a1 = q->arg1;` | 将四元式的三个操作数字段提取为局部变量：`a1`（第一操作数）。 |
| 69 | `        const char *a2 = q->arg2;` | `a2`（第二操作数）。某些操作码（如 `OP_JMP`、`OP_LABEL`）不使用这两个字段，其值为 `"_"`。 |
| 70 | `        const char *r  = q->result;` | `r`（结果/目标）。在不同操作码中含义不同：可能是存放结果的目标变量，也可能是跳转的目标标号（`OP_JMP`、`OP_JNZ` 中）。 |
| 71 | （空行） | 空白行。 |
| 72 | `        fprintf(f, "    # %-6s %-6s %-6s %s\n",` | 在每条汇编指令之前输出一行注释。`#` 是 GAS 汇编的单行注释符；`%-6s` 是左对齐宽度 6 的字符串格式，使输出列对齐。 |
| 73 | `                op_names[q->op], a1, a2, r);` | 注释内容为：操作码名称（如 `:=`、`+`、`jmp` 等）和三个操作数。`op_names[]` 是定义在 `quadruple.c` 中的全局字符串数组，将操作码枚举值映射为人类可读的名称（如 `op_names[OP_ADD]` = `"+"`）。这让生成的 `.s` 文件能直观看到每条汇编指令对应的四元式。 |
| 74 | （空行） | 空白行。 |
| 75 | `        switch (q->op) {` | 根据四元式的操作码 (`op`) 分支到不同的汇编生成逻辑。这是经典的基于操作码的代码生成调度。 |
| 76 | （空行） | 空白行。 |

---

#### OP_PROGRAM — case 77-79

| 行号 | 代码 | 讲解 |
|------|------|------|
| 77 | `        case OP_PROGRAM:` | 匹配程序入口四元式 `(program, id, _, _)`。这是四元式列表的第一条，来自解析 `program id` 的文法规则。参数 `id` 在本实现中未使用，因为入口函数名固定为 `main`。 |
| 78 | `            // 无需额外代码, main 入口已在上面` | 注释：`main` 函数的序言（pushq rbp / movq rsp rbp）已经在循环之前输出，此 case 无需生成任何额外代码。 |
| 79 | `            break;` | 跳出 switch。 |

---

#### OP_ASSIGN — case 81-90（赋值 :=）

赋值操作的四元式为 `(:=, src, _, dst)`。语义：将 `src` 的值复制到 `dst`。汇编实现：先加载 src 到 `%eax`，再写入 dst。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 81 | `        case OP_ASSIGN:` | 匹配赋值操作码（枚举值 2）。 |
| 82 | `            // (:=, src, _, dst)` | 注释：说明四元式格式——第一参数是源操作数，第二参数固定为 `_`（未使用），第三参数是目标变量。 |
| 83 | `            // * movl src, %eax ; movl %eax, dst` | 注释：说明生成的汇编逻辑——两步数据传送。为什么要经过 `%eax` 中转？x86-64 的 `movl` 不允许两个操作数都是内存地址（不允许 memory-to-memory 传送），所以必须先将源加载到寄存器，再从寄存器写入目标。 |
| 84 | `            if (is_const_operand(a1)) {` | 判断源操作数 `a1` 是否为常数。 |
| 85 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 若 `a1` 是常数（如 `"5"`），生成 `movl $5, %eax`，将立即数 5 加载到 `%eax`。`$` 前缀在 AT&T 语法中表示立即数。 |
| 86 | `            } else {` | 若 `a1` 不是常数…… |
| 87 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 生成 `movl 变量名(%rip), %eax`，从内存加载变量值到 `%eax`。例如 `x := 5` → `movl $5, %eax`；`x := y` → `movl y(%rip), %eax`。 |
| 88 | `            }` | if-else 结束。 |
| 89 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将 `%eax` 的值写入目标变量 `r`。RIP 相对寻址访问数据段中的变量。例如 `movl %eax, x(%rip)`。 |
| 90 | `            break;` | 跳出 switch。 |

---

#### OP_ADD — case 92-104（加法 +）

加法操作的四元式为 `(+, a1, a2, r)`。语义：`r = a1 + a2`。汇编实现：加载 a1 到 `%eax`，对 `%eax` 加上 a2，将结果写回 r。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 92 | `        case OP_ADD:` | 匹配加法操作码（枚举值 3）。 |
| 93 | `            if (is_const_operand(a1)) {` | 判断第一个操作数是否为常数。 |
| 94 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 若 `a1` 是常数，用 `movl $值, %eax` 立即数加载。 |
| 95 | `            } else {` | 否则…… |
| 96 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量内存加载到 `%eax`。 |
| 97 | `            }` | 加载部分 if-else 结束。 |
| 98 | `            if (is_const_operand(a2)) {` | 判断第二个操作数是否为常数。 |
| 99 | `                fprintf(f, "    addl    $%s, %%eax\n", a2);` | 若 `a2` 为常数，生成 `addl $值, %eax`。`addl` 是 32 位加法指令，将源操作数加到目标操作数上：`%eax = %eax + a2`。例如 `addl $3, %eax`。 |
| 100 | `            } else {` | 否则…… |
| 101 | `                fprintf(f, "    addl    %s(%%rip), %%eax\n", a2);` | 用变量值加到 `%eax`：`addl y(%rip), %eax`。 |
| 102 | `            }` | 加法部分 if-else 结束。 |
| 103 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将 `%eax`（此时已为 `a1 + a2` 的和）写入目标变量 `r`。 |
| 104 | `            break;` | 跳出 switch。 |

---

#### OP_SUB — case 106-118（减法 -）

减法操作的四元式为 `(-, a1, a2, r)`。语义：`r = a1 - a2`。汇编实现与加法完全相同，仅将 `addl` 换成 `subl`。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 106 | `        case OP_SUB:` | 匹配减法操作码（枚举值 4）。 |
| 107 | `            if (is_const_operand(a1)) {` | 判断第一操作数是否为常数。 |
| 108 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 109 | `            } else {` | 否则…… |
| 110 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载到 `%eax`。 |
| 111 | `            }` | if-else 结束。 |
| 112 | `            if (is_const_operand(a2)) {` | 判断减数 `a2` 是否为常数。 |
| 113 | `                fprintf(f, "    subl    $%s, %%eax\n", a2);` | `subl $值, %eax`：从 `%eax` 减去立即数。AT&T 语法中 `subl S, D` 计算 `D = D - S`。 |
| 114 | `            } else {` | 否则…… |
| 115 | `                fprintf(f, "    subl    %s(%%rip), %%eax\n", a2);` | `subl y(%rip), %eax`：从 `%eax` 减去变量 `y` 的值。 |
| 116 | `            }` | if-else 结束。 |
| 117 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将结果写回目标变量。 |
| 118 | `            break;` | 跳出 switch。 |

---

#### OP_MUL — case 120-132（乘法 *）

乘法操作的四元式为 `(*, a1, a2, r)`。语义：`r = a1 * a2`。实现时对立即数乘数使用三操作数形式的 `imull` 指令。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 120 | `        case OP_MUL:` | 匹配乘法操作码（枚举值 5）。 |
| 121 | `            if (is_const_operand(a1)) {` | 判断被乘数是否为常数。 |
| 122 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 123 | `            } else {` | 否则…… |
| 124 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载到 `%eax`。 |
| 125 | `            }` | if-else 结束。 |
| 126 | `            if (is_const_operand(a2)) {` | 判断乘数 `a2` 是否为常数。 |
| 127 | `                fprintf(f, "    imull   $%s, %%eax, %%eax\n", a2);` | **三操作数形式**：`imull $常数, %eax, %eax`。这是 x86-64 中 `imull` 的特殊编码：`imull S, D1, D2` 计算 `D2 = D1 * S`。这里 `D1=%eax`（已加载的 a1），`S=常数`，`D2=%eax`（覆盖原值），因此效果为 `%eax = %eax * 常数`。三操作数形式只能对立即数使用，不能对内存操作数，所以只有 `a2` 为常数时才能用这种简洁形式。`imull` 是 32 位有符号乘法指令（"i" 前缀表示 signed integer）。 |
| 128 | `            } else {` | 若 `a2` 不是常数…… |
| 129 | `                fprintf(f, "    imull   %s(%%rip), %%eax\n", a2);` | **二操作数形式**：`imull 变量(%rip), %eax`，计算 `%eax = %eax * [变量]`。这是标准的 `imull S, D` 两操作数形式，`D = D * S`。两个操作数不可同时为内存地址——这里源是内存中的变量，目标是寄存器 `%eax`，满足约束。 |
| 130 | `            }` | if-else 结束。 |
| 131 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将乘法结果从 `%eax` 写入目标变量。 |
| 132 | `            break;` | 跳出 switch。 |

**为什么乘法有特殊处理**：x86-64 的 `imull` 支持两种编码：两操作数形式 `imull src, dst`（`dst*=src`）；三操作数形式 `imull imm, src, dst`（`dst=src*imm`）。三操作数形式更高效（一条指令完成常量乘法），但只能对立即数使用。同时 `a1` 必须预先加载到 `%eax`，因为无论哪种形式，都需要一个寄存器来存放一个操作数。

---

#### OP_DIV — case 134-149（除法 /）

除法操作的四元式为 `(/, a1, a2, r)`。语义：`r = a1 / a2`（整数除法）。实现最复杂，因为 x86-64 的除法指令有特殊要求。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 134 | `        case OP_DIV:` | 匹配除法操作码（枚举值 6）。 |
| 135 | `            if (is_const_operand(a1)) {` | 判断被除数 `a1` 是否为常数。 |
| 136 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 137 | `            } else {` | 否则…… |
| 138 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载被除数到 `%eax`。 |
| 139 | `            }` | if-else 结束。 |
| 140 | `            fprintf(f, "    cltd\n"); // 符号扩展 %eax → %edx:%eax` | **`cltd` (Convert Long to Double-long)**：将 `%eax` 中的 32 位有符号值符号扩展到 64 位的 `%edx:%eax` 寄存器对。`idivl` 指令要求被除数为 `%edx:%eax` 组成的 64 位值（分子），除数为 32 位（分母）。如果 `%eax` 中的值为正，`%edx` 被填充为 `0x00000000`；如果为负，`%edx` 被填充为 `0xFFFFFFFF`（全 1）。这一步对于有符号除法至关重要——不执行 `cltd` 会导致 `%edx` 保留脏数据，除法结果将完全错误。 |
| 141 | `            if (is_const_operand(a2)) {` | 判断除数 `a2` 是否为常数。 |
| 142 | `                // idiv 不支持立即数, 需要先 mov 到寄存器` | 注释：重要说明——`idivl` 指令**不允许立即数作为操作数**（x86 指令集限制）。因此若除数为常数，必须先将其加载到某个通用寄存器中。 |
| 143 | `                fprintf(f, "    movl    $%s, %%ecx\n", a2);` | 将常数除数加载到 `%ecx` 寄存器。选择 `%ecx` 作为中转寄存器，因为 `idivl` 可以将任意 32 位寄存器作为除数，而 `%ecx` 是调用者保存寄存器（volatile），不会与 ABI 约定冲突。`%ecx` 是 `%rcx` 的低 32 位。 |
| 144 | `                fprintf(f, "    idivl   %%ecx\n");` | **`idivl %ecx`**：有符号除法——将 64 位被除数 `%edx:%eax` 除以 32 位除数 `%ecx`，商存放在 `%eax`，余数存放在 `%edx`。本编译器只关心商（整数除法结果），忽略余数。`idivl` 的 "i" 前缀表示有符号除法；无符号除法版本是 `divl`。 |
| 145 | `            } else {` | 若除数 `a2` 不是常数…… |
| 146 | `                fprintf(f, "    idivl   %s(%%rip)\n", a2);` | 直接用内存操作数作为除数：`idivl 变量(%rip)`，将被除数 `%edx:%eax` 除以该变量值，商存入 `%eax`。这也是合法的 `idivl` 操作数形式。 |
| 147 | `            }` | if-else 结束。 |
| 148 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将商 (`%eax` 中的值) 写入目标变量 `r`。 |
| 149 | `            break;` | 跳出 switch。 |

**除法 · cltd + idivl 规约总结**：
1. `cltd` 必须出现在 `idivl` 之前，将 `%eax` 符号扩展到 `%edx:%eax`
2. `idivl` 不接受立即数除数，常数除数需先装入寄存器（此处选 `%ecx`）
3. 商总是进入 `%eax`，余数进入 `%edx`（本编译器忽略余数）
4. 除数为零时 CPU 会触发除零异常（#DE），本编译器不做运行时检查

---

#### OP_AND_OP — case 151-163（逻辑与 AND）

逻辑与操作的四元式为 `(and, a1, a2, r)`。语义：`r = a1 AND a2`（按位与，0 为假 / 非 0 为真）。在 C 风格语义中，逻辑与相当于按位与（因为真值已被归一化为 1 和 0）。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 151 | `        case OP_AND_OP:` | 匹配逻辑与操作码（枚举值 15）。命名为 `OP_AND_OP` 而非 `OP_AND` 是为了避免与 C 标准库中的 `AND` 宏/保留字冲突。 |
| 152 | `            if (is_const_operand(a1)) {` | 判断第一操作数是否为常数。 |
| 153 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 154 | `            } else {` | 否则…… |
| 155 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载到 `%eax`。 |
| 156 | `            }` | if-else 结束。 |
| 157 | `            if (is_const_operand(a2)) {` | 判断第二操作数是否为常数。 |
| 158 | `                fprintf(f, "    andl    $%s, %%eax\n", a2);` | `andl $值, %eax`：将 `%eax` 与立即数按位与。`andl` 是 32 位按位与指令，`D = D & S`。 |
| 159 | `            } else {` | 否则…… |
| 160 | `                fprintf(f, "    andl    %s(%%rip), %%eax\n", a2);` | `andl 变量(%rip), %eax`：`%eax = %eax & 变量值`。 |
| 161 | `            }` | if-else 结束。 |
| 162 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将按位与结果写入目标变量。 |
| 163 | `            break;` | 跳出 switch。 |

---

#### OP_OR_OP — case 165-177（逻辑或 OR）

逻辑或操作的四元式为 `(or, a1, a2, r)`。语义：`r = a1 OR a2`。实现与 `AND` 完全相同，仅将 `andl` 换成 `orl`。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 165 | `        case OP_OR_OP:` | 匹配逻辑或操作码（枚举值 16）。 |
| 166 | `            if (is_const_operand(a1)) {` | 判断第一操作数是否为常数。 |
| 167 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 168 | `            } else {` | 否则…… |
| 169 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载到 `%eax`。 |
| 170 | `            }` | if-else 结束。 |
| 171 | `            if (is_const_operand(a2)) {` | 判断第二操作数是否为常数。 |
| 172 | `                fprintf(f, "    orl     $%s, %%eax\n", a2);` | `orl $值, %eax`：将 `%eax` 与立即数按位或。 |
| 173 | `            } else {` | 否则…… |
| 174 | `                fprintf(f, "    orl     %s(%%rip), %%eax\n", a2);` | `orl 变量(%rip), %eax`：`%eax = %eax | 变量值`。 |
| 175 | `            }` | if-else 结束。 |
| 176 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将结果写入目标变量。 |
| 177 | `            break;` | 跳出 switch。 |

---

#### OP_NOT_OP — case 179-189（逻辑非 NOT）

逻辑非操作的四元式为 `(not, a1, _, r)`。语义：`r = NOT a1`（若 a1 非零则结果为 0，若 a1 为零则结果为 1）。这是一个布尔取反操作。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 179 | `        case OP_NOT_OP:` | 匹配逻辑非操作码（枚举值 17）。命名为 `OP_NOT_OP` 以避免与 C++ 关键字或宏冲突。 |
| 180 | `            if (is_const_operand(a1)) {` | 判断操作数 `a1` 是否为常数。注意 `NOT` 只有一个操作数，`a2` 和 `r` 中只有 `r`（结果）有实际意义。 |
| 181 | `                fprintf(f, "    movl    $%s, %%eax\n", a1);` | 常数加载到 `%eax`。 |
| 182 | `            } else {` | 否则…… |
| 183 | `                fprintf(f, "    movl    %s(%%rip), %%eax\n", a1);` | 从变量加载操作数值到 `%eax`。 |
| 184 | `            }` | if-else 结束。 |
| 185 | `            fprintf(f, "    testl   %%eax, %%eax\n");` | **`testl %eax, %eax`**：将 `%eax` 与自身做按位与运算，但只设置标志位，不改变 `%eax` 的值。这是 x86 中测试一个值是否为零的最有效方式。`testl` 与 `andl` 的区别：`andl` 修改目标寄存器并设置标志位；`testl` 只设置标志位。效果等同于 `cmp $0, %eax`，但 `testl` 指令编码更短（2 字节 vs 3+ 字节）。具体地：若 `%eax==0` 则 ZF=1（零标志置位）；若 `%eax≠0` 则 ZF=0。 |
| 186 | `            fprintf(f, "    sete    %%al\n");` | **`sete %al`**：若 ZF=1（即 `%eax` 等于 0），则将 `%al` 设为 1；否则设为 0。这里体现了逻辑非的语义转换——"等于零"等价于"NOT 非零"。`sete` = "set if equal"，检查 ZF 标志，恰好对应 "之前的值等于零"。 |
| 187 | `            fprintf(f, "    movzbl  %%al, %%eax\n");` | **`movzbl %al, %eax`**：将 `%al` 零扩展到 `%eax`。因为 `sete` 只设置了 `%al`（8 位），而高 24 位可能包含之前运算的残余数据，需要清零以确保 `%eax` 是干净的 0 或 1。 |
| 188 | `            fprintf(f, "    movl    %%eax, %s(%%rip)\n", r);` | 将逻辑非结果（0 或 1）写入目标变量 `r`。 |
| 189 | `            break;` | 跳出 switch。 |

**`testl + sete` 为什么实现逻辑非**：
- `NOT a1` 的真值表：a1=0 → 结果=1；a1≠0 → 结果=0
- `testl %eax, %eax`：测试 `%eax` 是否为零 → 若为零则 ZF=1
- `sete %al`：ZF=1 时 `%al=1`（即 a1 为零 → 结果为 1 ✓）；ZF=0 时 `%al=0`（即 a1 非零 → 结果为 0 ✓）
- 这精确实现了布尔取反

---

#### OP_JE ~ OP_JGE — case 191-208（六种比较运算）

比较操作的四元式为 `(je/jl/..., a1, a2, r)`。语义：比较 a1 和 a2，将布尔结果（0 或 1）存入 r。所有六种比较全部委托给 `write_cmp_set()` 函数，传入对应的条件置位指令即可。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 191 | `        case OP_JE:` | `OP_JE`（枚举值 9）：等于比较 `a1 == a2`。注意此处的 `OP_JE`（比较并产生布尔结果）与 `OP_JMP` 不同——`OP_JE` 产生一个可赋值的布尔值，`OP_JMP` 是直接跳转。 |
| 192 | `            write_cmp_set(f, "sete", a1, a2, r);` | 委托给 `write_cmp_set`，传入 `"sete"`（set if equal）。若 `a1 == a2` 则 r=1，否则 r=0。 |
| 193 | `            break;` | 跳出 switch。 |
| 194 | `        case OP_JNE:` | `OP_JNE`（枚举值 10）：不等于比较 `a1 != a2`。 |
| 195 | `            write_cmp_set(f, "setne", a1, a2, r);` | `setne`（set if not equal）：若 `a1 != a2` 则 r=1。 |
| 196 | `            break;` | 跳出 switch。 |
| 197 | `        case OP_JL:` | `OP_JL`（枚举值 11）：小于比较 `a1 < a2`（有符号）。 |
| 198 | `            write_cmp_set(f, "setl", a1, a2, r);` | `setl`（set if less）：若 SF≠OF（有符号小于），则 r=1。标志位逻辑：`cmp a2, a1` 计算 `a1 - a2`，若结果为负且没有溢出（或为正但发生了溢出），则表示 `a1 < a2`。 |
| 199 | `            break;` | 跳出 switch。 |
| 200 | `        case OP_JG:` | `OP_JG`（枚举值 12）：大于比较 `a1 > a2`（有符号）。 |
| 201 | `            write_cmp_set(f, "setg", a1, a2, r);` | `setg`（set if greater）：若 ZF=0 且 SF=OF（有符号大于），则 r=1。条件为 `!ZF && SF==OF`。 |
| 202 | `            break;` | 跳出 switch。 |
| 203 | `        case OP_JLE:` | `OP_JLE`（枚举值 13）：小于等于比较 `a1 <= a2`（有符号）。 |
| 204 | `            write_cmp_set(f, "setle", a1, a2, r);` | `setle`（set if less or equal）：若 ZF=1 或 SF≠OF，则 r=1。 |
| 205 | `            break;` | 跳出 switch。 |
| 206 | `        case OP_JGE:` | `OP_JGE`（枚举值 14）：大于等于比较 `a1 >= a2`（有符号）。 |
| 207 | `            write_cmp_set(f, "setge", a1, a2, r);` | `setge`（set if greater or equal）：若 SF=OF，则 r=1。 |
| 208 | `            break;` | 跳出 switch。 |

**setX 指令与标志位对应关系**：

| setX 指令 | 条件 | 标志位逻辑 | 对应高级语义 |
|-----------|------|------------|------------|
| `sete` | equal | ZF=1 | `==` |
| `setne` | not equal | ZF=0 | `!=` |
| `setl` | less (signed) | SF≠OF | `<` |
| `setg` | greater (signed) | ZF=0 and SF=OF | `>` |
| `setle` | less or equal (signed) | ZF=1 or SF≠OF | `<=` |
| `setge` | greater or equal (signed) | SF=OF | `>=` |

标志位含义：**ZF** (Zero Flag) — 结果为零；**SF** (Sign Flag) — 结果为负；**OF** (Overflow Flag) — 有符号溢出。

---

#### OP_JMP — case 210-213（无条件跳转）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 210 | `        case OP_JMP:` | `OP_JMP`（枚举值 7）：无条件跳转。 |
| 211 | `            // (jmp, _, _, label)` | 注释：四元式格式——`a1` 和 `a2` 均为 `_`（未使用），`r`（结果字段）存放目标标号名称（如 `L1`、`L_end`）。 |
| 212 | `            fprintf(f, "    jmp     %s\n", r);` | 生成无条件跳转指令 `jmp 标号`。例如 `jmp L1`，CPU 将跳转到 `L1:` 标号处继续执行。`jmp` 是无条件直接跳转，不检查任何标志位。 |
| 213 | `            break;` | 跳出 switch。 |

---

#### OP_JNZ — case 215-224（条件非零跳转）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 215 | `        case OP_JNZ:` | `OP_JNZ`（枚举值 8）：若条件为真（非零）则跳转。用于实现 IF 语句的条件分支和 WHILE 循环的循环条件检查。 |
| 216 | `            // (jnz, cond, _, label)` | 注释：四元式格式——`a1` 是条件变量（布尔值 0/1），`a2` 为 `_`，`r` 是条件为真时的跳转目标标号。 |
| 217 | `            // * 如果 cond 是标签引用 (L*), 直接作为跳转目标` | 注释：说明一种特殊用法——如果条件操作数 `a1` 本身就是标号（以 `L` 开头），则直接作为跳转目标。但实际上这里走的是标准 `cmpl + jne` 路径，不会出现 "直接作为跳转目标" 的情况（这个注释可能是遗留的或描述其他模块的行为）。 |
| 218 | `            if (is_const_operand(a1)) {` | 判断条件变量 `a1` 是否为常数。如果条件本身是一个硬编码的常量（如 `jnz 1 _ L1`），则直接比较常数值 0。 |
| 219 | `                fprintf(f, "    cmpl    $0, $%s\n", a1);` | 与常数 0 比较：`cmpl $0, $常量`。注意这行有微妙之处——`cmpl` 的两个操作数都可以是立即数，但实际上这种用法不常见且可能不正确（`cmpl` 不能比较两个立即数）。在实际代码中，`a1` 几乎不会是常量条件。 |
| 220 | `            } else {` | 若条件变量是内存变量…… |
| 221 | `                fprintf(f, "    cmpl    $0, %s(%%rip)\n", a1);` | `cmpl $0, 变量(%rip)`：将变量值与 0 比较。例如 `cmpl $0, t1(%rip)`，检查 `t1` 是否为零。AT&T 语法 `cmpl S, D` 计算 `D - S`（即 `变量 - 0`），设置标志位。 |
| 222 | `            }` | if-else 结束。 |
| 223 | `            fprintf(f, "    jne     %s\n", r);` | `jne 标号`：若上一步 `cmpl` 发现两数不相等（即 `变量 ≠ 0`，变量非零），则跳转到标号 `r`。逻辑：`jne` = "jump if not equal" = "jump if ZF=0"。因为 `变量 ≠ 0` 等价于"变量为真"，符合 `jnz` 的语义。 |
| 224 | `            break;` | 跳出 switch。 |

**jnz 与 IF/WHILE 的关系**：解析器在生成 IF 语句代码时，先将比较结果（0 或 1）存入临时变量 `t`，然后生成 `jnz t _ L_then` 四元式。如果 `t` 为 1（比较成立），则跳转到 `L_then` 执行 then 分支；如果 `t` 为 0，则继续执行下一条指令（即 else 分支或跳过 then 块）。

---

#### OP_WRITE — case 226-233（printf 输出）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 226 | `        case OP_WRITE:` | `OP_WRITE`（枚举值 20）：输出操作，调用 C 标准库 `printf` 函数输出变量值。 |
| 227 | `            // (write, id, _, _)` | 注释：四元式格式——`a1` 是要输出的变量名，`a2` 和 `r` 未使用。 |
| 228 | `            // * printf("%d\n", id)` | 注释：生成的汇编等价于 C 代码 `printf("%d\n", id)`。 |
| 229 | `            fprintf(f, "    movl    %s(%%rip), %%esi\n", a1);` | **printf 第二个参数**：将变量值加载到 `%esi`（`%rsi` 的低 32 位）。x86-64 System V ABI 调用约定：整数参数依次使用 `%rdi, %rsi, %rdx, %rcx, %r8, %r9`。`printf` 的第二个参数（格式串中 `%d` 对应的整数值）通过 `%rsi`/`%esi` 传递。由于本编译器所有数据都是 32 位 int，用 `movl` 加载到 `%esi` 会自动将 `%rsi` 的高 32 位清零。 |
| 230 | `            fprintf(f, "    leaq    .fmt(%%rip), %%rdi\n");` | **printf 第一个参数**：用 `leaq`（加载有效地址，64 位）将格式字符串 `.fmt` 的地址加载到 `%rdi`。`leaq` 不访问内存，只计算地址表达式 `.fmt(%rip)` 的值（RIP 相对寻址算出 `.fmt` 的绝对地址），并将结果存入 `%rdi`。这是向 `printf` 传递格式字符串 `"%d\n"` 的标准方式。 |
| 231 | `            fprintf(f, "    xorl    %%eax, %%eax\n");` | **清零 `%eax`（设置 AL=0）**：x86-64 System V ABI 规定，调用变参函数（variadic functions，如 `printf`）时，`%al`（`%rax` 最低 8 位）必须设置为通过向量寄存器（XMM/YMM）传递的浮点参数个数。本编译器不传递任何浮点参数，所以将 `%eax` 清零（`xorl %eax, %eax` 将整个 32 位 `%eax` 置 0，同时清零 `%al`）。如果不设置 `%al`，`printf` 可能会错误地从向量寄存器读取参数，导致崩溃或输出乱码。 |
| 232 | `            fprintf(f, "    call    printf@PLT\n");` | **调用 `printf`**：`call printf@PLT` 通过过程链接表（Procedure Linkage Table）间接调用 `printf`。`@PLT` 后缀是 GAS 中用于动态链接的标记，让链接器生成通过 PLT 的延迟绑定调用。运行时第一次调用 `printf` 时，动态链接器会解析 `printf` 的实际地址并填充 PLT 条目；后续调用则直接跳转。这样可以减小可执行文件体积并加快启动速度。 |
| 233 | `            break;` | 跳出 switch。 |

**printf 调用约定总结（x86-64）**：
```
%rdi = 格式字符串地址（第一个参数）
%esi = 要输出的整数值（第二个参数，对应 %d）
%eax = 0（标识没有浮点变参，AL=0）
```
调用后 `printf` 将格式化的输出写入 stdout，返回值（写入的字符数）留在 `%eax` 中，但本编译器未使用该返回值。

---

#### OP_END — case 235-240（程序结束，epilogue）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 235 | `        case OP_END:` | `OP_END`（枚举值 18）：程序结束操作码。来自解析 `end` 关键字。 |
| 236 | `            // 程序出口` | 注释。 |
| 237 | `            fprintf(f, "    xorl    %%eax, %%eax\n");` | **将返回值设为 0**：`xorl %eax, %eax` 将 `%eax` 清零。在 x86-64 ABI 中，`%eax`/`%rax` 用于存放函数返回值。`main` 函数返回 0 表示程序正常退出（相当于 C 语言的 `return 0;`）。`xorl` 是一种比 `movl $0, %eax` 更高效的清零方式（指令编码更短，且不依赖立即数字段）。 |
| 238 | `            fprintf(f, "    leave\n");` | **恢复栈帧**：`leave` 指令等价于 `movq %rbp, %rsp; popq %rbp`。第一步将 `%rsp` 恢复到 `%rbp` 的位置（释放局部栈空间），第二步弹出原来压栈的 `%rbp` 值恢复调用者的帧指针。这是序言 `pushq %rbp; movq %rsp, %rbp` 的逆操作。 |
| 239 | `            fprintf(f, "    ret\n");` | **返回调用者**：`ret` 从栈中弹出返回地址（由 `call` 指令压入），并跳转到该地址。在 `main` 函数中，这会将控制权返回给 C 运行时库的 `__libc_start_main` 或 `_start`，由它们调用 `exit()` 结束进程。 |
| 240 | `            break;` | 跳出 switch。 |

---

#### OP_LABEL — case 242-245（汇编标号）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 242 | `        case OP_LABEL:` | `OP_LABEL`（枚举值 19）：标签/标号操作码，在汇编中生成一个跳转目标标号。 |
| 243 | `            // (label, Lx, _, _) → 汇编标签` | 注释：四元式格式——`a1` 存放标号名（如 `L1`、`L_loop`、`L_end`），`a2` 和 `r` 未使用。此四元式不生成指令，只在汇编中定义一个地址标号。 |
| 244 | `            fprintf(f, "%s:\n", a1);` | 输出标号定义 `L1:`。在 GAS 汇编中，以字母开头后跟冒号的标识符定义了一个代码地址标号。后续的 `jmp`、`jne` 等指令可以通过该标号名跳转到此处。 |
| 245 | `            break;` | 跳出 switch。 |

---

#### default — case 247-249（未支持操作码）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 247 | `        default:` | 默认分支，处理未知操作码（理论上不会发生，但作为防御性编程）。 |
| 248 | `            fprintf(f, "    # 未支持的操作码 %d\n", q->op);` | 输出一条汇编注释，标注未支持的操作码编号。这有助于调试——如果生成的 `.s` 文件中出现此注释，说明代码生成遗漏了某个操作码。 |
| 249 | `            break;` | 跳出 switch。 |
| 250 | `        }` | switch 语句结束。 |

---

### 循环收尾与文件结束

| 行号 | 代码 | 讲解 |
|------|------|------|
| 251 | `        fprintf(f, "\n");` | 每条四元式处理完毕后输出一个空行，分隔不同四元式的汇编代码块，提高 `.s` 文件可读性。 |
| 252 | `    }` | for 循环结束——所有四元式处理完成。 |
| 253 | （空行） | 空白行。 |
| 254 | `    fprintf(f, "    .size   main, .-main\n");` | **函数大小声明**：`.size main, .-main` 向汇编器声明 `main` 函数的代码大小。`.size` 伪指令的格式为 `.size symbol, expression`。这里 `.-main` 是一个地址表达式：`.` 表示当前汇编位置（汇编器维护的地址计数器），`main` 是函数入口地址。因此 `.-main` 计算的是从 `main` 标号处到当前位置的总字节数，即整个 `main` 函数的代码长度。这个信息被写入 ELF 符号表，供调试器（如 GDB）和性能分析工具使用。 |
| 255 | `    fclose(f);` | 关闭输出文件。写入的内容会被刷新到磁盘。 |
| 256 | `    printf("目标代码已生成: %s\n", outfile);` | 向标准输出打印成功消息，告知用户汇编文件已生成到哪个路径。 |
| 257 | `}` | `codegen_generate` 函数结束。 |

---

## 附录：x86-64 AT&T 语法关键约定

| 特性 | AT&T 语法 | Intel 语法 | 本模块对应代码 |
|------|-----------|------------|---------------|
| 操作数顺序 | `op S, D`（源在前，目标在后） | `op D, S`（目标在前，源在后） | 所有 `movl`/`addl` 等 |
| 寄存器标记 | `%rax`, `%eax`, `%al` | `rax`, `eax`, `al` | `%%eax` → `%eax` |
| 立即数标记 | `$5`, `$-3` | `5`, `-3` | `$%s`（`is_const_operand` 判断后加 `$`） |
| 内存寻址 | `位移(%基址)` 如 `x(%rip)` | `[基址+位移]` 如 `[rip+x]` | `%s(%%rip)` |
| 指令大小后缀 | `movl` (32-bit), `movq` (64-bit) | `mov` (由操作数推导) | `movl`, `addl`, `subl` 等 |
| 间接调用 | `call *%rax` | `call rax` | `call printf@PLT` |

## 附录：本模块使用的 x86-64 寄存器

| 寄存器 | 用途 | 出现位置 |
|--------|------|----------|
| `%eax` / `%rax` | 累加器，存放运算结果、返回值 | 几乎所有操作码 |
| `%al` | `%rax` 最低 8 位，`setX`/`movzbl` 使用 | `sete %al`, `movzbl %al, %eax` |
| `%ecx` | 临时寄存器，存放常数除数 | `OP_DIV` 的 `idivl` 除数中转 |
| `%edx` | 除法时存放被除数的高 32 位 | `cltd` 后 `%edx:%eax` |
| `%rsp` / `%rbp` | 栈指针 / 帧指针，管理栈帧 | `pushq %rbp`, `movq %rsp, %rbp`, `leave` |
| `%rdi` | 函数第一个参数（printf 格式串地址） | `leaq .fmt(%rip), %rdi` |
| `%esi` | 函数第二个参数（printf 要输出的值） | `movl x(%rip), %esi` |

## 附录：生成的汇编文件示例

对于源程序 `program test; var x, y : integer; begin x := 3; y := x + 5; write y end.`，生成的 `.s` 文件大致为：

```asm
    .section .rodata
.fmt:
    .string "%d\n"

    .section .bss
x:
    .zero 4
y:
    .zero 4

    .section .text
    .globl  main
    .type   main, @function
main:
    pushq   %rbp
    movq    %rsp, %rbp

    # :=     3      _      x
    movl    $3, %eax
    movl    %eax, x(%rip)

    # +      5      x      t1
    movl    $5, %eax
    addl    x(%rip), %eax
    movl    %eax, t1(%rip)

    # :=     t1     _      y
    movl    t1(%rip), %eax
    movl    %eax, y(%rip)

    # write  y      _      _
    movl    y(%rip), %esi
    leaq    .fmt(%rip), %rdi
    xorl    %eax, %eax
    call    printf@PLT

    # end    _      _      _
    xorl    %eax, %eax
    leave
    ret

    .size   main, .-main
```
