## 模块: parser.c — 语法分析 (纯语法结构识别)

# parser.c 逐行讲解

> parser.c — 语法分析器（解析器）的实现文件。采用递归下降子程序法，负责语法结构识别与错误检查。
> 所有语义动作（四元式生成、符号表填写）通过 `semantic.h` 接口委托给 `semantic.c`。
> 共 421 行，分为 6 个部分：头文件与预读、表达式分析（7 级）、递归下降子程序（10 个）、公开接口。

---

## 一、头文件与注释（行 1–10）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#include "parser.h"` | 引入自身头文件，获得 `parser_init`、`parser_parse` 的函数声明和 `grammar.h` 传递引入 |
| 2 | `#include "scanner.h"` | 引入词法分析器头文件。`next_token()` 在内部调用 `scanner_next_token()`，`parser_parse()` 中调用 `scanner_scan_all()` 和 `scanner_init()` |
| 3 | `#include "semantic.h"` | 引入语义动作接口。所有 `sem_a1~a6`、`sem_emit_*`、`sem_if_*`、`sem_while_*` 等语义函数的声明都在此头文件。这是语法与语义分离的关键 |
| 5–10 | 分割注释 | 说明 parser.c 的职责边界——只做语法结构识别和错误检查 |

---

## 二、Token 预读机制（行 12–23）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 12 | `static Token peek;` | 全局静态变量，存储预读的一个 Token。`static` 限定文件内可见 |
| 13 | `static int peek_valid;` | 标志位，0 表示 peek 为空，1 表示 peek 存储了有效 Token |
| 16–23 | `next_token(Compiler *c)` 函数 | 预读机制核心。如果 `peek_valid=1`，将 peek 复制到 `c->token` 并清零标志（消费 peek）；否则调用 `scanner_next_token()` 直接从源程序取下一个 Token。这样 parser 就有了"超前看一个"的能力 |

**设计意图**：递归下降中经常需要"看一眼但不消费"（如 `STATEMENT()` 根据当前 Token 分发到不同子程序），peek 机制提供 LL(1) 所需要的 1 个 Token 前瞻。

---

## 三、表达式分析 — 前向声明（行 25–33）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 26–33 | 8 个 `static void` 声明 | 7 级优先级互相递归调用，C 语言要求先声明后定义。调用链：`parse_expression → parse_or_expr → parse_and_expr → parse_not_expr → parse_comparison → parse_arith_expr → parse_term → parse_factor` |

---

## 四、表达式 L1 — or（行 35–43）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 35–43 | `parse_or_expr()` | 最低优先级（L1）。先调用 `parse_and_expr` 得左操作数 → 当后续 Token 为 `TOK_OR` 时循环：消费 or 运算符 → 递归 parse_and_expr 得右操作数 → 调 `sem_emit_binop(c, OP_OR_OP, sv, &right)` 生成 `(or, left, right, t)` 四元式。语义结果覆写 sv（名字变为临时变量名）。<br><br>**while 循环消除左递归**：`E → E or T` 消除为 `E → T { or T }`，用 while 而非直接左递归避免无限递归 |

---

## 五、表达式 L2 — and（行 45–53）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 45–53 | `parse_and_expr()` | 与 or 模式完全相同。先 parse_not_expr → while TOK_AND → sem_emit_binop(OP_AND_OP) |

---

## 六、表达式 L3 — not（行 55–63）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 55–63 | `parse_not_expr()` | 一元 `not` 运算符。因为 not 是一元的，处理方式不同：<br>• 遇 `TOK_NOT` → next_token 消费 → 递归自身（支持 `not not x`）→ 调 `sem_emit_unary_not(c, sv)` 生成 `(not, val, _, t)` 四元式 → sv 被覆写为临时变量<br>• 否则 → 降级到 `parse_comparison()` |

---

## 七、表达式 L4 — 比较运算符（行 65–76）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 65–76 | `parse_comparison()` | 处理 6 种关系运算符（`=` `<` `>` `<=` `>=` `<>`）。先 parse_arith_expr 得左值 → 检查当前 Token 是否是 `TOK_EQ/LT/GT/LE/GE/NE` 之一 → 如不是则直接返回（无比较）→ 如是则记录 `rel_op`，next_token 消费运算符 → parse_arith_expr 得右值 → `sem_emit_comparison(c, rel_op, sv, &right)` 生成比较四元式。注意比较是**非左结合**的，这里只处理 `<expr> rel_op <expr>` 一层 |

---

## 八、表达式 L5 — 加减（行 78–88）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 78–88 | `parse_arith_expr()` | 先 parse_term 得左值 → while `+` 或 `-` 时循环（左结合）：消费运算符 → parse_term 得右值 → `TOK_PLUS` 映射为 `OP_ADD`，`TOK_MINUS` 映射为 `OP_SUB` → `sem_emit_binop(c, quad_op, sv, &right)` |

**示例**：`a + b - c` 解析过程：
1. parse_term → factor 得 name=`"a"`
2. 遇 `+` → next_token → parse_term → name=`"b"` → sem_emit_binop(OP_ADD, "a", "b") → sv.name=`"t1"`
3. 遇 `-` → next_token → parse_term → name=`"c"` → sem_emit_binop(OP_SUB, "t1", "c") → sv.name=`"t2"`
4. 返回 t2 = (a+b)-c

---

## 九、表达式 L6 — 乘除（行 90–100）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 90–100 | `parse_term()` | 与加减相同模式，处理 `*` 映射为 `OP_MUL`，`/` 映射为 `OP_DIV` |

---

## 十、表达式 L7 — 因子（行 102–131）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 103–105 | `TOK_ID` 分支 | 标识符。调 `sem_value_from_id(c, index, sv)` → 从符号表取名字写入 sv.name → is_temp=0 → next_token 消费。这是语法分析中唯一确定"这是一个标识符"的地方 |
| 106–108 | `TOK_CONST` 分支 | 常数。调 `sem_value_from_const(c, index, sv)` → 从常数表取值转字符串写入 sv.name → is_temp=0 |
| 109–117 | `TOK_LPAREN` 分支 | 左括号。next_token → `parse_expression(c, sv)` 递归回到表达式入口（最低优先级 or_expr）→ 期望右括号 `TOK_RPAREN`，没有则报错误 "期望 ')'"。**括号改变了优先级**，把整个表达式当因子处理 |
| 118–121 | `TOK_MINUS` 分支 | 一元负号。next_token 消费 `-` → 递归 `parse_factor` 得操作数 → `sem_emit_unary_minus(c, sv)` 生成 `(-, 0, val, t)`（用 0 减去该值） |
| 122–127 | `else` 分支 | 语法错误。当前 Token 不是以上任何合法因子开头 → 设 error_flag=1 → 写错误信息含实际 Token 码 → sv.name 填 "err" 防止后续使用未初始化值 |

**入口**：`parse_expression()`（行 130–131）直接调用 `parse_or_expr`，这是整个表达式分析的入口。

---

## 十一、递归下降子程序 — 前向声明（行 134–145）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 136–145 | 10 个 static void 声明 | 由于 C 语言自上而下编译，前向声明不可或缺。10 个子程序分别对应文法的 10 个非终结符 |

---

## 十二、PROGRAM — 程序入口（行 147–174）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 148–149 | `if TOK_PROGRAM` | 文法要求源程序以 `program` 开头。不匹配 → 报错 "期望 'program'" |
| 150 | `next_token(c)` | 消费 `program` 关键字 |
| 151–152 | `if TOK_ID` | program 后必须跟标识符（程序名）。保存其 idx（符号表索引） |
| 153 | `next_token(c)` | 消费程序名标识符 |
| 154 | `sem_mark_program(c, idx)` | 语义动作：将程序名在符号表中标记为 KIND_PROGRAM，并生成 `(program, name, _, _)` 四元式 |
| 155 | `SUB_PROGRAM(c)` | 递归下降进入子程序部分 |
| 156–158 | `if TOK_DOT` | 程序以 `.` 结束 → next_token 消费 → `sem_emit_end(c)` 生成 `(end, _, _, _)` 四元式 |
| 159–162 | else | `.` 缺失 → 语法错误，报 "期望 '.' 但得到 Token X" |
| 165–167 | else | program 后不是标识符 → 报错 "program 后期望标识符" |
| 170–172 | else | 连 program 都没有 → 报错 "期望 'program'" |

---

## 十三、SUB_PROGRAM（行 176–180）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 177–180 | `SUB_PROGRAM()` | 文法是 `SUB_PROGRAM → VARIABLE COM_SENTENCE`。纯组合函数，依次调用两个子程序，自身不包含语义动作 |

---

## 十四、VARIABLE — 变量声明（行 182–205）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 183–184 | `if TOK_VAR` | 文法 `VARIABLE → var ID_SEQ : TYPE ; | ε`。当前 Token 是 var 则进入声明分支，否则为空产生式 → 什么也不做 |
| 185 | `next_token(c)` | 消费 `var` 关键字 |
| 186 | `sem_a1(c)` | 语义动作 a1：`cur_offset = 0`，初始化变量偏移计数器 |
| 187 | `ID_SEQUENCE(c)` | 解析标识符序列 `id { , id }`，每遇 id 触发 a2 压栈 |
| 188–189 | `if TOK_COLON` | 标识符列表后必须是 `:` |
| 190 | `TYPE(c)` | 解析类型名（integer/real/char），触发 a3/a4/a5 设置 cur_type |
| 191–193 | `if TOK_SEMICOLON` | `;` 触发 a6——FIFO 弹栈，对每个标识符填写类型/宽度/偏移到符号表 → next_token |
| 194–197 | else | 缺少 `;` → 语法错误 |
| 200–202 | else | 缺少 `:` → 语法错误 |

**流程示例** `var a,b:integer;`：
```
a1(offset=0) → a2(push a) → a2(push b) → a3(type=integer, len=4)
→ a6: 栈[a,b] → a.type=integer, a.offset=0, offset=4
             → b.type=integer, b.offset=4, offset=8
```

---

## 十五、ID_SEQUENCE — 标识符序列（行 207–229）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 208–211 | 第一个 id | 必须有至少一个标识符 → 调 sem_a2 压栈 → next_token 消费 |
| 212–223 | while `,` | 逗号分隔的后续标识符。读入 `,` → next_token → 期望标识符 → sem_a2 压栈 → next_token。循环直到不是逗号为止。如果 `,` 后不是标识符则报错并 return |
| 224–227 | else | 连第一个标识符都没有 → 语法错误 "期望标识符" |

---

## 十六、TYPE — 类型（行 231–242）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 233–236 | switch 三个 case | TOK_INTEGER → sem_a3(cur_type=integer) → next_token；TOK_REAL → sem_a4(cur_type=real)；TOK_CHAR → sem_a5(cur_type=char) |
| 237–240 | default | 不是以上三种 → 语法错误 "期望类型名 (integer/real/char)" |

---

## 十七、COM_SENTENCE — 复合语句（行 244–265）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 245–247 | `if TOK_BEGIN` | `COM_SENTENCE → begin STATEMENT { ; STATEMENT } end`。未遇 begin → 报错 |
| 247–248 | next_token → STATEMENT | 消费 begin → 解析第一条语句 |
| 249–252 | while `;` | 分号分隔后续语句。读 `;` → next_token → STATEMENT。循环直到不是分号为止 |
| 253–254 | `if TOK_END` | 语句结束 → next_token 消费 end |
| 255–258 | else | 缺少 end → 语法错误 |
| 260–263 | else | 缺少 begin → 语法错误 |

---

## 十八、STATEMENT — 语句分发（行 267–299）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 268–299 | `STATEMENT()` 的 switch | **这是语法分析的枢纽**。根据当前 Token 类型分发到不同的语句子程序： |
| 270–271 | `TOK_ID` | 赋值语句。`a := 2` 以标识符开头 → EVA_SENTENCE |
| 273–274 | `TOK_IF` | if 语句 → IF_STATEMENT |
| 276–277 | `TOK_WHILE` | while 语句 → WHILE_STATEMENT |
| 279–291 | `TOK_WRITE` | write 输出语句。next_token 消费 write → 期望标识符 → 取出 id_name → `sem_emit_write(c, id_name)` 生成 `(write, id, _, _)` 四元式 → next_token。分号由 COM_SENTENCE 的 while 循环统一处理 |
| 293–294 | `TOK_BEGIN` | 嵌套的 begin-end 块 → COM_SENTENCE |
| 296–297 | default | 其他 Token 视为空语句（不报错） |

---

## 十九、EVA_SENTENCE — 赋值语句（行 301–323）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 302–307 | `if TOK_ID` | 赋值语句 `id := EXPRESSION` 必须以标识符开头。保存 idx 和 id_name → next_token 消费 id |
| 308–309 | `if TOK_ASSIGN` | 赋值符号 `:=` 必须紧随 id → next_token 消费 |
| 310–311 | `parse_expression(c, &sv)` | 递归下降分析右端表达式，结果存入 SemValue sv |
| 312 | `sem_emit_assign(c, sv.name, id_name)` | 语义动作：生成 `(:=, expr_val, _, id)` 四元式 |
| 313–316 | else | 标识符后面不是 `:=` → 语法错误 |
| 318–321 | else | 赋值语句不是以标识符开头 → 语法错误 |

---

## 二十、IF_STATEMENT（行 325–355）

### 文法与四元式模式

```
if EXPRESSION then STATEMENT [ else STATEMENT ]

(jg/..., a, b, t)     -- 比较结果在 t
(jnz, t, _, L_true)   -- t≠0 跳转到 then
(jmp, _, _, L_false)  -- t=0 跳转到 else/出口
(label, L_true, _, _) -- L_then:
<then body>
(jmp, _, _, L_end)    -- 有 else 时跳过 else
(label, L_false, _, _) -- L_false:
<else body>
(label, L_end, _, _)  -- L_end:
```

| 行号 | 代码 | 讲解 |
|------|------|------|
| 327 | `next_token(c)` | 消费 `if` 关键字 |
| 329–330 | `parse_expression(c, &cond)` | 解析条件表达式，结果存在 SemValue cond（一个临时变量，值为 0 或 1） |
| 332–333 | `sem_if_begin(c, &cond, &label_true, &label_false, &label_end)` | 语义动作：分配 3 个标号，并生成第一条跳转四元式 `(jnz, cond, _, L_true)` + `(jmp, _, _, L_false)` |
| 335 | `sem_if_then_label(c, label_true)` | 生成 `(label, L_true, _, _)` |
| 337–338 | `if TOK_THEN` | 期望 `then` 关键字 → next_token 消费 |
| 339 | `STATEMENT(c)` | 递归解析 then 分支（可以是任何 STATEMENT） |
| 341–346 | `if TOK_ELSE` 有 else 分支 | next_token 消费 else → `sem_if_then_end(c, label_end)` 生成跳过 else 的 jmp → `sem_if_false_label(c, label_false)` 生成 else 入口标号 → STATEMENT(c) 解析 else 体 → `sem_if_end_label(c, label_end)` 生成出口标号 |
| 347–348 | `else` 无 else 分支 | `sem_if_false_label(c, label_false)` — false 标号即为唯一起效的出口标号，end 标号不生成 |
| 350–354 | else | 没有 then → 语法错误 "if 后期望 'then'" |

---

## 二十一、WHILE_STATEMENT（行 357–381）

### 文法与四元式模式

```
while EXPRESSION do STATEMENT

(label, L_loop, _, _)  -- L_loop:
(jg/..., a, b, t)       -- 比较结果
(jnz, t, _, L_body)     -- t≠0 进入循环体
(jmp, _, _, L_exit)     -- t=0 退出循环
(label, L_body, _, _)   -- L_body:
<body>
(jmp, _, _, L_loop)     -- 无条件回跳入口
(label, L_exit, _, _)   -- L_exit:
```

| 行号 | 代码 | 讲解 |
|------|------|------|
| 359 | `next_token(c)` | 消费 `while` |
| 361–362 | `sem_while_begin(c, &label_loop, &label_body, &label_exit)` | 语义动作：分配 3 个标号（loop/body/exit） |
| 364 | `sem_while_loop_label(c, label_loop)` | 生成 `(label, L_loop, _, _)` |
| 366–367 | `parse_expression(c, &cond)` | 解析条件表达式，结果 cond |
| 369 | `sem_while_check(c, &cond, label_body, label_exit)` | 语义动作：生成 `(jnz, cond, _, L_body)` + `(jmp, _, _, L_exit)` |
| 370 | `sem_while_body_label(c, label_body)` | 生成 `(label, L_body, _, _)` |
| 372–375 | `if TOK_DO` | 期望 do → next_token → STATEMENT(c) 解析循环体 → `sem_while_end(c, label_loop, label_exit)` 生成回跳 `(jmp, _, _, L_loop)` + 出口标号 `(label, L_exit, _, _)` |
| 376–380 | else | 没有 do → 语法错误 |

---

## 二十二、公开接口 — parser_init（行 383–388）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 385–388 | `parser_init()` | 重置预读状态。`(void)c` 避免未使用参数警告。`peek_valid = 0` 确保下次 next_token 走 scanner_next_token 路径 |

---

## 二十三、公开接口 — parser_parse（行 390–421）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 391–392 | 清零错误标志 | error_flag=0，error_msg 置空 |
| 394–395 | `scanner_scan_all(c)` | **第一遍**——词法分析。一次性扫描整个源程序，生成 Token 序列 + 填充符号表/常数表。失败则返回 0 |
| 397 | `saved_tok_count` | 保存第一遍扫描得到的 Token 总数。必须保存，因为 scanner_init 会重置 |
| 399–401 | 重置扫描器 | scanner_init 重置 pos 为 0，重新定位到源程序开头（从头再读）。`peek_valid = 0` 清除任何残留 peek。**恢复 token_count** 为第一遍的计数——这样 `token_list[]` 数据不丢失 |
| 403–406 | 清零中间代码状态 | temp_count=0、label_count=0、quad_count=0、sem_top=0。**符号表 sym_table 和常数表 const_table 保留**——它们在第一遍已填充完毕 |
| 408 | `next_token(c)` | 读取源程序的第一个 Token |
| 409 | `PROGRAM(c)` | 启动递归下降——从文法起始符号 PROGRAM 开始 |
| 411–412 | 错误检查 | 语法分析中任一子程序发现错误，error_flag 被置 1 → 直接返回 0 |
| 413–418 | EOF 检查 | 成功解析全部语句后，应该到达文件末尾（TOK_EOF）。如果当前 Token 不是 EOF，说明源程序有"多余输入"——如 `end. x` 中 end 后的 x → 报错 |
| 420 | `return 1` | 编译成功 |

**两遍扫描架构**：第一遍纯词法，快速收集所有 Token 和符号信息；第二遍语法+语义，利用第一遍收集的符号表做分析。这样做的优势是可以先获得完整的标识符列表再开始语法分析。
