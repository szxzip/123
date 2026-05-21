# parser.c 逐行讲解

本文档对 parser.c 的 646 行代码进行逐行讲解，说明编译器语法分析器的实现细节。代码按功能分为若干小节省。

---

## 一、文件头——包含语句与模块注释（第1—20行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#include "parser.h"` | 引入解析器自己的头文件，其中声明了 `parser_init` 和 `parser_parse` 接口函数。 |
| 2 | `#include "scanner.h"` | 引入词法分析器接口。parser 需要调用 `scanner_next_token` 来驱动 Token 读取，也需要 `scanner_scan_all` 和 `scanner_init` 来实现两遍扫描架构。 |
| 3 | `#include "symbol.h"` | 引入符号表接口。parser 通过 `sym_new_temp` 分配临时变量、`sym_new_label` 分配标号、`sym_set_type` 填写符号表属性。 |
| 4 | `#include "quadruple.h"` | 引入四元式接口。parser 通过 `quad_emit` 向四元式列表中添加中间代码指令。 |
| 6—20 | `// ======...` 注释块 | 模块说明注释，列出了所有递归下降子程序与文档中 `Syntax::XXX()` 方法的对应关系，以及表达式分析和因子分析的说明。例如 `VARIABLE()` 除语法分析外，还实现了文档要求的 `a1~a6` 语义动作。 |

---

## 二、预读机制——peek/peek_valid 与 next_token()（第22—33行）

这是 parser 的词法驱动核心。parser 不直接从 scanner 流中取 Token，而是通过一个**单槽预读缓冲区** `peek` 配合标志位 `peek_valid`，实现向前看一个 Token。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 22 | `static Token peek;` | 静态全局变量——预读缓冲区。当 `peek_valid == 1` 时，该变量保存超前读取的 Token，还未被 parser 消费。类型 `Token` 包含 `code`（Token 码）、`value`（符号表/常数表索引）、`real_val`、`int_val` 四个字段。 |
| 23 | `static int peek_valid;` | 预读有效标志。为 1 表示 `peek` 中有一个未消费的 Token；为 0 表示 `peek` 为空，需要从 scanner 读取下一个 Token。初始值为 0，由 `parser_init` 设置。 |
| 26 | `static void next_token(Compiler *c) {` | 定义静态函数 `next_token`——供 parser 内各递归下降子程序调用，用于获取下一个 Token。它有两种行为模式，取决于 `peek_valid` 的值。 |
| 27 | `if (peek_valid) {` | **情况1：预读缓冲区有 Token。** 当 `peek_valid == 1` 时，说明上一次某处 `peek` 中已经存好了一个超前读取的 Token（例如在判断是否进入某个产生式时提前读了但未消费）。 |
| 28 | `c->token = peek;` | 直接从预读缓冲区取值，将 `peek` 复制到 `c->token`（编译器当前 Token），完成本次消费。 |
| 29 | `peek_valid = 0;` | 消费后将预读标志复位为 0。下一次调用 `next_token` 将走情况2，从 scanner 真实读取。 |
| 30 | `} else {` | **情况2：预读缓冲区为空。** 这是最常见的正常流程——parser 顺序消费 Token。 |
| 31 | `scanner_next_token(c);` | 调用词法分析器的 `scanner_next_token`，从源程序字符流中拆出下一个 Token 并存入 `c->token`。该函数内部会根据字符 DFA 识别标识符、常数、关键字、界符等，然后将 Token 码放入 `c->token.code`，将符号表/常数表索引放入 `c->token.value`。 |
| 33 | `}` | `next_token` 函数结束。注意该函数没有显式 return，只是通过调用方的上下文（`c->token`）传递读取到的 Token。 |

### 预读机制的作用

递归下降解析器在区分产生式时需要"向前看"一个 Token。例如：
- `VARIABLE → var ID_SEQUENCE : TYPE ; | ε`：读到 `var` 则进入，否则走空产生式。
- `STATEMENT` 中根据当前 Token 码分发到 `EVA_SENTENCE`（`TOK_ID`）、`IF_STATEMENT`（`TOK_IF`）、`WHILE_STATEMENT`（`TOK_WHILE`）、`TOK_WRITE` 等。
- 表达式运算符 after 读取：`while (c->token.code == TOK_OR)` 中的 `c->token` 已经通过上一次 `next_token` 提前读取。

实际上这个预读机制目前功能有限（`peek` 从未被写入），相当于 `next_token` 直接从 scanner 取 Token。设计上预留了扩展可能（backtracking）。

---

## 三、7级表达式分析——递归下降与算术表达式（第35—201行）

表达式解析采用**递归下降法**，每级优先级一个子程序。从低到高 7 级，每级：
- 先调下一级（更高优先级）的子程序解析左侧操作数
- 然后用 `while` 循环处理本级的**左结合**运算符（消除左递归）
- 每次遇到运算符，读取右侧操作数，分配临时变量，生成一条三地址四元式
- 将结果临时变量的名字回传给上层

所有表达式的"值"通过 `SemValue` 结构体向上传递，包含 `name`（变量名/临时变量名/常数字符串）和 `is_temp`（是否为临时变量标志）。

### 3.1 parse_expression — 表达式入口（第199—201行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 199 | `static void parse_expression(Compiler *c, SemValue *sv) {` | 表达式的最外层入口函数。所有需要表达式的地方（赋值右值、IF/WHILE 条件、write 参数等）都调用此函数开始解析。 |
| 200 | `parse_or_expr(c, sv);` | 直接委托给 `parse_or_expr`（优先级最低的层次，绑定最松的运算符 `or`）。整个表达式的语法规则可表示为：`EXPRESSION → or_expr`。因为在递归下降的优先级链里，`or_expr` 会向下调用 `and_expr`，`and_expr` 又会向下调用 `not_expr`……形成一条调用链，最终覆盖全部 7 级。 |
| 201 | `}` | `parse_expression` 函数结束。 |

### 3.2 parse_or_expr — L1：逻辑或 or（第53—64行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 54 | `parse_and_expr(c, sv);` | 先调用 `parse_and_expr` 解析左侧操作数（更高优先级的 `and` 表达式）。`sv` 将携带左侧的语义值（变量名或临时变量名）向上传递。 |
| 55 | `while (c->token.code == TOK_OR) {` | **消除左递归的 while 循环**。产生式 `E → E or T`（即 L1 包含 L2）是左递归的，递归下降无法直接处理。通过 `while` 将左递归转换为循环：`E → T { or T }`。当预读 Token 是 `or`（码值 27）时进入循环。 |
| 56 | `next_token(c);` | 消费 `or` 运算符，读取下一个 Token（应为 `and_expr` 的第一个 Token——运算符右侧）。 |
| 57 | `SemValue right;` | 声明局部变量 `right`，用于存放右侧操作数的语义值。 |
| 58 | `parse_and_expr(c, &right);` | 递归调用 `parse_and_expr` 解析右侧操作数。`right.name` 将包含右侧的变量名或临时变量名。 |
| 59 | `char *t = sym_new_temp(c);` | 调用符号表模块分配一个**临时变量**，返回其名称（形如 `T0`、`T1`、…）。临时变量用于存储中间计算结果，最终会被活动记录中某段偏移空间承载。 |
| 60 | `quad_emit(c, OP_OR_OP, sv->name, right.name, t);` | **生成逻辑或四元式**。格式：`(or, left_name, right_name, Tn)`。操作码 `OP_OR_OP`=16。含义：`Tn := left_name OR right_name`。 |
| 61 | `strncpy(sv->name, t, MAX_NAME - 1);` | 将结果临时变量的名字 `T0`/`T1`/… 复制到 `sv->name`。这一步使**结果值向上传递**——此处的临时变量成为上一级（如果有的话，比如嵌在 `and` 中）的操作数。 |
| 62 | `sv->is_temp = 1;` | 标记当前语义值为临时变量。当赋值语句 `id := expr` 的右值被解析完毕后，`sv.is_temp` 会影响代码生成策略（虽然最终都是传递 `sv->name`）。 |
| 63 | `}` | while 循环结束。如连续有多个 `a or b or c`，则循环多次：先算 `a or b` → `T0`，再算 `T0 or c` → `T1`，实现左结合语义。 |
| 64 | `}` | `parse_or_expr` 函数结束。 |

### 3.3 parse_and_expr — L2：逻辑与 and（第66—77行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 67 | `parse_not_expr(c, sv);` | 先调 `parse_not_expr` 解析左侧操作数。`and` 优先级高于 `or`，所以这里作为 `or_expr` 的下级。 |
| 68 | `while (c->token.code == TOK_AND) {` | 当预读 Token 是 `and`（码值 26）时进入循环。同样用 while 消除左递归。 |
| 69 | `next_token(c);` | 消费 `and` 运算符，读取右侧第一个 Token。 |
| 70 | `SemValue right;` | 声明右侧语义值。 |
| 71 | `parse_not_expr(c, &right);` | 调用 `parse_not_expr` 解析右侧 `and` 操作数。 |
| 72 | `char *t = sym_new_temp(c);` | 分配临时变量。 |
| 73 | `quad_emit(c, OP_AND_OP, sv->name, right.name, t);` | **生成逻辑与四元式**。格式：`(and, left_name, right_name, Tn)`。操作码 `OP_AND_OP`=15。 |
| 74 | `strncpy(sv->name, t, MAX_NAME - 1);` | 将结果临时变量名向上传递。 |
| 75 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 76 | `}` | while 循环结束。 |
| 77 | `}` | `parse_and_expr` 函数结束。 |

### 3.4 parse_not_expr — L3：一元逻辑非 not（第79—90行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 80 | `if (c->token.code == TOK_NOT) {` | 当预读 Token 是 `not`（码值 28）时进入**一元运算符**处理分支。注意 `not` 是前缀一元运算符，优先级高于 `and`，低于比较运算符。 |
| 81 | `next_token(c);` | 消费 `not`，读取操作数部分的首个 Token。 |
| 82 | `parse_not_expr(c, sv);` | 递归调用自身——支持多层 `not` 嵌套，例如 `not not a`。先解析内层 not/比较表达式，结果放入 `sv`。 |
| 83 | `char *t = sym_new_temp(c);` | 分配临时变量。 |
| 84 | `quad_emit(c, OP_NOT_OP, sv->name, "_", t);` | **生成逻辑非四元式**。格式：`(not, operand_name, _, Tn)`。操作码 `OP_NOT_OP`=17。第二个参数 `"_"` 表示未使用（一元运算符只有一个操作数）。 |
| 85 | `strncpy(sv->name, t, MAX_NAME - 1);` | 结果向上传递。 |
| 86 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 87 | `} else {` | 如果当前 Token 不是 `not`，则下降优先级。 |
| 88 | `parse_comparison(c, sv);` | 直接调用 `parse_comparison` 解析比较表达式。这体现了 `not` 作为可选项（`[not] comparison`）的语法结构。 |
| 89 | `}` | if-else 结束。 |
| 90 | `}` | `parse_not_expr` 函数结束。 |

### 3.5 parse_comparison — L4：比较运算符（第92—117行）

比较运算包括 `= <> < > <= >=` 六个关系运算符。比较结果不再是数值，而是一个布尔值，存放于临时变量中供后续条件跳转使用。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 93 | `parse_arith_expr(c, sv);` | 首先调用 `parse_arith_expr` 解析左侧比较对象（← L5 加减的语义值）。 |
| 94 | `if (c->token.code == TOK_EQ \|\| c->token.code == TOK_LT \|\|` | 判断当前 Token 是否是六个比较运算符之一。因为比较运算是可选的（`arith_expr [relop arith_expr]`），如果当前不是比较运算符，则 `sv` 保持不变，直接返回。 |
| 95 | `c->token.code == TOK_GT \|\| c->token.code == TOK_LE \|\|` | 继续检查 `>`（31）和 `<=`（32）。 |
| 96 | `c->token.code == TOK_GE \|\| c->token.code == TOK_NE) {` | 继续检查 `>=`（33）和 `<>`（34）。如果均不满足，不进入比较分支，此时 `sv` 携带的就是算术表达式的结果（可能用于 `or`/`and`/`not` 的布尔短路求值）。 |
| 97 | `int rel_op = c->token.code;` | 保存关系运算符的 Token 码，以便在 switch 中映射到对应的四元式操作码。 |
| 98 | `next_token(c);` | 消费该关系运算符。 |
| 99 | `SemValue right;` | 声明右侧语义值。 |
| 100 | `parse_arith_expr(c, &right);` | 调用 `parse_arith_expr` 解析右侧操作数（比较对象）。 |
| 101 | `char *t = sym_new_temp(c);` | 分配临时变量，用于存储比较的布尔结果（通常编码为 0/1）。 |
| 102 | `int quad_op;` | 声明四元式操作码变量。 |
| 103 | `switch (rel_op) {` | 将关系运算符的 Token 码映射为对应的四元式操作码。 |
| 104 | `case TOK_EQ: quad_op = OP_JE; break;` | `=` 对应比较-等于跳转 `OP_JE`（Jump if Equal）= 9。在四元式中表示"如果 a1==a2 则跳转"的语义先决条件。 |
| 105 | `case TOK_LT: quad_op = OP_JL; break;` | `<` 对应 `OP_JL`（Jump if Less）= 11。 |
| 106 | `case TOK_GT: quad_op = OP_JG; break;` | `>` 对应 `OP_JG`（Jump if Greater）= 12。 |
| 107 | `case TOK_LE: quad_op = OP_JLE; break;` | `<=` 对应 `OP_JLE`（Jump if Less or Equal）= 13。 |
| 108 | `case TOK_GE: quad_op = OP_JGE; break;` | `>=` 对应 `OP_JGE`（Jump if Greater or Equal）= 14。 |
| 109 | `case TOK_NE: quad_op = OP_JNE; break;` | `<>` 对应 `OP_JNE`（Jump if Not Equal）= 10。 |
| 110 | `default: quad_op = OP_JE; break;` | 防御性默认值，理论上不会到达（因为 if 条件已经过滤了所有 6 个比较符）。 |
| 112 | `// 比较操作生成四元式, 结果在临时变量` | 注释说明：比较操作本身**不是跳转指令**，而是产生一个存放布尔结果的临时变量。IF/WHILE 会使用这个临时变量配合 `OP_JNZ` 进行条件跳转。 |
| 113 | `quad_emit(c, quad_op, sv->name, right.name, t);` | **生成比较四元式**。例如 `(jg, a, b, T0)`。注意这里虽然操作码名叫 `OP_JG` 等带 `J`（Jump），但在实现中它们实际上把比较结果（布尔值 1/0）存入 `t`（result 域），而非直接跳转。真正的条件跳转由 IF_STATEMENT/WHILE_STATEMENT 中额外的 `OP_JNZ` 指令完成。 |
| 114 | `strncpy(sv->name, t, MAX_NAME - 1);` | 将结果临时变量名向上传递。后续 IF/WHILE 将引用这个临时变量的名字做条件判断。 |
| 115 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 116 | `}` | if 比较分支结束。如果当前 Token 不是比较运算符，则 `sv` 保持原样（来自 `parse_arith_expr` 的语义值）。 |
| 117 | `}` | `parse_comparison` 函数结束。 |

### 3.6 parse_arith_expr — L5：加减法 + -（第119—134行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 120 | `parse_term(c, sv);` | 先调用 `parse_term`（L6：乘除）解析左侧操作数。加减优先级低于乘除，所以乘除在这里作为下级被消化。 |
| 121 | `while (c->token.code == TOK_PLUS \|\| c->token.code == TOK_MINUS) {` | 当预读 Token 是 `+`（码值 16）或 `-`（码值 17）时，循环处理。用 while 消除左递归。连续遇到 `+`/`-` 时依次归约。 |
| 122 | `int op = c->token.code;` | 保存当前的运算符码（PLUS 或 MINUS），因为消费后 `c->token.code` 会变化。 |
| 123 | `next_token(c);` | 消费 `+` 或 `-`，读取右侧的 term。 |
| 124 | `SemValue right;` | 右侧语义值。 |
| 125 | `parse_term(c, &right);` | 调用 `parse_term` 解析右侧操作数。`right.name` 获取乘除结果或因子。 |
| 126 | `char *t = sym_new_temp(c);` | 分配结果临时变量。 |
| 127 | `if (op == TOK_PLUS)` | 根据运算符选择加法或减法四元式。 |
| 128 | `quad_emit(c, OP_ADD, sv->name, right.name, t);` | **生成加法四元式**：`(+, left, right, Tn)`。操作码 `OP_ADD`=3。 |
| 129 | `else` | |
| 130 | `quad_emit(c, OP_SUB, sv->name, right.name, t);` | **生成减法四元式**：`(-, left, right, Tn)`。操作码 `OP_SUB`=4。 |
| 131 | `strncpy(sv->name, t, MAX_NAME - 1);` | 结果向上传递。 |
| 132 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 133 | `}` | while 循环结束。 |
| 134 | `}` | `parse_arith_expr` 函数结束。 |

### 3.7 parse_term — L6：乘除法 * /（第136—151行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 137 | `parse_factor(c, sv);` | 先调用 `parse_factor`（L7：因子）解析左侧操作数。乘除优先级高于加减，低于因子。 |
| 138 | `while (c->token.code == TOK_MUL \|\| c->token.code == TOK_DIV) {` | 当预读 Token 是 `*`（码值 14）或 `/`（码值 15）时进入循环。while 消除左递归。 |
| 139 | `int op = c->token.code;` | 保存运算符码。 |
| 140 | `next_token(c);` | 消费运算符，读取右侧的 factor。 |
| 141 | `SemValue right;` | 右侧语义值。 |
| 142 | `parse_factor(c, &right);` | 调用 `parse_factor` 解析右侧因子（变量、常量、括弧表达式或一元负号）。 |
| 143 | `char *t = sym_new_temp(c);` | 分配临时变量。 |
| 144 | `if (op == TOK_MUL)` | 判断是乘法还是除法。 |
| 145 | `quad_emit(c, OP_MUL, sv->name, right.name, t);` | **生成乘法四元式**：`(*, left, right, Tn)`。操作码 `OP_MUL`=5。 |
| 146 | `else` | |
| 147 | `quad_emit(c, OP_DIV, sv->name, right.name, t);` | **生成除法四元式**：`(/, left, right, Tn)`。操作码 `OP_DIV`=6。 |
| 148 | `strncpy(sv->name, t, MAX_NAME - 1);` | 结果向上传递。 |
| 149 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 150 | `}` | while 循环结束。 |
| 151 | `}` | `parse_term` 函数结束。 |

### 3.8 parse_factor — L7：因子（第153—197行）

因子是表达式的最内层，处理原子成分：标识符、常数、括弧表达式和一元负号。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 154 | `if (c->token.code == TOK_ID) {` | **情况1：标识符**（变量名）。码值 1。 |
| 155 | `// 标识符 -> 返回其名称` | 注释说明处理策略：返回变量在符号表中的名字。 |
| 156 | `int idx = c->token.value;` | 提取 Token 的 value 字段，即该标识符在符号表 `sym_table[]` 中的索引。词法分析阶段已将每个标识符填入符号表。 |
| 157 | `if (idx >= 0 && idx < c->sym_count) {` | 索引合法性检查，确保索引在有效范围内。 |
| 158 | `strncpy(sv->name, c->sym_table[idx].name, MAX_NAME - 1);` | 从符号表取出该标识符的原始名字（如 `x`、`sum`），复制到语义值的 `name` 字段。 |
| 159 | `} else {` | 索引越界则使用备用命名方式。 |
| 160 | `snprintf(sv->name, MAX_NAME, "id%d", idx);` | 索引无效时生成形如 `id5`、`id12` 的名字。这是防御性措施，正常流程不会进入此分支。 |
| 162 | `sv->is_temp = 0;` | 标记为非临时变量——这是用户在源程序中定义的变量。 |
| 163 | `next_token(c);` | 消费该标识符 Token，读取下一个 Token。 |
| 164 | `} else if (c->token.code == TOK_CONST) {` | **情况2：常数**（字面量）。码值 2。 |
| 165 | `// 常数 -> 返回其字符串表示` | 注释说明处理策略：将常数转为字符串存储。 |
| 166 | `int idx = c->token.value;` | `c->token.value` 是常数在 `const_table[]` 中的索引。词法分析器将每个常数（整数/实数）的数值填入常数表。 |
| 167 | `if (idx >= 0 && idx < c->const_count) {` | 索引合法性检查。 |
| 168 | `snprintf(sv->name, MAX_NAME, "%g", c->const_table[idx]);` | 将常数表中的数值格式化为字符串存入语义值。例如常量 `3.14` → `sv->name` = `"3.14"`。最大值 `%g` 格式会自动选择定点或科学计数法。 |
| 169 | `} else {` | 索引无效时的备用方案。 |
| 170 | `snprintf(sv->name, MAX_NAME, "%d", idx);` | 直接用索引值作为名字。 |
| 172 | `sv->is_temp = 0;` | 标记为非临时变量（常数）。 |
| 173 | `next_token(c);` | 消费该常数 Token。 |
| 174 | `} else if (c->token.code == TOK_LPAREN) {` | **情况3：括弧表达式**。码值 19，左括号 `(`。 |
| 175 | `next_token(c);` | 消费 `(` 字符。 |
| 176 | `parse_expression(c, sv);` | 递归调用 `parse_expression`——进入一条全新的表达式解析调用链。这体现了括弧内可以包含任意复杂度的表达式，且优先级被重置为最低（括号内的 `or` 优先于括号外的 `*`）。 |
| 177 | `if (c->token.code == TOK_RPAREN)` | 期望匹配右括号 `)`（码值 20）。 |
| 178 | `next_token(c);` | 消费 `)` 字符，完成括弧匹配。 |
| 179 | `else {` | 缺少右括号——语法错误。 |
| 180 | `c->error_flag = 1;` | 置位错误标志。此标志会终止整个解析流程。 |
| 181 | `snprintf(c->error_msg, sizeof(c->error_msg), "语法错误: 期望 ')'");` | 记录错误信息：期望右括号。 |
| 182 | `}` | if 右括号匹配检查结束。 |
| 183 | `} else if (c->token.code == TOK_MINUS) {` | **情况4：一元负号** `-`。码值 17。此处与二元减区分：当 `-` 出现在因子开头位置时，它是一元运算符。 |
| 184 | `// 一元负号: -factor` | 注释说明语义：取反。 |
| 185 | `next_token(c);` | 消费 `-` 字符。 |
| 186 | `parse_factor(c, sv);` | 递归调用 `parse_factor` 解析负号的操作数（右结合）。注意此处递归调用的是 `parse_factor`（而非 `parse_expression`），保证 `- - a` 的二层取反被正确处理——先解析内层 `-a` 为一个因子，再对其应用外层负号。 |
| 187 | `char *t = sym_new_temp(c);` | 分配临时变量存放取反结果。 |
| 188 | `quad_emit(c, OP_SUB, "0", sv->name, t);` | **生成取负四元式**：`(-, 0, operand, Tn)`。语义：`Tn := 0 - operand`，即数学上的取负。操作码复用 `OP_SUB`=4。 |
| 189 | `strncpy(sv->name, t, MAX_NAME - 1);` | 结果向上传递。 |
| 190 | `sv->is_temp = 1;` | 标记为临时变量。 |
| 191 | `} else {` | **默认：语法错误**。当前 Token 既不是标识符、常数、`(`、也不是一元 `-`。 |
| 192 | `c->error_flag = 1;` | 置位错误标志。 |
| 193—194 | `snprintf(c->error_msg, ...)` | 记录错误信息："表达式语法错误: 意外的 Token %d"，将出错的 Token 码格式化到错误消息中。 |
| 195 | `snprintf(sv->name, MAX_NAME, "err");` | 将语义值的名字设为 `"err"`，避免返回未初始化数据导致后续代码崩溃。 |
| 196 | `}` | 错误处理结束。 |
| 197 | `}` | `parse_factor` 函数结束。 |

### 表达式解析调用链总结

```
parse_expression → parse_or_expr (L1: or)
                     → parse_and_expr (L2: and)
                        → parse_not_expr (L3: not, 一元)
                           → parse_comparison (L4: = <> < > <= >=)
                              → parse_arith_expr (L5: + -)
                                 → parse_term (L6: * /)
                                    → parse_factor (L7: id/const/(E)/-E)
```

**左递归消除**：L1、L2、L5、L6 使用 `while` 循环处理左结合运算符，将左递归产生式 `A → A op B` 转换为 `A → B { op B }`。

**优先级**：调用深度越深优先级越高。`parse_factor` 最深（优先级最高），`parse_or_expr` 最浅（优先级最低）。因为每级子程序首行都调用更高级的子程序，确保了先匹配优先级高的运算符。

---

## 四、语义动作 a1~a6——变量声明的翻译文法（第203—251行）

变量声明的语义分析采用**翻译文法**（Translation Grammar），将语义动作作为产生式的附加元素嵌入语法分析流程中。文档 PPT 第 19-20 页定义了 6 个语义动作 `a1~a6`。

### 4.1 全局上下文相关字段

这些字段在 `Compiler` 结构体（`grammar.h:143-144`）中定义：
- `sem_stack[]`：语义栈，FIFO 队列（数组模拟），用于暂存变量声明中的标识符索引
- `sem_top`：栈顶指针（数组有效元素个数）
- `cur_type`：当前声明的类型码（`TY_INTEGER`=0 / `TY_REAL`=1 / `TY_CHAR`=2）
- `cur_offset`：当前偏移量（在活动记录中分配连续偏移给变量）

### 4.2 semantic_a1 — 初始化偏移（第215—217行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 215 | `static void semantic_a1(Compiler *c) {` | **a1 动作**：初始化当前偏移量为 0。调用时机：VARIABLE 子程序一进入 `var` 后立即执行。 |
| 216 | `c->cur_offset = 0;` | 将活动记录偏移重置为 0。后续每个新声明的变量将从这个起始地址分配空间。每次遇到 `var` 关键字都会执行此动作，因为每个变量声明区段都有独立的偏移分配语义。 |
| 217 | `}` | `semantic_a1` 函数结束。 |

### 4.3 semantic_a2 — 压栈标识符（第219—224行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 219 | `static void semantic_a2(Compiler *c) {` | **a2 动作**：将当前标识符的符号表索引压入语义栈。调用时机：ID_SEQUENCE 中每解析到一个标识符时调用。 |
| 220 | `// a2: push(id_Token.val)` | 注释说明：将 `c->token.value`（标识符在符号表中的索引，词法分析阶段已填入）存入栈中。 |
| 221 | `if (c->sem_top < MAX_SYMBOLS) {` | 栈越界检查：确保 `sem_top` 不超过 `MAX_SYMBOLS`（256），防止数组越界。 |
| 222 | `c->sem_stack[c->sem_top++] = c->token.value;` | 后自增加入栈：将符号表索引存入 `sem_stack[sem_top]`，然后将 `sem_top` 加一。这是典型的数组模拟栈操作，`sem_top` 始终指向下一个空闲位置。 |
| 223 | `}` | 越界检查结束。 |
| 224 | `}` | `semantic_a2` 函数结束。 |

### 4.4 semantic_a3 — 整型类型设置（第226—228行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 226 | `static void semantic_a3(Compiler *c) {` | **a3 动作**：设置为整型。调用时机：TYPE 中解析到 `integer` 关键字时。 |
| 227 | `c->cur_type = TY_INTEGER;` | 将当前类型设置为整型（`TY_INTEGER`=0）。整型宽度 = 4 字节（由 `type_len()` 函数计算，定义于 `grammar.h:166`）。 |
| 228 | `}` | `semantic_a3` 函数结束。 |

### 4.5 semantic_a4 — 实型类型设置（第230—232行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 230 | `static void semantic_a4(Compiler *c) {` | **a4 动作**：设置为实型。调用时机：TYPE 中解析到 `real` 关键字时。 |
| 231 | `c->cur_type = TY_REAL;` | 将当前类型设置为实型（`TY_REAL`=1）。实型宽度 = 8 字节。 |
| 232 | `}` | `semantic_a4` 函数结束。 |

### 4.6 semantic_a5 — 字符类型设置（第234—236行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 234 | `static void semantic_a5(Compiler *c) {` | **a5 动作**：设置为字符型。调用时机：TYPE 中解析到 `char` 关键字时。 |
| 235 | `c->cur_type = TY_CHAR;` | 将当前类型设置为字符型（`TY_CHAR`=2）。字符型宽度 = 1 字节。 |
| 236 | `}` | `semantic_a5` 函数结束。 |

### 4.7 semantic_a6 — 弹栈填写符号表（FIFO）（第238—251行）

这是最关键的语义动作，完成了声明语义——将栈中所有标识符按声明顺序（FIFO：先进先出）填入符号表，分配偏移。

| 行号 | 代码 | 讲解 |
|------|------|------|
| 238 | `static void semantic_a6(Compiler *c) {` | **a6 动作**：将栈中缓存的标识符按声明顺序填入符号表。调用时机：VARIABLE 中解析到类型后的分号 `;` 时。 |
| 239 | `// a6: 按声明顺序填写符号表 (FIFO: 从栈底到栈顶)` | 注释说明 FIFO 机制：按 `sem_stack[0]` → `sem_stack[1]` → … → `sem_stack[sem_top-1]` 的顺序处理，即按声明顺序分配偏移。 |
| 240 | `int type = c->cur_type;` | 从上下文获取当前声明的类型（由 a3/a4/a5 之一设置）。 |
| 241 | `int len = type_len(type);` | 调用内联函数 `type_len` 计算该类型的宽度（整型=4、实型=8、字符=1）。 |
| 242 | `int i;` | 循环变量声明。 |
| 243 | `for (i = 0; i < c->sem_top; i++) {` | **FIFO 循环**：从栈底（索引 0）到栈顶（索引 `sem_top-1`）。因为是 `for(0 → sem_top-1)`，所以是先进先出。 |
| 244 | `int idx = c->sem_stack[i];` | 从语义栈取出当前标识符在符号表中的索引。 |
| 245 | `if (idx >= 0 && idx < c->sym_count) {` | 索引合法性校验。 |
| 246 | `sym_set_type(c, idx, type, len, c->cur_offset);` | **填充符号表**：将当前标识符的类型（type）、长度（len）、活动记录偏移（cur_offset）写入符号表条目。`sym_set_type` 内部会设置 `sym_table[idx].type`、`sym_table[idx].len`、`sym_table[idx].offset`。 |
| 247 | `c->cur_offset += len;` | 偏移递增：当前偏移加上该类型的宽度，准备给下一个声明的变量使用。例如声明 `a, b: integer;` 时，a 的偏移为 0（宽度 4），b 的偏移为 4。 |
| 248 | `}` | 索引校验结束。 |
| 249 | `}` | for 循环结束。 |
| 250 | `c->sem_top = 0;` | **清空语义栈**。将栈顶指针重置为 0，相当于弹出所有元素。为下一次 `var` 声明（如果有）做准备。 |
| 251 | `}` | `semantic_a6` 函数结束。 |

### 语义栈 FIFO 机制总结

`sem_stack[]` 实际上是一个**队列**（FIFO），因为：
- 入队：a2 每次压到 `sem_stack[sem_top++]`，按声明顺序排列
- 出队：a6 从 `i=0` 到 `sem_top-1` 遍历处理，先入栈的标识符先得到偏移

例如声明 `var a, b, c: integer;` 的处理过程：
1. `a1`：`cur_offset = 0`
2. 读 `a` → `a2`：`sem_stack[0] = a的索引`，`sem_top = 1`
3. 读 `b` → `a2`：`sem_stack[1] = b的索引`，`sem_top = 2`
4. 读 `c` → `a2`：`sem_stack[2] = c的索引`，`sem_top = 3`
5. 读 `:` → 读 `integer` → `a3`：`cur_type = TY_INTEGER`
6. 读 `;` → `a6`：
   - `i=0`：a 的 `offset = 0`，`cur_offset = 4`
   - `i=1`：b 的 `offset = 4`，`cur_offset = 8`
   - `i=2`：c 的 `offset = 8`，`cur_offset = 12`
   - `sem_top = 0`

---

## 五、递归下降子程序——语句解析（第253—472行）

### 5.1 前向声明（第256—265行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 256 | `static void PROGRAM(Compiler *c);` | 程序入口的前向声明，因为 `PROGRAM` 调用 `SUB_PROGRAM`，而 `SUB_PROGRAM` 定义在 `PROGRAM` 之后（304行），C 语言需要提前声明。 |
| 257—265 | `static void SUB_PROGRAM/.../WHILE_STATEMENT(...)` | 其他所有递归下降子程序的前向声明，共 9 个，构成完整的递归下降调用图。 |

### 5.2 PROGRAM — 程序入口（第269—302行）

产生式：`PROGRAM → program id SUB_PROGRAM .`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 270 | `if (c->token.code == TOK_PROGRAM) {` | **期望关键字 `program`**（码值 3）。如果第一个 Token 不是 `program`，整个解析失败。 |
| 271 | `next_token(c);` | 消费 `program`，读取下一个 Token（应为标识符——程序名）。 |
| 272 | `if (c->token.code == TOK_ID) {` | **期望标识符**：程序名必须是合法的标识符。 |
| 273 | `// 生成 (program, id, _, _)` | 注释说明：生成 `program` 四元式，标记程序入口。 |
| 274 | `int idx = c->token.value;` | 提取程序名在符号表中的索引。词法分析已将程序名填入符号表。 |
| 275—276 | `const char *name = (idx >= 0 && idx < c->sym_count) ? c->sym_table[idx].name : "?";` | 三元表达式：如果索引有效则取符号表中名字，否则用 `"?"` 兜底。 |
| 278 | `if (idx >= 0 && idx < c->sym_count) {` | 索引有效时将符号表中该条目标记为程序名。 |
| 279 | `c->sym_table[idx].kind = KIND_PROGRAM;` | 设置符号种类为 `KIND_PROGRAM`=0（区别于 `KIND_VARIABLE`=1 和 `KIND_TEMP`=2）。 |
| 281 | `next_token(c);` | 消费程序名标识符。 |
| 282 | `quad_emit(c, OP_PROGRAM, name, "_", "_");` | **生成程序入口四元式**：`(program, 程序名, _, _)`。操作码 `OP_PROGRAM`=1。这是四元式序列的第一条指令，标记程序开始。 |
| 283 | `SUB_PROGRAM(c);` | **递归调用**：进入 `SUB_PROGRAM`→`VARIABLE`→`COM_SENTENCE` 的完整子程序链，解析整个程序体（变量声明 + 复合语句）。 |
| 284 | `if (c->token.code == TOK_DOT) {` | **期望句号 `.`**（码值 18）。Pascal 语法中程序以 `.` 结尾。 |
| 285 | `next_token(c);` | 消费 `.`。 |
| 286 | `quad_emit(c, OP_END, "_", "_", "_");` | **生成程序结束四元式**：`(end, _, _, _)`。操作码 `OP_END`=18。这是四元式序列的最后一条指令。 |
| 287—289 | `} else { c->error_flag = 1; snprintf(...); }` | 如果没有句号——语法错误："语法错误: 期望 '.' 但得到 Token %d"。 |
| 290 | `}` | `if (TOK_ID)` 结束。 |
| 292 | `c->error_flag = 1;` | `program` 后不是标识符——语法错误。 |
| 293—296 | `snprintf(...)` | "语法错误: program 后期望标识符"。 |
| 297—300 | `} else { c->error_flag = 1; snprintf(...); }` | 第一个 Token 不是 `program`——语法错误："语法错误: 期望 'program'"。 |
| 302 | `}` | `PROGRAM` 函数结束。 |

### 5.3 SUB_PROGRAM — 子程序（第306—309行）

产生式：`SUB_PROGRAM → VARIABLE COM_SENTENCE`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 307 | `VARIABLE(c);` | 调用 `VARIABLE` 解析可能存在的变量声明区段。如果源程序中 `var` 不存在，`VARIABLE` 将直接返回（ε 产生式——空）。 |
| 308 | `COM_SENTENCE(c);` | 调用 `COM_SENTENCE` 解析复合语句。这是程序执行体，包含所有可执行语句。 |
| 309 | `}` | `SUB_PROGRAM` 函数结束。 |

### 5.4 VARIABLE — 变量声明（第314—337行）

产生式：`VARIABLE → var ID_SEQUENCE : TYPE ; | ε`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 315 | `if (c->token.code == TOK_VAR) {` | **判断是否有 `var` 关键字**（码值 4）。这是产生式分支的关键：有 `var` 则走变量声明流程，否则走 ε（空产生式）——即无变量声明，函数直接返回。 |
| 316 | `next_token(c);` | 消费 `var` 关键字，读取第一个标识符 Token。 |
| 317 | `semantic_a1(c);` | **执行 a1 动作**：初始化 `cur_offset = 0`。在开始解析标识符列表前重置偏移。 |
| 318 | `ID_SEQUENCE(c);` | 调用 `ID_SEQUENCE` 解析标识符序列。内部会多次调用 a2，将每个标识符索引压入语义栈。 |
| 319 | `if (c->token.code == TOK_COLON) {` | **期望冒号 `:`**（码值 11）。标识符序列后必须跟冒号。 |
| 320 | `next_token(c);` | 消费 `:`，准备读取类型关键字。 |
| 321 | `TYPE(c);` | 调用 `TYPE` 解析类型关键字（integer/real/char）。内部会调用 a3/a4/a5 设置 `cur_type`。 |
| 322 | `if (c->token.code == TOK_SEMICOLON) {` | **期望分号 `;`**（码值 12）。声明结尾。 |
| 323 | `semantic_a6(c);` | **执行 a6 动作**：按照 FIFO 顺序将栈中标识符填入符号表，分配偏移，最后清空语义栈。 |
| 324 | `next_token(c);` | 消费分号。变量声明解析完成。 |
| 325 | `} else {` | 缺少分号——语法错误。 |
| 326—328 | `c->error_flag = 1; snprintf(...)` | "语法错误: 变量声明后期望 ';'"。 |
| 330 | `} else {` | 缺少冒号——语法错误。 |
| 331—333 | `c->error_flag = 1; snprintf(...)` | "语法错误: 标识符表后期望 ':'"。 |
| 335 | `}` | `if (TOK_VAR)` 结束。 |
| 336 | `// 否则 VARIABLE -> ε, 无变量声明` | 注释说明：如果当前 Token 不是 `var`，本条产生式走空——源程序没有变量声明段，直接返回。 |
| 337 | `}` | `VARIABLE` 函数结束。 |

### 5.5 ID_SEQUENCE — 标识符序列（第341—362行）

产生式：`ID_SEQUENCE → id { , id }`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 342 | `if (c->token.code == TOK_ID) {` | **必须至少有一个标识符**。如果连第一个都没有，语法错误。 |
| 343 | `semantic_a2(c);` | **执行 a2 动作**：将第一个标识符的符号表索引压入语义栈。 |
| 344 | `next_token(c);` | 消费第一个标识符，读取下一个 Token。 |
| 345 | `while (c->token.code == TOK_COMMA) {` | **逗号重复循环**：当预读 Token 是逗号 `,`（码值 10）时，表示还有更多标识符待声明。用 while 循环处理 `{ , id }` 的重复。 |
| 346 | `next_token(c);` | 消费逗号，读取下一个 Token（应为标识符）。 |
| 347 | `if (c->token.code == TOK_ID) {` | 期望另一个标识符。 |
| 348 | `semantic_a2(c);` | **执行 a2 动作**：将该标识符索引压入语义栈。 |
| 349 | `next_token(c);` | 消费该标识符，继续检查是否有更多逗号。 |
| 350 | `} else {` | 逗号后不是标识符——语法错误。 |
| 351—355 | `c->error_flag = 1; snprintf(...); return;` | "语法错误: ',' 后期望标识符"。使用 `return` 直接退出函数，避免后续处理继续读取乱掉的 Token。 |
| 356 | `}` | if (TOK_ID after comma) 结束。 |
| 357 | `}` | while 循环结束。 |
| 358 | `} else {` | 第一个 Token 不是标识符——语法错误。 |
| 359—361 | `c->error_flag = 1; snprintf(...)` | "语法错误: 期望标识符"。 |
| 362 | `}` | `ID_SEQUENCE` 函数结束。 |

### 5.6 TYPE — 类型（第366—385行）

产生式：`TYPE → integer | real | char`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 367 | `switch (c->token.code) {` | 根据 Token 码分发到三种类型分支。 |
| 368 | `case TOK_INTEGER:` | 如果当前 Token 是 `integer`（码值 5）。 |
| 369 | `semantic_a3(c);` | **执行 a3 动作**：设置 `cur_type = TY_INTEGER`，宽度 = 4。 |
| 370 | `next_token(c);` | 消费 `integer` 关键字。 |
| 371 | `break;` | 跳出 switch。 |
| 372 | `case TOK_REAL:` | 如果当前 Token 是 `real`（码值 6）。 |
| 373 | `semantic_a4(c);` | **执行 a4 动作**：设置 `cur_type = TY_REAL`，宽度 = 8。 |
| 374 | `next_token(c);` | 消费 `real` 关键字。 |
| 375 | `break;` | 跳出 switch。 |
| 376 | `case TOK_CHAR:` | 如果当前 Token 是 `char`（码值 7）。 |
| 377 | `semantic_a5(c);` | **执行 a5 动作**：设置 `cur_type = TY_CHAR`，宽度 = 1。 |
| 378 | `next_token(c);` | 消费 `char` 关键字。 |
| 379 | `break;` | 跳出 switch。 |
| 380 | `default:` | 都不是已知类型名——语法错误。 |
| 381—384 | `c->error_flag = 1; snprintf(...)` | "语法错误: 期望类型名 (integer/real/char)"。 |
| 385 | `}` | `TYPE` 函数结束。 |

### 5.7 COM_SENTENCE — 复合语句（第389—409行）

产生式：`COM_SENTENCE → begin STATEMENT { ; STATEMENT } end`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 390 | `if (c->token.code == TOK_BEGIN) {` | **期望 `begin` 关键字**（码值 8）。复合语句以 `begin` 开头。 |
| 391 | `next_token(c);` | 消费 `begin`，读取复合语句的第一个语句。 |
| 392 | `STATEMENT(c);` | 调用 `STATEMENT` 解析第一个语句。这是必须的至少一个语句（不能 `begin end` 空语句序列）。 |
| 393 | `while (c->token.code == TOK_SEMICOLON) {` | **分号循环**：当预读 Token 是分号 `;`（码值 12）时，说明还有后续语句。用 while 处理 `{ ; STATEMENT }` 的重复。 |
| 394 | `next_token(c);` | 消费分号，准备读取下一个语句。 |
| 395 | `STATEMENT(c);` | 调用 `STATEMENT` 解析后续语句。 |
| 396 | `}` | while 循环结束。 |
| 397 | `if (c->token.code == TOK_END) {` | **期望 `end` 关键字**（码值 9）。复合语句必须以 `end` 结束。 |
| 398 | `next_token(c);` | 消费 `end`。 |
| 399—402 | `} else { c->error_flag = 1; snprintf(...); }` | 缺少 `end`——语法错误："语法错误: 期望 'end'"。 |
| 404—408 | `} else { c->error_flag = 1; snprintf(...); }` | 缺少 `begin`——语法错误："语法错误: 期望 'begin'"。 |
| 409 | `}` | `COM_SENTENCE` 函数结束。 |

### 5.8 STATEMENT — 语句分发（第413—446行）

产生式（扩展版）：`STATEMENT → EVA_SENTENCE | IF_STATEMENT | WHILE_STATEMENT | write id | COM_SENTENCE | ε`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 414 | `switch (c->token.code) {` | **根据首 Token 码分发**——LL(1) 的核心：通过当前 Token 码确定属于哪条产生式。 |
| 415 | `case TOK_ID:` | 首 Token 是标识符（码值 1），只能是赋值语句。 |
| 416 | `EVA_SENTENCE(c);` | 调用 `EVA_SENTENCE` 解析赋值。产生式：`EVA → id := EXPRESSION`。 |
| 417 | `break;` | 跳出 switch。 |
| 418 | `case TOK_IF:` | 首 Token 是 `if`（码值 21）。 |
| 419 | `IF_STATEMENT(c);` | 调用 `IF_STATEMENT` 解析条件语句。 |
| 420 | `break;` | 跳出 switch。 |
| 421 | `case TOK_WHILE:` | 首 Token 是 `while`（码值 24）。 |
| 422 | `WHILE_STATEMENT(c);` | 调用 `WHILE_STATEMENT` 解析循环语句。 |
| 423 | `break;` | 跳出 switch。 |
| 424 | `case TOK_WRITE:` | 首 Token 是 `write`（码值 35）。这是一个扩展的输出语句。 |
| 425 | `next_token(c);` | 消费 `write` 关键字，期望读取被输出的变量名。 |
| 426 | `if (c->token.code == TOK_ID) {` | 期望标识符（要输出的变量）。 |
| 427 | `int idx = c->token.value;` | 获取标识符的符号表索引。 |
| 428—429 | `const char *id_name = (idx >= 0 && idx < c->sym_count) ? c->sym_table[idx].name : "?";` | 三元表达式获取变量名。 |
| 430 | `quad_emit(c, OP_WRITE, id_name, "_", "_");` | **生成输出四元式**：`(write, 变量名, _, _)`。操作码 `OP_WRITE`=20。语义：输出该变量的值。 |
| 431 | `next_token(c);` | 消费变量名 Token。 |
| 432 | `// 分号由 COM_SENTENCE 的 while 循环处理` | 注释说明：write 语句后面的分号不由这里处理，而是由上层 `COM_SENTENCE` 的 `while (TOK_SEMICOLON)` 循环统一处理。 |
| 433—437 | `} else { c->error_flag = 1; snprintf(...); }` | `write` 后不是标识符——语法错误："语法错误: write 后期望标识符"。 |
| 438 | `break;` | 跳出 switch。 |
| 439 | `case TOK_BEGIN:` | 首 Token 是 `begin`（码值 8）。这意味着嵌套的复合语句。 |
| 440 | `COM_SENTENCE(c);` | 递归调用 `COM_SENTENCE`。因为 `COM_SENTENCE` 本身以 `begin` 开头，这里直接再进入一条复合语句，支持多层嵌套。 |
| 441 | `break;` | 跳出 switch。 |
| 442 | `default:` | 无法匹配任何已知的语句首 Token。可能是空语句（额外的分号）或语法错误。 |
| 443 | `// 空语句或错误` | 注释说明：默认情况下什么也不做，相当于空语句——容忍多余的 `;` 不报错。 |
| 444 | `break;` | 跳出 switch。 |
| 445 | `}` | switch 结束。 |
| 446 | `}` | `STATEMENT` 函数结束。 |

### 5.9 EVA_SENTENCE — 赋值语句（第450—472行）

产生式：`EVA_SENTENCE → id := EXPRESSION`

| 行号 | 代码 | 讲解 |
|------|------|------|
| 452 | `if (c->token.code == TOK_ID) {` | **期望赋值目标标识符**（码值 1）。因为 `STATEMENT` 中已通过首 Token 确认它是赋值语句的开头，所以此处的 if 理论上总是成立的。 |
| 453 | `int idx = c->token.value;` | 提取赋值目标在符号表中的索引。 |
| 454—455 | `const char *id_name = (idx >= 0 && idx < c->sym_count) ? c->sym_table[idx].name : "?";` | 三元表达式获取变量名，用于四元式中的目标字段。 |
| 456 | `next_token(c);` | 消费标识符 Token。 |
| 457 | `if (c->token.code == TOK_ASSIGN) {` | **期望赋值符号 `:=`**（码值 13）。 |
| 458 | `next_token(c);` | 消费 `:=`，准备解析右值表达式。 |
| 459 | `SemValue sv;` | 声明局部语义值变量，用于接收表达式分析的结果。 |
| 460 | `parse_expression(c, &sv);` | 调用 `parse_expression` 解析赋值右值表达式。这启动了整个 7 级表达式递归下降链，将结果（变量名或临时变量名）存入 `sv.name`。 |
| 461 | `quad_emit(c, OP_ASSIGN, sv.name, "_", id_name);` | **生成赋值四元式**：`(:=, 右值名字, _, 目标变量名)`。操作码 `OP_ASSIGN`=2。第二个参数 `"_"` 表示未使用（赋值只有单个数据源）。 |
| 462—465 | `} else { c->error_flag = 1; snprintf(...); }` | 标识符后不是 `:=`——语法错误："语法错误: 赋值语句中期望 ':='"。 |
| 467—471 | `} else { c->error_flag = 1; snprintf(...); }` | 不是标识符（防御性）——语法错误："语法错误: 赋值语句中期望标识符"。 |
| 472 | `}` | `EVA_SENTENCE` 函数结束。 |

### 赋值语句的四元式模式

```
(:=, 表达式结果变量/常数值, _, 目标变量名)
```

例如 `x := a + b * 3` 生成的四元式序列：
```
(*, b, 3, T0)       // parse_term 中的乘法
(+, a, T0, T1)      // parse_arith_expr 中的加法
(:=, T1, _, x)      // EVA_SENTENCE 中的赋值
```

---

## 六、IF_STATEMENT——条件语句的完整代码生成（第485—540行）

产生式：`IF_STATEMENT → if EXPRESSION then STATEMENT [ else STATEMENT ]`

IF 语句是编译器中代码生成最复杂的部分之一，涉及 3 个标号（label_true、label_false、label_end）和多个跳转四元式。核心思路：
1. 计算条件表达式 → 得到布尔结果临时变量
2. 用 `jnz` 跳转到 then 分支（条件为真）
3. 用 `jmp` 跳转到 else 分支/出口（条件为假）
4. 在 then 分支后，如有 else，用 `jmp` 跳过 else 体
5. 出口标签 `label_end` 标记 IF 语句结束

### 两种情况对比

**有 else：**
```
    jnz cond _ L_then        ← 条件真 → then
    jmp _ _ L_false          ← 条件假 → else
L_then:
    <then body>
    jmp _ _ L_end            ← then 结束跳过 else
L_false:
    <else body>
L_end:
```

**无 else：**
```
    jnz cond _ L_then        ← 条件真 → then
    jmp _ _ L_false          ← 条件假 → 出口（L_false 直接作为出口）
L_then:
    <then body>
L_false:                     ← 作为出口标签使用
```

| 行号 | 代码 | 讲解 |
|------|------|------|
| 486 | `char buf[MAX_STR];` | 声明格式化字符串缓冲区（MAX_STR=64）。后续反复用 `snprintf` 生成形如 `"L0"`、`"L1"` 的标签字符串。 |
| 488 | `next_token(c);` | 消费 `if` 关键字。由 `STATEMENT` 已确认当前 Token 是 `if`，这里消费它并读取条件表达式的首个 Token。 |
| 490—491 | `SemValue cond;` | 声明语义值变量 `cond`，用于接收条件表达式的结果（临时变量名）。 |
| 492 | `parse_expression(c, &cond);` | **解析条件表达式**。这启动了完整的 7 级表达式递归下降链。结果存入 `cond.name`（临时变量名，如 `T0`），该临时变量存储的是条件表达式的布尔值结果。在 L4 的比较层，比较运算符会生成形如 `(jg, a, b, T0)` 的四元式。 |
| 494 | `int label_true = sym_new_label(c);` | 分配**真标签**（L_then）。调用 `sym_new_label` 从编译器上下文的 `label_count` 分配一个新的数字标签。`label_true` 存储的是一个整数（0, 1, 2, …）。 |
| 495 | `int label_false = sym_new_label(c);` | 分配**假标签**（L_false）。另一个独立的整数标签编号。 |
| 496 | `int label_end = sym_new_label(c);` | 分配**结束标签**（L_end）。第三个独立标签编号。三个标签互不重复。 |
| 498—499 | `snprintf(buf, MAX_STR, "L%d", label_true);` | 格式化真标签字符串，例如 `label_true=3` → `buf = "L3"`。 |
| 500 | `quad_emit(c, OP_JNZ, cond.name, "_", buf);` | **生成条件真跳转**：`(jnz, 条件变量名, _, L_then)`。操作码 `OP_JNZ`=8。语义：如果条件变量 `cond.name` 非零（即为 true），则跳转到标签 L_then 执行 then 分支。 |
| 502—503 | `snprintf(buf, MAX_STR, "L%d", label_false);` | 格式化假标签字符串。 |
| 504 | `quad_emit(c, OP_JMP, "_", "_", buf);` | **生成无条件跳转**：`(jmp, _, _, L_false)`。操作码 `OP_JMP`=7。语义：如果 `jnz` 没有跳转（即条件为假），则继续执行到此 `jmp` 指令，跳转到 else 分支标签（或出口标签）。 |
| 506—507 | `snprintf(buf, MAX_STR, "L%d", label_true);` | 再次格式化真标签字符串。 |
| 508 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送标签四元式**：`(label, L_then, _, _)`。操作码 `OP_LABEL`=19。这表示 L_then 标签的位置——跳转指令（jnz）的目标。 |
| 510 | `if (c->token.code == TOK_THEN) {` | **期望 `then` 关键字**（码值 22）。条件表达式后必须跟 `then`。 |
| 511 | `next_token(c);` | 消费 `then`。 |
| 512 | `STATEMENT(c);` | 调用 `STATEMENT` 解析 then 分支体。递归下降可解析任意复杂度的 then 体（包括嵌套的 if、while、begin-end 等）。 |
| 514 | `if (c->token.code == TOK_ELSE) {` | **判断是否有 else 分支**（码值 23）。这是产生式的可选部分 `[else STATEMENT]`。 |
| 515 | `next_token(c);` | 消费 `else` 关键字。 |
| 517—518 | `snprintf(buf, MAX_STR, "L%d", label_end);` | 格式化结束标签字符串。 |
| 519 | `quad_emit(c, OP_JMP, "_", "_", buf);` | **then 结束跳转**：`(jmp, _, _, L_end)`。语义：then 分支执行完毕后，跳过 else 分支，直接到 L_end 标签（IF 语句的出口）。这避免了执行完 then 后又执行 else。 |
| 521—522 | `snprintf(buf, MAX_STR, "L%d", label_false);` | 格式化假标签字符串。 |
| 523 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送 L_false 标签**：`(label, L_false, _, _)`。此时开始 else 分支体。 |
| 525 | `STATEMENT(c);` | 调用 `STATEMENT` 解析 else 分支体。 |
| 527—528 | `snprintf(buf, MAX_STR, "L%d", label_end);` | 格式化结束标签。 |
| 529 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送 L_end 标签**：`(label, L_end, _, _)`。标记 IF 语句结束，后续代码从此处继续。 |
| 530 | `} else {` | **无 else 分支的情况**。 |
| 531 | `// 无 else: L_false 即为出口` | 注释说明：没有 else 时，L_false 标签直接充当出口。不需要额外的 `jmp` 跳过 else。 |
| 532—533 | `snprintf(buf, MAX_STR, "L%d", label_false); quad_emit(...)` | **发送 L_false 标签**。此时 L_false 既作为条件假时的跳转目标（第 504 行的 `jmp L_false`），也作为整个 IF 语句的出口标签。 |
| 534 | `}` | 有/无 else 的分支结束。 |
| 535—539 | `} else { c->error_flag = 1; snprintf(...); }` | 没有 `then` 关键字——语法错误："语法错误: if 后期望 'then'"。 |
| 540 | `}` | `IF_STATEMENT` 函数结束。 |

### IF 的完整四元式序列示例

假设源程序为 `if a > b then x := 1 else x := 2`：

```
第N条: (jg, a, b, T0)        ← parse_comparison 生成的比较结果
第N+1: (jnz, T0, _, L0)      ← 条件真 → L0 (then)
第N+2: (jmp, _, _, L1)       ← 条件假 → L1 (else)
第N+3: (label, L0, _, _)     ← L_then 标签
第N+4: (:=, 1, _, x)         ← then 体
第N+5: (jmp, _, _, L2)       ← 跳过 else
第N+6: (label, L1, _, _)     ← L_false 标签
第N+7: (:=, 2, _, x)         ← else 体
第N+8: (label, L2, _, _)     ← L_end 标签
```

---

## 七、WHILE_STATEMENT——循环语句的完整代码生成（第552—597行）

产生式：`WHILE_STATEMENT → while EXPRESSION do STATEMENT`

WHILE 循环使用 3 个标签（label_loop、label_body、label_exit），生成循环入口 → 条件判断 → 循环体 → 跳回的经典结构。

```
L_loop:                    ← 循环入口 (每次迭代回到此处)
    <comparison quads>     ← 条件表达式求值
    jnz cond _ L_body      ← 条件真 → 进入循环体
    jmp _ _ L_exit         ← 条件假 → 退出循环
L_body:                    ← 循环体标签
    <body quads>
    jmp _ _ L_loop         ← 无条件跳回循环入口
L_exit:                    ← 退出标签
```

| 行号 | 代码 | 讲解 |
|------|------|------|
| 553 | `char buf[MAX_STR];` | 格式化字符串缓冲区。 |
| 555 | `next_token(c);` | 消费 `while` 关键字。由 `STATEMENT` 已确认当前 Token 是 `while`（码值 24）。 |
| 557 | `int label_loop = sym_new_label(c);` | 分配**循环入口标签**（L_loop）。此标签标记每次迭代的起始位置。 |
| 558 | `int label_body = sym_new_label(c);` | 分配**循环体标签**（L_body）。条件为真时跳转到此执行循环体。 |
| 559 | `int label_exit = sym_new_label(c);` | 分配**退出标签**（L_exit）。条件为假时退出循环到此处。 |
| 561—562 | `snprintf(buf, MAX_STR, "L%d", label_loop);` | 格式化循环入口标签字符串。 |
| 563 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送 L_loop 标签**：`(label, L_loop, _, _)`。此标签在 L_body 之后通过 `jmp L_loop` 跳回，实现反复执行。 |
| 565—566 | `SemValue cond;` | 声明 `cond` 语义值，存放条件表达式的结果临时变量名。 |
| 567 | `parse_expression(c, &cond);` | **解析条件表达式**。结构与 IF 的条件表达式相同。 |
| 569—570 | `snprintf(buf, MAX_STR, "L%d", label_body);` | 格式化循环体标签。 |
| 571 | `quad_emit(c, OP_JNZ, cond.name, "_", buf);` | **条件真进入循环体**：`(jnz, 条件变量, _, L_body)`。语义：条件为真时，跳转到 L_body 执行循环体。 |
| 573—574 | `snprintf(buf, MAX_STR, "L%d", label_exit);` | 格式化退出标签。 |
| 575 | `quad_emit(c, OP_JMP, "_", "_", buf);` | **条件假退出**：`(jmp, _, _, L_exit)`。如果 `jnz` 没跳（条件为假），则继续执行到此条指令，跳出循环到 L_exit。 |
| 577—578 | `snprintf(buf, MAX_STR, "L%d", label_body);` | 再次格式化循环体标签。 |
| 579 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送 L_body 标签**。循环体从这里开始。 |
| 581 | `if (c->token.code == TOK_DO) {` | **期望 `do` 关键字**（码值 25）。条件表达式后必须跟 `do`。 |
| 582 | `next_token(c);` | 消费 `do`，准备解析循环体语句。 |
| 583 | `STATEMENT(c);` | 调用 `STATEMENT` 解析循环体。支持任意复杂的语句（包括嵌套的 while、if、begin-end 等）。 |
| 585—586 | `snprintf(buf, MAX_STR, "L%d", label_loop);` | 再次格式化循环入口标签。 |
| 587 | `quad_emit(c, OP_JMP, "_", "_", buf);` | **跳回循环入口**：`(jmp, _, _, L_loop)`。循环体执行完毕后，无条件跳回到 L_loop，重新执行条件检查，实现反复迭代。 |
| 589—590 | `snprintf(buf, MAX_STR, "L%d", label_exit);` | 格式化退出标签。 |
| 591 | `quad_emit(c, OP_LABEL, buf, "_", "_");` | **发送 L_exit 标签**：`(label, L_exit, _, _)`。循环退出点，程序继续从此处执行后续代码。 |
| 592—596 | `} else { c->error_flag = 1; snprintf(...); }` | 没有 `do`——语法错误："语法错误: while 后期望 'do'"。 |
| 597 | `}` | `WHILE_STATEMENT` 函数结束。 |

### WHILE 的完整四元式序列示例

假设源程序为 `while i < 10 do i := i + 1`：

```
第N条: (label, L0, _, _)     ← L_loop
第N+1: (jl, i, 10, T0)      ← 比较 i<10
第N+2: (jnz, T0, _, L1)     ← 条件真 → L_body
第N+3: (jmp, _, _, L2)      ← 条件假 → L_exit
第N+4: (label, L1, _, _)    ← L_body
第N+5: (+, i, 1, T1)        ← i+1
第N+6: (:=, T1, _, i)       ← i := i+1
第N+7: (jmp, _, _, L0)      ← 跳回 L_loop
第N+8: (label, L2, _, _)    ← L_exit
```

---

## 八、公开接口——两遍扫描架构与 Token 计数保存/恢复（第601—646行）

### 8.1 parser_init — 初始化（第601—604行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 601 | `void parser_init(Compiler *c) {` | `parser_init` 函数：初始化解析器状态。在每次开始语法分析前调用。 |
| 602 | `(void)c;` | 显式回避"未使用参数"的编译器警告。当前实现中 `c` 确实不需要被修改，但保留此参数以保持接口一致性和未来扩展可能性。 |
| 603 | `peek_valid = 0;` | 复位预读标志。清空之前可能残留的预读状态，确保 `next_token` 从 scanner 真实读取。 |
| 604 | `}` | `parser_init` 函数结束。 |

### 8.2 parser_parse — 两遍扫描架构详解（第606—646行）

`parser_parse` 是整个编译器中最重要的函数。它实现**两遍扫描**架构：

**第一遍**（词法分析）：
1. 调用 `scanner_scan_all` 将全篇源程序串行扫描
2. 同时填充符号表、常数表和 Token 序列
3. Token 序列可供后续查看/调试

**第二遍**（语法分析）：
1. 保存词法分析的 `token_count`（因为 `scanner_init` 会清零它）
2. 调用 `scanner_init` 重置扫描器回到源程序开头
3. 恢复 `token_count`
4. 重置临时计数（temp_count、label_count、quad_count、sem_top）
5. 调用 `PROGRAM` 开始递归下降
6. 检查语法错误和多余的尾部 Token

| 行号 | 代码 | 讲解 |
|------|------|------|
| 607 | `c->error_flag = 0;` | 清除错误标志。确保从干净状态开始本次解析。 |
| 608 | `c->error_msg[0] = '\0';` | 清除错误消息缓冲区。将第一个字符设为 '\0' 表示空字符串。 |
| 610 | `// 先做词法分析, 将 Token 全部读出到序列` | 注释说明第一遍扫描的意图。 |
| 611 | `scanner_scan_all(c);` | **第一遍——词法分析阶段**。该函数会：(1) 逐个调用 `scanner_next_token` 扫描全篇源程序，(2) 将每个 Token 存入 `c->token_list[]`，(3) 将标识符填入符号表，(4) 将常数填入常数表，(5) 设置 `c->token_count` 为 Token 总数。 |
| 612 | `if (c->error_flag) return 0;` | 如果词法分析阶段报错（如非法字符、格式错误），直接返回 0 表示失败，不再继续进行语法分析。 |
| 614 | `// 保存词法分析的 token_count (scanner_init 会清零)` | 注释说明保存 token_count 的原因——第二遍扫描开始时，`scanner_init` 会将该值清零。 |
| 615 | `int saved_tok_count = c->token_count;` | **保存 token_count**。将第一遍扫描得到的 Token 总数存到局部变量 `saved_tok_count`。这一步很关键：虽然第二遍扫描会逐 Token 消费，但 `token_count` 仍然需要保持第一遍的值，因为某些后续模块（如 Token 序列查看/调试）依赖它。 |
| 617 | `// 重置扫描器, 从源程序重新读取做语法分析` | 注释说明第二遍扫描的意图。 |
| 618 | `scanner_init(c, c->source);` | **初始化扫描器用于第二遍**。`scanner_init` 内部会：(1) 复制源程序指针，(2) 重置 `c->pos` 为 0（回到源程序开头），(3) 重置 `c->ch` 为第一个字符，(4) **清零 `c->token_count`**——这就是为什么上一步要先保存。 |
| 619 | `peek_valid = 0;` | 再次确保预读标志为 0。虽然 `parser_init` 刚设过，但这里显式重置防止意外。 |
| 620 | `c->token_count = saved_tok_count;` | **恢复 token_count**。将之前保存的 Token 总数写回编译器上下文。 |
| 622 | `// 初始化符号表和四元式 (保留词法阶段的符号/常数表)` | |
| 623 | `// 注意: sym_table 和 const_table 保留, 只重置临时计数` | 注释说明：sym_table 和 const_table 已在第一遍扫描时填充完成，此处不重置它们。只将那些在第二遍扫描中动态增长的计数重置。 |
| 624 | `c->temp_count = 0;` | 重置临时变量计数器。第二遍扫描中 `sym_new_temp` 将从这个基数开始分配 `T0, T1, T2, …`。 |
| 625 | `c->label_count = 0;` | 重置标签计数器。第二遍扫描中 `sym_new_label` 将从 0 开始分配 `L0, L1, L2, …`。 |
| 626 | `c->quad_count = 0;` | 重置四元式计数。清空四元式序列，准备从位置 0 开始生成新的中间代码。 |
| 627 | `c->sem_top = 0;` | 重置语义栈顶指针。确保声明语义栈从空开始。 |
| 629 | `// 读取第一个 Token` | |
| 630 | `next_token(c);` | 从第二遍扫描器读取第一个 Token 到 `c->token` 中。此时 `c->token` 应该是 `TOK_PROGRAM`（正常情况下）。 |
| 632 | `// 开始递归下降分析` | |
| 633 | `PROGRAM(c);` | **启动递归下降解析**。这是整个语法分析的入口点。从 `PROGRAM` 开始，经过 `SUB_PROGRAM` → `VARIABLE` → `COM_SENTENCE` → `STATEMENT` → 表达式分析等，覆盖全部源程序。 |
| 635 | `if (c->error_flag) return 0;` | 如果递归下降过程中设置了错误标志，返回 0 表示解析失败。 |
| 637 | `// 检查是否到达文件末尾` | |
| 638 | `if (c->token.code != TOK_EOF) {` | 解析完毕后，检查当前预读 Token 是否是文件结束符 `TOK_EOF`（码值 0）。如果不是，说明 `PROGRAM .` 之后还有多余的输入。 |
| 639 | `c->error_flag = 1;` | 设置错误标志。 |
| 640—641 | `snprintf(c->error_msg, sizeof(c->error_msg), "语法错误: 多余输入 Token %d", c->token.code);` | 记录错误信息，将多余的 Token 码格式化到消息中。例如源程序 `program a; begin end. extra` 会触发此错误，提示多余的 Token。 |
| 642 | `return 0;` | 返回失败。 |
| 644 | `}` | if 文件末尾检查结束。 |
| 645 | `return 1;` | **返回成功**。解析全程无错误，四元式序列已生成完毕。 |
| 646 | `}` | `parser_parse` 函数结束。 |

### 两遍扫描流程图

```
┌────────────────────────────────────────┐
│  parser_parse(c)                       │
│                                        │
│  ┌─ 第一遍: scanner_scan_all(c) ────┐ │
│  │  • 扫描全篇源程序字符             │ │
│  │  • 填充 sym_table[] (标识符)      │ │
│  │  • 填充 const_table[] (常数)      │ │
│  │  • 填充 token_list[] (Token序列)  │ │
│  │  • 设置 token_count               │ │
│  └──────────────────────────────────┘ │
│                   │                    │
│          saved_tok_count = token_count │
│                   │                    │
│  ┌─ 重置: scanner_init(c, source) ──┐ │
│  │  • scanner_init 清零 token_count  │ │
│  └──────────────────────────────────┘ │
│                   │                    │
│    token_count = saved_tok_count (恢复)│
│                   │                    │
│  ┌─ 第二遍: 递归下降语法分析 ───────┐ │
│  │  • next_token(c) 读取第一个Token  │ │
│  │  • PROGRAM(c) 启动递归下降       │ │
│  │  • 递归调用链完成解析             │ │
│  │  • 同时生成四元式序列             │ │
│  └──────────────────────────────────┘ │
│                   │                    │
│        检查 error_flag                 │
│        检查 TOK_EOF                    │
│        返回 1 (成功) / 0 (失败)        │
└────────────────────────────────────────┘
```

---

## 附录：整体调用关系图

```
parser_parse()
  ├── scanner_scan_all()           [第一遍: 词法分析]
  ├── scanner_init()               [重置扫描器]
  └── PROGRAM()                    [第二遍: 递归下降]
        ├── quad_emit(OP_PROGRAM)
        ├── SUB_PROGRAM()
        │     ├── VARIABLE()
        │     │     ├── semantic_a1()       (init offset)
        │     │     ├── ID_SEQUENCE()
        │     │     │     └── semantic_a2() ×N  (push to sem_stack)
        │     │     ├── TYPE()
        │     │     │     └── semantic_a3/a4/a5()  (set type)
        │     │     └── semantic_a6()       (pop sem_stack, fill sym_table)
        │     └── COM_SENTENCE()
        │           └── STATEMENT() ×N
        │                 ├── EVA_SENTENCE()
        │                 │     └── parse_expression()
        │                 ├── IF_STATEMENT()
        │                 │     ├── parse_expression()
        │                 │     ├── quad_emit(OP_JNZ)
        │                 │     ├── quad_emit(OP_JMP)
        │                 │     ├── quad_emit(OP_LABEL)
        │                 │     └── STATEMENT()
        │                 ├── WHILE_STATEMENT()
        │                 │     ├── quad_emit(OP_LABEL)
        │                 │     ├── parse_expression()
        │                 │     ├── quad_emit(OP_JNZ)
        │                 │     ├── quad_emit(OP_JMP)
        │                 │     └── STATEMENT()
        │                 ├── TOK_WRITE
        │                 │     └── quad_emit(OP_WRITE)
        │                 └── TOK_BEGIN → COM_SENTENCE() (recursive)
        └── quad_emit(OP_END)

parse_expression()
  └── parse_or_expr()              [L1: or]
        └── parse_and_expr()       [L2: and]
              └── parse_not_expr() [L3: not]
                    └── parse_comparison() [L4: = <> < > <= >=]
                          └── parse_arith_expr() [L5: + -]
                                └── parse_term() [L6: * /]
                                      └── parse_factor() [L7: id/const/(E)/-E]
```
