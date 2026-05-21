## 一、源语言文法（BNF）

```
PROGRAM    → program id SUB_PROGRAM .
SUB_PROGRAM → VARIABLE COM_SENTENCE
VARIABLE   → var ID_SEQ : TYPE ; | ε
ID_SEQ     → id { , id }
TYPE       → integer | real | char
COM_SENTENCE → begin STATEMENT { ; STATEMENT } end
STATEMENT  → EVA | IF | WHILE | write id | COM_SENTENCE
EVA        → id := EXPRESSION
IF         → if EXPRESSION then STATEMENT [ else STATEMENT ]
WHILE      → while EXPRESSION do STATEMENT

EXPRESSION → or_expr
or_expr    → and_expr { or and_expr }
and_expr   → not_expr { and not_expr }
not_expr   → [not] comparison
comparison → arith_expr { rel_op arith_expr }
arith_expr → term { (+|-) term }
term       → factor { (*|/) factor }
factor     → id | const | ( EXPRESSION ) | - factor
```

## 二、词法分析

**方法**：有限自动机（DFA） + 关键字查表。

| 类别 | 识别方法 | 输出 Token 码 |
|------|---------|--------------|
| 标识符 | 字母开头，后跟字母或数字的 DFA | `TOK_ID (1)` |
| 常数 | 8 状态 Pascal 常数自动机（区分整数/实数/科学计数法） | `TOK_CONST (2)` |
| 关键字/界符 | 查 `keys_table[35]`，索引 + 3 = Token 码 | `TOK_PROGRAM(3) ~ TOK_WRITE(35)` |
| 双字符界符 | 预读一个字符（`:=` `<=` `>=` `<>`） | 查表得对应 Token 码 |

**Token 结构**：`(code, value)` — code 标识类别，value 指向符号表/常数表索引。

**常数自动机状态转换矩阵**（8 行 × 5 列）：

| 状态 | digit | dot | E/e | +/- | other |
|------|-------|-----|-----|-----|-------|
| 1 初态 | 2 | 0 | 0 | 0 | 0 |
| 2 整数 | 2 | 3 | 5 | 8(接受) | 8(接受) |
| 3 刚读. | 4 | 0 | 0 | 0 | 0 |
| 4 小数 | 4 | 0 | 5 | 8(接受) | 8(接受) |
| 5 刚读E | 7 | 0 | 0 | 6 | 0 |
| 6 刚读符号 | 7 | 0 | 0 | 0 | 0 |
| 7 指数 | 7 | 0 | 0 | 8(接受) | 8(接受) |

终值公式：`num = n × 10^(e × p − m)`，其中 n=尾数，m=小数位数，p=指数值，e=±1。

## 三、语法分析

**方法**：递归下降子程序法（自顶向下）。

每个非终结符实现为一个 C 函数，通过当前 Token 决定选择哪个产生式。无回溯，由 FIRST 集保证唯一匹配。

**表达式分析**采用 7 级优先级递归下降（自底向上归约效果）：
- L1: `or` → L2: `and` → L3: `not` → L4: 比较 `= <> < > <= >=`
- L5: `+ -` → L6: `* /` → L7: 因子 `id / const / (E) / unary -`

左递归通过 `while` 循环消除：先调用高优先级函数，再循环处理同级左结合运算符。

## 四、语义分析（翻译文法）

**方法**：语法制导翻译，在语法分析过程中嵌入语义动作，边解析边生成中间代码。

### 声明语句 — 翻译文法 a1~a6

| 动作 | 触发位置 | 操作 |
|------|---------|------|
| a1 | 读 `var` 后 | `cur_offset := 0` |
| a2 | 每读一个 id | `push(id.index)` 压语义栈 |
| a3 | 读 `integer` | `cur_type := integer, len := 4` |
| a4 | 读 `real` | `cur_type := real, len := 8` |
| a5 | 读 `char` | `cur_type := char, len := 1` |
| a6 | 读 `;` 后 | FIFO 弹栈，填符号表 type/len/offset，offset += len |

### 操作语句 — 生成四元式

| 语句 | 语义动作 | 四元式示例 |
|------|---------|-----------|
| `id := EXPR` | 解析表达式得结果 t，生成赋值 | `(:=, t, _, id)` |
| `write id` | 生成输出指令 | `(write, id, _, _)` |
| `if E then S1 else S2` | 生成比较 + 条件跳转 jnz + 标号 | `(jnz, t, _, L1)` `(jmp,_,_,L2)` |
| `while E do S` | 生成循环入口标号 + 条件跳转 + 回跳 | `(label, L0, _, _)` `(jmp,_,_,L0)` |

## 五、符号表系统

**数据结构**：顺序数组 `sym_table[MAX_SYMBOLS]`，线性查找（O(n)）。

**条目字段**：

| 字段 | 含义 | 取值 |
|------|------|------|
| name | 标识符名 | `"a"`, `"t3"`, `"example"` |
| kind | 种类 | `KIND_PROGRAM(0)` / `KIND_VARIABLE(1)` / `KIND_TEMP(2)` |
| type | 类型 | `TY_INTEGER(0) 4字节` / `TY_REAL(1) 8字节` / `TY_CHAR(2) 1字节` |
| offset | 活动记录偏移 | 按声明顺序 FIFO 分配，每变量递增 len |
| len | 类型宽度 | integer→4, real→8, char→1 |

**常数表**：独立 `const_table[MAX_CONSTANTS]`，存储所有字面常量值（双精度），查填容差 1e-12。

**临时变量**：`t1, t2, t3...` 随表达式求值按需分配，登记为 `KIND_TEMP`。

## 六、中间代码（四元式）

**格式**：`(op, arg1, arg2, result)` — 四地址指令。

| 操作码 | 格式 | 示例 |
|--------|------|------|
| `:=` | `(:=, src, _, dst)` | `(:=, 2, _, a)` |
| `+ − * /` | `(+, a1, a2, r)` | `(*, 2, 5, t1)` |
| `jmp` | `(jmp, _, _, label)` | 无条件跳转 |
| `jnz` | `(jnz, cond, _, label)` | 条件为真时跳转 |
| `je/jl/jg/jle/jge/jne` | `(je, a1, a2, r)` | 比较得 0/1 存入 r |
| `and/or/not` | 逻辑运算 | `(and, a, b, t)` |
| `label` | `(label, Lx, _, _)` | 汇编标号 |
| `write` | `(write, id, _, _)` | 输出语句 |
| `program` | `(program, name, _, _)` | 程序入口 |
| `end` | `(end, _, _, _)` | 程序出口 |

**设计优势**：与目标机器无关，便于优化和跨平台移植。操作数为字符串形式，codegen 可直接拼入汇编。

## 七、中间代码优化

**优化策略**：四元式级别优化（与目标平台无关），多遍迭代至不动点。

| 优化 | 方法 | 示例 |
|------|------|------|
| 常量折叠 | 编译时计算纯常数表达式 | `(*, 2, 5, t1)` → `(:=, 10, _, t1)` |
| 常量传播 | 将已知常数的变量在后续使用中替换为常数 | `(:=, 10, _, t1)` + `(+, t1, a, t2)` → `(+, 10, a, t2)` |
| 死代码消除 | 统计变量使用次数，删除无用的 `KIND_TEMP` 赋值 | 使用计数为 0 的临时变量赋值 → 标记删除 → 压缩序列 |

**迭代顺序**：传播 → 折叠 → 消除，因为传播和折叠可能互相触发新的优化机会。

## 八、目标代码生成

**方法**：四元式逐条翻译为 x86-64 AT&T 汇编。

| 四元式 | x86-64 汇编指令 | 说明 |
|--------|----------------|------|
| `(:=, C, _, var)` | `movl $C, %eax; movl %eax, var(%rip)` | 常数赋值 |
| `(+, a, b, t)` | `movl a(%rip), %eax; addl b(%rip), %eax; movl %eax, t(%rip)` | 加法 |
| `(*, a, C, t)` | `movl a(%rip), %eax; imull $C, %eax, %eax; movl %eax, t(%rip)` | 乘立即数 |
| `(/, a, b, t)` | `movl a(%rip), %eax; cltd; idivl b(%rip); movl %eax, t(%rip)` | 有符号除法 |
| `(jnz, t, _, L)` | `cmpl $0, t(%rip); jne L` | 条件跳转 |
| `(je, a, b, t)` | `cmp + sete + movzbl` | 比较得 0/1 |
| `(write, id, _, _)` | `movl id(%rip), %esi; leaq .fmt(%rip), %rdi; xorl %eax, %eax; call printf@PLT` | printf 调用 |
| `(end, _, _, _)` | `xorl %eax, %eax; leave; ret` | 函数出口 |

**输出格式**：GAS 标准 `.s` 汇编文件，可直接用 `gcc -no-pie` 汇编链接为可执行文件。

## 九、编译器工作流程总览

```
源程序(.txt)
    │  词法分析 [scanner.c]
    │  └─ keys_table 查关键字/界符
    │  └─ const_aut 常数自动机识别常数
    │  └─ 双字符界符预读
    ▼
Token 序列 + 符号表/常数表
    │  语法分析 [parser.c] — 递归下降
    │  └─ 语句层：10 个递归下降子程序
    │  └─ 表达式层：7 级优先级递归下降
    │  语义分析 [semantic.c]
    │  └─ 声明语句：a1~a6 翻译文法填符号表
    │  └─ 操作语句：边解析边生成四元式
    ▼
四元式序列（中间代码）
    │  优化 [optimize.c]
    │  └─ 常量折叠 → 常量传播 → 死代码消除 (多遍迭代)
    ▼
优化后四元式序列
    │  目标代码生成 [codegen.c]
    │  └─ 逐条翻译为 x86-64 AT&T 汇编
    ▼
.s 汇编文件 ──→ gcc 汇编链接 ──→ 可执行文件
```

## 十、编译命令

**Linux（GTK3 GUI）：**
```bash
gcc -DUSE_GTK $(pkg-config --cflags gtk+-3.0) src/*.c -o compiler $(pkg-config --libs gtk+-3.0) -lm
```

**Linux / Windows（纯 CLI）：**
```bash
gcc src/*.c -o compiler -lm                                # Linux
x86_64-w64-mingw32-gcc src/*.c -o compiler.exe -lm          # Windows 交叉编译
```

## 十一、运行

```bash
./compiler                        # GTK3 图形界面（仅 Linux GTK 版）
./compiler test/sample1.txt       # CLI 编译，生成 .s 文件

gcc -no-pie test/sample1.txt.s -o s1 && ./s1    # 汇编链接执行
```

## 十二、源文件结构

| 文件 | 环节 | 说明 |
|------|------|------|
| `grammar.h` | 全局定义 | Token 码、四元式操作码、符号表/Token/Quadruple/SemValue/Compiler 结构体 |
| `scanner.c/h` | 词法分析 | 关键字查表、常数自动机、双界符预读 |
| `symbol.c/h` | 符号表 | 标识符/常数查填、temp 分配、语义栈 |
| `parser.c/h` | 语法分析 | 递归下降（纯语法结构） |
| `semantic.c/h` | 语义分析 | a1~a6 翻译文法、表达式/IF/WHILE 四元式生成 |
| `quadruple.c/h` | 中间代码 | 四元式 emit/dump/backpatch |
| `optimize.c/h` | 优化 | 折叠/传播/死代码消除 |
| `codegen.c/h` | 目标代码 | x86-64 AT&T 汇编生成 |
| `main.c` | 入口 | CLI + GTK3 GUI |
