# 编译器源码逐行讲解

基于 Pascal 子集的小型编译器前端，将源程序编译为 x86-64 AT&T 汇编。

---

## 1. grammar.h（173 行）— 全局定义

所有模块共享的类型、枚举、结构体、常量。

| 行 | 讲解 |
|----|------|
| 1-2 | `#ifndef` / `#define` 头文件保护，防止重复包含 |
| 4-7 | 引入标准库：`ctype.h`(字符分类)、`stdio.h`(printf/fopen)、`stdlib.h`(malloc/atoi)、`string.h`(strcmp/memset) |
| 9-15 | 容量上限宏：符号表 256、常数表 256、四元式 1024、Token 2048、行/名字/字符串各自最大长度。全用静态数组，避免动态分配 |
| 17-60 | **Token 类型码枚举**：`TOK_EOF=0`（文件结束）、`TOK_ID=1`（标识符）、`TOK_CONST=2`（常数）。关键字从 3 开始（`TOK_PROGRAM=3`），恰好等于 `keys_table` 索引 + 3。界符 10-20，扩展关键字 IF/WHILE/etc 21-28，比较运算符 29-34，write 35 |
| 62-84 | **四元式操作码枚举**：`OP_PROGRAM=1` 标记程序入口，`OP_ASSIGN=2` 赋值，`OP_ADD/SUB/MUL/DIV` 3-6 算术运算，`OP_JMP/JNZ/JE~JGE` 7-14 控制流，`OP_AND_OP/OR_OP/NOT_OP` 15-17 逻辑运算（后缀 `_OP` 避免与 C 保留字冲突），`OP_END=18` 程序结束，`OP_LABEL=19` 标签，`OP_WRITE=20` 输出 |
| 86-90 | 符号种类（程序名/变量/临时变量）和类型码（integer=0 占 4 字节、real=1 占 8 字节、char=2 占 1 字节） |
| 92-98 | **Token 结构体**：`code` 类别码、`value` 符号表/常数表索引、`real_val/int_val` 常数值（冗余存储方便使用） |
| 100-107 | **Symbol 结构体**：`name` 标识符名、`type` 类型码、`kind` 种类、`offset` 活动记录偏移（声明顺序累加）、`len` 类型宽度 |
| 109-115 | **Quadruple 结构体**：`op` 操作码、`arg1/arg2/result` 三个操作数字符串（直接存字符串而非索引，方便 codegen 拼汇编） |
| 117-121 | **SemValue 结构体**：表达式分析返回值，`name` 存变量名或常数字符串，`is_temp` 标记是否是临时变量 |
| 123-155 | **Compiler 结构体**（全局上下文）：词法分析区（source/pos/ch/token/token_list）、符号表区（sym_table/const_table/sem_stack）、四元式区（quads/temp_count/label_count）、错误信息区。所有模块通过指针共享这一个结构体 |
| 159-171 | extern 声明三个外部表（keys_table/op_names/const_aut，定义在各自 .c 中），`type_len()` 内联函数（integer→4, real→8, char→1） |

---

## 2. scanner.h（24 行）— 词法分析器接口

| 行 | 讲解 |
|----|------|
| 1-3 | 头文件保护，引入 grammar.h |
| 5-17 | 模块说明注释：实现技术（查表识别关键字/界符、字母 DFA 识别标识符、常数自动机识别实数/科学计数法、双字符界符预读） |
| 19-22 | 4 个公开函数声明：`init` 加载源程序、`next_token` 单次取 Token、`scan_all` 扫描全部 Token、`dump_tokens` 格式化输出 |

---

## 3. scanner.c（246 行）— 词法分析器实现

| 行 | 讲解 |
|----|------|
| 1-3 | 引入 scanner.h、symbol.h（扫描时顺带填符号表）、math.h（pow） |
| 8-16 | **keys_table[]**：35 项统一表，`reserve()` 查表返回 `index+3`，所以 `"program"` 在索引 0 → 返回 3 → 正好等于 `TOK_PROGRAM`。最后 `NULL` 作哨兵 |
| 21-30 | **const_aut[8][5]**：Pascal 常数自动机状态转换矩阵。8 行 × 5 列（digit/dot/Ee/+-/other）。状态 1 初态、2 整数部分、3 刚读小数点、4 小数部分、5 刚读 E、6 刚读符号、7 指数、8 接受态。0 表示非法转换 |
| 33-38 | `next_char()`：从 source 中取下一个字符放入 `c->ch`，越界则置 `'\0'` |
| 41-44 | `skip_whitespace()`：跳过空格/制表/换行/回车 |
| 47-54 | `is_letter()` 和 `is_digit()`：字符分类（A-Za-z 是字母，0-9 是数字） |
| 57-64 | `reserve()`：在 keys_table 中线性查找字符串，找到返回 `i+3`（即 Token 码），未找到返回 0（表示是标识符） |
| 68-115 | `constant_automaton()`：Pascal 常数处理机。维护 4 个变量：`n`（尾数值，逐位累加 10n+digit）、`m`（小数位数计数）、`p`（指数值）、`e`（指数符号 ±1）。循环：判断当前字符类型 → 查 const_aut 矩阵 → 执行语义动作（各状态下更新 n/m/p/e）→ 遇到状态 8 时用 `num = n × 10^(e×p - m)` 计算最终值 |
| 117-124 | `scanner_init()`：保存源程序指针，计算长度，pos 归零，读第一个字符 |
| 127-207 | `scanner_next_token()`：主 Token 读取。①跳过空白；②`\0` 返回 EOF；③字母开头 → 连续读取字母/数字 → 调 reserve() 查表 → 0 则是标识符（调 sym_enter_id 填符号表）否则是关键字/界符；④数字开头 → 调 constant_automaton → 填常数表；⑤其它字符 → 双界符预读（`:=` `<=` `>=` `<>`）→ 拼字符串查表 → 未识别报错 |
| 210-223 | `scanner_scan_all()`：循环调用 next_token 直到 EOF，将每个 Token 存入 token_list[] |
| 226-246 | `scanner_dump_tokens()`：格式化输出 Token 序列，标识符显示 `(k,1)(i,索引)`，常数显示 `(c,索引)`，其它显示 `(p,类别码)`，每 8 个换行 |

---

## 4. symbol.h（26 行）— 符号表接口

| 行 | 讲解 |
|----|------|
| 15-24 | 公开函数：`init` 初始化、`enter_id` 查填标识符（线性查找 O(n)，未找到追加）、`lookup_id` 查找、`set_type` 修改类型（翻译文法 a6 用）、`enter_const/lookup_const` 常数查填（1e-12 容差）、`new_temp` 分配临时变量 t1~tN、`new_label` 分配标号 L1~LN、`dump/const_dump` 打印 |

---

## 5. symbol.c（125 行）— 符号表实现

| 行 | 讲解 |
|----|------|
| 1-5 | 引入 symbol.h、math.h（fabs），定义 type_names[] 数组用于打印 |
| 8-19 | `sym_init()`：清零所有计数器和表（sym_count/const_count/temp_count/label_count/sem_top=0，cur_type 默认 integer，cur_offset=0，memset 清空三张表） |
| 22-38 | `sym_enter_id()`：先线性遍历 sym_table[] 查找同名 → 找到直接返回索引；未找到则在尾部插入新条目（name/type/kind/offset/len） |
| 41-48 | `sym_lookup_id()`：纯查找，未找到返回 -1 |
| 51-56 | `sym_set_type()`：修改已存在符号的类型/宽度/偏移（翻译文法 a6 中用到） |
| 59-69 | `sym_enter_const()`：线性遍历常数表，fabs 容差 1e-12 判断相等 → 找到返回索引，否则追加 |
| 72-79 | `sym_lookup_const()`：纯常数查找 |
| 82-89 | `sym_new_temp()`：temp_count 自增，snprintf 生成名字 `"t1"`/`"t2"`...，同时在符号表中注册为 KIND_TEMP。注意 `static char buf[MAX_NAME]` 每次调用覆盖同一缓冲区，所以调用者必须立即使用 |
| 92-94 | `sym_new_label()`：返回递增的整数编号 |
| 97-113 | `sym_dump()`：用 snprintf 累加格式化输出符号表，含索引/名字/种类（程序/变量/临时）/类型名/偏移/宽度 |
| 116-125 | `const_dump()`：打印常数表（索引/值） |

---

## 6. quadruple.h（19 行）— 四元式接口

| 行 | 讲解 |
|----|------|
| 14-17 | 4 个函数：`init` 初始化、`emit` 生成一条（返回序号）、`dump` 打印、`backpatch` 回填跳转目标（当前未实际使用） |

---

## 7. quadruple.c（52 行）— 四元式实现

| 行 | 讲解 |
|----|------|
| 4-8 | `op_names[]` 操作码名称表，索引与 OP_ 枚举对应。索引 0 留空，1-20 依次为 program/:=/+/.../jmp/.../write |
| 11-16 | `quad_init()`：归零 quad_count/temp_count/label_count，memset 清空 quads 数组 |
| 19-27 | `quad_emit()`：核心函数。检查四元式计数上限 → 取当前槽位指针 → 填 op → 用 snprintf 写三个操作数（空串或 NULL 填 `"_"`）→ quad_count 后置自增返回旧值（即本条四元式的序号/索引） |
| 30-45 | `quad_dump()`：格式化输出四元式表格，4 列定宽：序号/操作名/arg1/arg2/result。操作码越界时显示 `?` |
| 48-52 | `quad_backpatch()`：将指定编号四元式的 result 字段替换为目标标签字符串（IF/WHILE 中生成跳转四元式时，目标在后续才确定，可用此回填） |

---

## 8. parser.h（22 行）— 语法分析器接口

| 行 | 讲解 |
|----|------|
| 5-17 | 说明注释：语句用递归下降子程序法（10 个子程序），表达式用 7 级优先级递归下降，语义动作用翻译文法 a1-a6 |
| 19-20 | 2 个公开函数：`init` 初始化、`parse` 执行编译（返回 1 成功/0 失败） |

---

## 9. parser.c（646 行）— 语法分析 + 语义分析实现

### 9.1 头部和 Token 预读（1-33 行）

| 行 | 讲解 |
|----|------|
| 1-21 | 引入头文件，文件头注释（递归下降子程序 → 文档对照表） |
| 22-23 | 两个静态全局变量：`peek` 预读 Token、`peek_valid` 标志 pek 是否有效。实现单 Token 预读机制 |
| 26-33 | `next_token()`：优先取 peek（消费后清零 peek_valid），否则调 scanner_next_token |

### 9.2 7 级优先级表达式分析（35-201 行）

| 行 | 讲解 |
|----|------|
| 44-51 | 8 个表达式函数声明（互递归），优先级低→高：or < and < not < 比较 < +- < */ < 因子 |
| 53-64 | `parse_or_expr()`：先 parse_and_expr，然后 `while TOK_OR` 循环：读下一个 Token → parse_and_expr 得右值 → 生成 `(or, left, right, t)` → 结果存临时变量 → 更新 sv 为 t |
| 66-77 | `parse_and_expr()`：同模式，处理 TOK_AND |
| 79-90 | `parse_not_expr()`：遇 TOK_NOT 则递归自身 → 生成 `(not, val, _, t)`；否则降级到 parse_comparison |
| 92-117 | `parse_comparison()`：先 parse_arith_expr 得左值 → 遇比较运算符（`=<>`）则 parse_arith_expr 得右值 → 查 switch 映射 TOK_EQ→OP_JE 等 → 生成比较四元式 → 结果存临时变量。注意：OP_JE 等不直接跳转，而是生成 0/1 布尔值 |
| 119-134 | `parse_arith_expr()`：先 parse_term，然后 `while +-` 循环处理左结合加减法 |
| 136-151 | `parse_term()`：同模式处理乘除，左结合 |
| 153-197 | `parse_factor()`（最底层）：①TOK_ID → 从符号表取名字写入 sv.name；②TOK_CONST → 从常数表取值转字符串；③TOK_LPAREN → parse_expression 递归 → 期望 `)`；④TOK_MINUS → 一元负号，生成 `(-, 0, factor, t)` |
| 199-201 | `parse_expression()`：入口，直接调 parse_or_expr（最低优先级） |

### 9.3 翻译文法 a1-a6（203-251 行）

| 行 | 讲解 |
|----|------|
| 203-217 | 注释说明 6 个语义动作对应 PPT 19-20 页的翻译文法 |
| 215-217 | `semantic_a1()`：cur_offset 归零（变量声明开始时重置偏移） |
| 219-224 | `semantic_a2()`：将当前 Token 的 value（符号表索引）压入 sem_stack（栈顶 sem_top++） |
| 226-236 | `semantic_a3/a4/a5()`：设置 cur_type 为 integer/real/char |
| 238-251 | `semantic_a6()`：FIFO 弹栈（从栈底 0 到栈顶 sem_top 遍历），对每个标识符调 sym_set_type 设置类型/宽度/偏移，偏移累加 `len`，最后 sem_top 归零清栈 |
| 实例 | 声明 `var a,b:integer;` 流程：a1(offset=0) → a2(push a) → a2(push b) → a3(cur_type=integer) → a6(遍历栈：a.type=integer, a.offset=0, a.len=4；b.type=integer, b.offset=4, b.len=4) |

### 9.4 递归下降子程序（253-597 行）

| 行 | 讲解 |
|----|------|
| 256-265 | 前向声明 10 个子程序 |

**PROGRAM（269-302 行）**：
- 期望 `program id SUB_PROGRAM .`
- 读 program → 读 id → 设符号表 kind=KIND_PROGRAM → 生成 `(program, name, _, _)` → SUB_PROGRAM → 读 `.` → 生成 `(end, _, _, _)`

**SUB_PROGRAM（306-309 行）**：依次调用 VARIABLE 和 COM_SENTENCE

**VARIABLE（314-337 行）**：
- `var id_seq : type ;` 或空（ε）
- 流程：a1(清零 offset) → ID_SEQUENCE → 读 `:` → TYPE → 读 `;` → a6(填符号表)

**ID_SEQUENCE（341-362 行）**：`id { , id }`，每遇一个 id 调用 a2 压栈

**TYPE（366-385 行）**：读 integer/real/char，分别调用 a3/a4/a5

**COM_SENTENCE（389-409 行）**：`begin STATEMENT { ; STATEMENT } end`

**STATEMENT（413-446 行）**：
- 根据当前 Token 分发：TOK_ID→EVA/赋值、TOK_IF→IF、TOK_WHILE→WHILE、TOK_WRITE→write（生成 `(write, id, _, _)`）、TOK_BEGIN→COM_SENTENCE（支持嵌套 begin-end）

**EVA_SENTENCE（451-472 行）**：
- `id := EXPRESSION`
- 读 id → 读 `:=` → parse_expression 得右值 → 生成 `(:=, expr_val, _, id)`

**IF_STATEMENT（487-540 行）**：
- `if EXPRESSION then STATEMENT [ else STATEMENT ]`
- 四元式模式：
  ```
  parse_expression → cond (结果在临时变量 t)
  (jnz, t, _, L_then)    // 条件真 → 跳 then
  (jmp, _, _, L_false)   // 否则 → 跳 else/出口
  (label, L_then, _, _)  // L_then:
  <then body>
  [jmp, _, _, L_end]     // 有 else 时跳过 else
  (label, L_false, _, _) // L_false:
  <else body>
  (label, L_end, _, _)   // L_end:
  ```

**WHILE_STATEMENT（554-597 行）**：
- `while EXPRESSION do STATEMENT`
- 四元式模式：
  ```
  (label, L_loop, _, _)  // L_loop:
  parse_expression → cond
  (jnz, cond, _, L_body) // 条件真 → 进入循环体
  (jmp, _, _, L_exit)    // 条件假 → 退出
  (label, L_body, _, _)  // L_body:
  <body>
  (jmp, _, _, L_loop)    // 无条件跳回入口
  (label, L_exit, _, _)  // L_exit:
  ```

### 9.5 公开接口（599-646 行）

| 行 | 讲解 |
|----|------|
| 601-604 | `parser_init()`：重置 peek_valid |
| 606-646 | `parser_parse()`（核心编译入口）：①先 scanner_scan_all 做词法分析 → Token 序列 + 符号表/常数表；②保存 token_count；③scanner_init 重新定位到源程序开头；④还原 token_count，清零 temp/label/quad/sem 计数（保留符号/常数表）；⑤next_token 读第一个 Token；⑥调 PROGRAM 开始递归下降；⑦检查 EOF，返回成功/失败 |

---

## 10. optimize.h（16 行）— 优化器接口

| 行 | 讲解 |
|----|------|
| 5-12 | 注释说明三种优化（常量折叠/传播/死代码消除） |
| 14 | 唯一公开函数 `optimize_run()` |

---

## 11. optimize.c（177 行）— 优化器实现

### 11.1 辅助函数（1-18 行）

| 行 | 讲解 |
|----|------|
| 5-12 | `is_int_const(s)`：检查字符串是否表示整数（支持负号），跳过 `_` 和空串 |
| 15-17 | `const_val(s)`：atoi 转整数 |

### 11.2 常量折叠（19-53 行）

| 行 | 讲解 |
|----|------|
| 25-53 | `fold_constants()`：遍历四元式，遇 `+`/`-`/`*`/`/` 且 arg1、arg2 都是整常数时，编译时计算 `a op b`（除零跳过），将操作码改为 `OP_ASSIGN`，arg1 改为计算结果，arg2 置 `_` |
| 示例 | `(*, 2, 5, t1)` → `(:=, 10, _, t1)` |

### 11.3 常量传播（55-94 行）

| 行 | 讲解 |
|----|------|
| 58-94 | `propagate_constants()`：找到 `(:=, C, _, var)` 形式的常数赋值 → 向后搜索该 var 的每次使用 → 在 `+`/`-`/`*`/`/` 运算中替换 arg1/arg2 为常数值。遇到重新赋值(a)、write(b)、end(b) 时停止传播 |
| 示例 | `(:=, 10, _, t1)` + 后续 `(+, t1, a, t2)` → `(+, 10, a, t2)` |

### 11.4 死代码消除（96-164 行）

| 行 | 讲解 |
|----|------|
| 99-132 | 第一遍：统计每个变量的使用次数。遍历所有四元式，对 WRITE/算术/赋值/JNZ 中出现在 arg1、arg2 位置的变量，在符号表对应索引的 use_count 累加 |
| 133-149 | 第二遍：找到赋值四元式，检查其 result 变量（在符号表中的索引）的使用次数是否为 0 且 kind 为 KIND_TEMP → 是则标记 op=-1 删除 |
| 150-163 | 第三遍：压缩四元式序列，将未被标记删除的四元式前移，更新 quad_count |

### 11.5 主入口（166-177 行）

| 行 | 讲解 |
|----|------|
| 167-177 | `optimize_run()`：至多 5 遍迭代，每遍依次执行 传播 → 折叠 → 消除，直至任一有变化。以当前例 sample1 为例：第 1 遍折叠 `2*5→10`，传播 `t1→10`，消除可能无用的中间四元式 |

---

## 12. codegen.h（21 行）— 代码生成器接口

| 行 | 讲解 |
|----|------|
| 5-17 | 注释说明每条四元式到汇编指令的映射关系（program→函数入口、assign→mov、算术→运算+mov、跳转→jmp/jne、比较→cmp+setX、write→printf 调用、end→leave/ret） |
| 19 | `codegen_generate(c, outfile)` 生成 .s 文件 |

---

## 13. codegen.c（257 行）— 代码生成器实现

| 行 | 讲解 |
|----|------|
| 4-8 | `is_const_operand(s)`：判断字符串是否为常数（以数字开头，或以 `-` 开头且后跟数字），不含 `_` 和空串 |
| 12-29 | `write_cmp_set(f, set_instr, a1, a2, r)`：比较操作辅助。①mov a1 到 %eax（常数前加 `$`，变量后加 `(%rip)`）；②cmpl a2 到 %eax；③`setX %al`（如 sete/setl/setg）→ ④movzbl 零扩展到 %eax → ⑤存到 result 变量。RIP 相对寻址是 x86-64 位置无关代码的标准写法 |
| 31-36 | 打开输出文件，失败则 stderr 报错 |
| 38-41 | `.section .rodata` 段：`.fmt` 标签指向 `"%d\n"` 格式串（printf 用） |
| 43-54 | `.section .bss` 段：遍历符号表，所有非 KIND_PROGRAM 的条目生成标签 + `.zero N`（分配 N 字节未初始化空间）。所有临时变量 t1/t2... 也会被分配（注意：无用的临时变量在死代码消除中会被删掉，但未删的仍在此分配） |
| 57-63 | `.section .text` 段开头：声明 `.globl main`、`.type main,@function`、`main:` 标签，标准 x86-64 帧指针函数序言 `pushq %rbp; movq %rsp,%rbp` |
| 66-252 | 遍历四元式数组，每条生成一行 `#` 注释 + 对应汇编。switch 分 20 种操作码处理：
| 77-79 | OP_PROGRAM：无操作（main 入口已在上面生成） |
| 81-89 | OP_ASSIGN：`movl src,%eax → movl %eax,dst(%rip)` |
| 92-103 | OP_ADD：`movl a1,%eax → addl a2,%eax → movl %eax,r(%rip)` |
| 106-117 | OP_SUB：同模式，用 subl |
| 120-131 | OP_MUL：同模式，用 imull（有符号乘）。立即数版用 `imull $imm, %eax, %eax` 三操作数形式 |
| 134-148 | OP_DIV：先 cltd 符号扩展 eax→edx:eax（idiv 前置要求），除数若是立即数需先 mov 到 ecx（idiv 不支持立即数操作数），再 `idivl %ecx` |
| 151-176 | OP_AND_OP / OP_OR_OP：andl / orl 运算 |
| 179-188 | OP_NOT_OP：testl %eax,%eax（置 ZF）→ sete %al（ZF=1 则 al=1，即 eax 为 0 则结果为 1）→ movzbl 零扩展 → 存结果 |
| 191-208 | OP_JE/OP_JNE/OP_JL/OP_JG/OP_JLE/OP_JGE：调 write_cmp_set，传入对应 set 指令（sete/setne/setl/setg/setle/setge） |
| 210-213 | OP_JMP：`jmp label` |
| 215-223 | OP_JNZ：`cmpl $0,cond(%rip)` → `jne label`。注意：当 cond 是立即数时生成 `cmpl $0,$N`（但实际 cond 永远不是常数） |
| 226-233 | OP_WRITE：`movl id(%rip),%esi`（第二个参数）→ `leaq .fmt(%rip),%rdi`（第一个参数，格式串地址）→ `xorl %eax,%eax`（AL 设为 0，表示没有浮点参数）→ `call printf@PLT` |
| 235-239 | OP_END：`xorl %eax,%eax`（返回值 0）→ `leave`（恢复 rbp/rsp）→ `ret`（返回） |
| 242-244 | OP_LABEL：`Lx:` 标签行 |
| 247-249 | default：不支持的操作码，生成注释说明 |
| 254 | `.size main,.-main` 声明函数大小（调试信息） |

---

## 14. main.c（355 行）— 主程序入口

### 14.1 头部（1-29 行）

| 行 | 讲解 |
|----|------|
| 1-27 | 文件头注释 + 引入全部 7 个头文件 + `<gtk/gtk.h>`（始终编译 GTK 代码） |
| 29 | 前向声明 `read_file()`（因为在 GUI 回调中用到） |

### 14.2 GTK3 图形界面（31-245 行）

| 行 | 讲解 |
|----|------|
| 35-38 | 全局变量：Compiler 上下文（GUI 模式共用）、5 个输出视图指针、parent_window、编译成功标志 |
| 40-60 | `on_open_clicked()`：弹出 GTK 文件选择对话框 → 选中后调 read_file 读文件 → 放入 src_view 文本区 |
| 62-131 | `on_compile_clicked()`：①重置 g_compiler；②读 src_view 文本；③调 parser_parse 编译；④成功则依次填充 Token 视图/符号表+常数表视图/四元式视图/汇编代码视图（调 optimize_run 优化 → codegen_generate 写到 `/tmp/opencode/compiler_output.s` → 读回显示）；⑤设置 g_compiled_ok 标志 |
| 133-176 | `on_gcc_clicked()`：①检查 g_compiled_ok；②popen 执行 `gcc -no-pie ... -o ...` 捕获输出；③成功则再 popen 执行 `/tmp/opencode/compiler_output` 捕获程序运行输出；④显示到 run_view |
| 178-189 | `create_output_area()`：创建带滚动条的只读等宽文本视图，附加到 Notebook 页 |
| 191-237 | `activate()`：GTK Application 激活函数。创建窗口 → 构建垂直布局 → 源代码输入区（等宽 200px 高）→ 按钮行（读取文件/编译/GCC 编译运行）→ 下半部分 Notebook（5 个标签页：Token/符号表+常数表/四元式/汇编代码/运行输出）→ show_all |
| 239-245 | `run_gtk()`：创建 GtkApplication → 连接 activate → 运行主循环 |

### 14.3 CLI 模式（247-355 行）

| 行 | 讲解 |
|----|------|
| 251-266 | `read_file()`：fopen → fseek(0,SEEK_END) 取文件大小 → fseek(0,SEEK_SET) 回跳 → malloc → fread 全读 → 末尾加 `\0` |
| 268-281 | `read_stdin()`：循环 fgets 读取，cap 不够时 realloc 扩容 |
| 283-287 | `main()` 入口：argc<2（无文件参数）→ run_gtk 启动 GUI |
| 289-292 | 创建 Compiler 局部变量，memset 清零，init 符号表和四元式 |
| 294-308 | 读源文件（argv[1] 或 stdin）→ 打印源程序 |
| 310-348 | 调 parser_parse 编译 → 成功则依次输出：Token 序列、符号表、常数表、优化前四元式、调 optimize_run、优化后四元式、调 codegen_generate 生成 .s 文件、打印 gcc 命令提示 |
| 353-354 | free 源程序，返回 0(成功) 或 1(失败) |

---

## 编译命令

**Linux（GTK3 GUI）：**
```bash
gcc $(pkg-config --cflags gtk+-3.0) src/*.c -o compiler $(pkg-config --libs gtk+-3.0) -lm
```

**Linux（纯 CLI，无 GTK 依赖）：**
```bash
gcc src/*.c -o compiler -lm
```

**交叉编译 Windows .exe：**
```bash
x86_64-w64-mingw32-gcc $(x86_64-w64-mingw32-pkg-config --cflags gtk+-3.0) \
  src/*.c -o compiler.exe $(x86_64-w64-mingw32-pkg-config --libs gtk+-3.0) -lm
```

## 运行

```bash
./compiler                         # GTK3 图形界面
./compiler test/sample1.txt        # CLI 编译 + 生成 .s 文件

# 汇编链接执行
gcc -no-pie test/sample1.txt.s -o s1 && ./s1
```
