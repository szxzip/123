## 模块: semantic.c — 语义动作实现 (a1~a6 + 四元式生成)

## 简明解释

**各函数组的实现要点：**

- **a1–a6（声明语义）**：翻译文法中处理变量声明的语义动作
  - `a1` 将 `cur_offset` 归零，初始化新一轮偏移分配
  - `a2` 将当前 Token 的 value（符号表索引）压入 `sem_stack`
  - `a3`/`a4`/`a5` 分别设置 `cur_type = TY_INTEGER` / `TY_REAL` / `TY_CHAR`
  - `a6` 从栈底到栈顶 (FIFO) 遍历，对每个索引调用 `sym_set_type(c, idx, type, len, cur_offset)`，然后 `cur_offset += len`；完成后清栈 `sem_top = 0`
- **表达式语义**：`sem_emit_binop` 分配临时变量 temp，生成 `(op, left, right, temp)` 四元式，并通过 `strncpy` 将 temp 名称**覆写回 left->name**（因为调用者持有 left 指针，upper-level 解析循环需要将当前结果作为下一轮左操作数）。一元 not 生成单操作数四元式 `(not, sv, _, t)`；一元负号翻译为 `0 - x` 即 `(-, "0", sv, t)`。比较运算通过 switch 将 6 种 TOK_\* 关系运算符映射为对应的 OP_J\* 四元式操作码（注意：OP_JE 等在 codegen 阶段翻译为 cmp + set\* 指令，生成 0/1 布尔值，**不是**直接跳转指令）
- **赋值与输出**：`sem_emit_assign` 直接 `quad_emit(OP_ASSIGN, src, "_", dst)`；`sem_emit_write` 调用 `quad_emit(OP_WRITE, id, "_", "_")`，在 codegen 阶段翻译为 `movl id, %esi; call printf`
- **程序结构**：`sem_mark_program` 将符号表条目标记为 `KIND_PROGRAM`，生成 `(program, name, _, _)` 入口标记。`sem_emit_end` 生成 `(end, _, _, _)`，codegen 翻译为 `xorl %eax,%eax; leave; ret`
- **IF 语义**：`sem_if_begin` 分配 3 个 `sym_new_label` 标号 (L_true, L_false, L_end)，通过指针返回给 parser，并生成初始的 `jnz cond L_true` + `jmp L_false`。then 分支体前后分别由 `sem_if_then_label` 发射入口、`sem_if_then_end` 在有 else 时补 `jmp L_end`。false 分支由 `sem_if_false_label` 发射入口。无论有无 else，最终出口由 `sem_if_end_label` 发射。标号管理完全在 semantic 层，parser 无需知道具体编号
- **WHILE 语义**：`sem_while_begin` 分配 loop/body/exit 三个标号。`sem_while_loop_label` 在循环顶部发射 loop label。`sem_while_check` 在条件表达式解析后发射 `jnz cond L_body` + `jmp L_exit`。`sem_while_body_label` 发射循环体入口 label。`sem_while_end` 在循环体末尾发射 `jmp L_loop`（无条件回跳）和 `L_exit` 出口 label
- **SemValue 工具**：`sem_init_sv` 将 name 设为空串、is_temp 设为 0。`sem_value_from_id` 从 `sym_table[idx].name` 拷贝标识符名到 sv，无效索引用 `"id%d"` 兜底。`sem_value_from_const` 将常数表的 double 值用 `snprintf("%g")` 转换为字符串（如 3.14 → "3.14"），无效索引用兜底字符串

> 编译器原理：语义分析是语法制导的——解析过程中每个产生式触发对应的语义动作。翻译文法 (translation grammar) 将语义动作嵌入文法规则中，使解析器在规约时同步执行语义动作，生成中间代码。三地址码（四元式）将复杂表达式拆解为 `t1 = a op b` 形式的简单步骤，每条四元式最多涉及三个地址（两个源操作数 + 一个目标）。临时变量的分配与覆写策略使得表达式树被线性化为四元式序列。

---

# semantic.c 逐行讲解

> semantic.c — 语义分析器的实现文件。包含翻译文法 a1~a6、表达式四元式生成、
> 赋值/输出/IF/WHILE 四元式生成、语义值工具函数。所有函数均通过 `semantic.h` 声明的接口被 `parser.c` 调用。
> 共 196 行，分为 7 个部分。

---

## 一、头文件（行 1–3）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#include "semantic.h"` | 引入自身头文件，获得所有 `sem_*` 函数的前向声明及 `grammar.h` 传递引入 |
| 3 | `#include "symbol.h"` | 引入符号表模块。需要 `sym_set_type()`（a6 填符号表）、`sym_new_temp()`（分配临时变量）、`sym_new_label()`（分配标号） |
| 2 | `#include "quadruple.h"` | 引入四元式模块。所有语义动作最终都会调用 `quad_emit()` 生成四元式 |

---

## 二、翻译文法 a1~a6（行 5–31）

### a1：初始化偏移（行 7–9）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 7–9 | `sem_a1()` | 对应翻译文法 `a1: id.cat := v; id.offset := 0`。将 `cur_offset` 归零，开始新一轮变量声明的偏移分配。在 `parser.c` 的 `VARIABLE()` 中读到 `var` 后立即调用 |

### a2：压栈标识符（行 11–14）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 11–14 | `sem_a2()` | 对应 `a2: push(id_Token.val)`。将当前 Token 的 `value`（即符号表中该标识符的索引）压入语义栈 `sem_stack`。`sem_top` 记录栈顶位置。在 `ID_SEQUENCE()` 中每读到一个 id 调用一次 |

### a3/a4/a5：设置当前类型（行 16–18）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 16 | `sem_a3()` | `cur_type = TY_INTEGER` — 翻译文法 `a3: id.type := i; id.len := 4` |
| 17 | `sem_a4()` | `cur_type = TY_REAL` — `a4: id.type := r; id.len := 8` |
| 18 | `sem_a5()` | `cur_type = TY_CHAR` — `a5: id.type := c; id.len := 1` |

这些函数在 `TYPE()` 中根据关键字选择调用。后续 a6 用 `cur_type` 和 `type_len(cur_type)` 获取当前类型信息。

### a6：弹栈填符号表（行 20–31）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 21–22 | 取类型信息 | `type = cur_type`（在读 TYPE 时由 a3/a4/a5 设置），`len = type_len(type)`（integer→4, real→8, char→1） |
| 23–29 | **FIFO 弹栈** | 对应翻译文法 pseudocode：`while(栈不空) { id.entry := pop(); enter(...) }`。从栈底 `sem_stack[0]` 到栈顶 `sem_stack[sem_top-1]` 遍历，每个 idx 调 `sym_set_type(c, idx, type, len, cur_offset)`，然后 `cur_offset += len`。这样按声明顺序依次分配：a 偏移 0，b 偏移 4 |
| 30 | 清栈 | `sem_top = 0`，栈清空，准备下次变量声明 |

---

## 三、表达式语义（行 33–71）

### sem_emit_binop — 二元运算（行 35–40）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 35–40 | `sem_emit_binop()` | 通用二元运算语义函数，被 or/and/+- 等各层调用。<br>① `sym_new_temp(c)` 分配临时变量如 t3 → ② `quad_emit(c, quad_op, left->name, right->name, t)` 生成如 `(+, a, b, t3)` → ③ `strncpy(left->name, t, ...)` 将结果覆写回 left — **关键**：left 指针指向调用者的 sv，所以调用者的 sv.name 被更新为临时变量名 → ④ `left->is_temp = 1` 标记结果来自临时变量 |
| 38 | `strncpy` 覆写 | 为什么覆写 left？因为调用链中上层的 while 循环需要当前结果作为下一轮左操作数 |

### sem_emit_unary_not — 逻辑非（行 42–47）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 42–47 | `sem_emit_unary_not(c, sv)` | 生成 `(not, sv.name, _, t)` 四元式。OP_NOT_OP 是单目运算，第二操作数为 `"_"`。结果覆写 sv |

### sem_emit_unary_minus — 一元负号（行 49–54）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 49–54 | `sem_emit_unary_minus(c, sv)` | 生成 `(-, "0", sv.name, t)` 四元式。一元 -x 翻译为 `0 - x` |

### sem_emit_comparison — 比较运算（行 56–71）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 56–71 | `sem_emit_comparison(c, rel_op, left, right)` | ① switch 将 Token 关系运算符码映射为四元式操作码：`TOK_EQ→OP_JE`、`TOK_LT→OP_JL` 等 6 种映射 → ② 分配临时变量 → ③ 生成比较四元式如 `(jg, a, 5, t)` → ④ 覆写 left |
| 59–66 | switch 映射表 | 注意命名歧义：`OP_JE` 在 codegen 阶段翻译为 `cmp + sete`，**不是直接 jmp 指令**，它生成的是 0/1 布尔值而不是条件跳转。真正的条件跳转由 IF/WHILE 中的 `OP_JNZ` + `OP_JMP` 完成 |

---

## 四、赋值与输出语义（行 73–81）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 75–77 | `sem_emit_assign(c, src, dst)` | 生成 `(:=, src, _, dst)`。`OP_ASSIGN` 四元式格式：arg1=源值，arg2 固定 `_`，result=目标变量 |
| 79–81 | `sem_emit_write(c, id)` | 生成 `(write, id, _, _)`。OP_WRITE 在 codegen 翻译为 `movl id, %esi / call printf` |

---

## 五、程序结构语义（行 83–95）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 85–91 | `sem_mark_program(c, idx)` | ① 验证 idx 有效后设 `sym_table[idx].kind = KIND_PROGRAM`（区分程序名和普通变量） → ② 从符号表取 name → ③ `quad_emit(OP_PROGRAM, name, "_", "_")` 生成程序入口标记 |
| 93–95 | `sem_emit_end(c)` | 生成 `(end, _, _, _)`。codegen 翻译为 `xorl %eax,%eax; leave; ret` |

---

## 六、IF 语句语义（行 97–135）

| 行号 | 函数 | 讲解 |
|------|------|------|
| 99–111 | `sem_if_begin(c, cond, &lt, &lf, &le)` | ① `sym_new_label` × 3 分配 Ln（true）、Lf（false）、Le（end）三个标号 → ② 通过指针输出这些标号给 parser → ③ `quad_emit(OP_JNZ, cond.name, _, L_true)` 条件跳转 → ④ `quad_emit(OP_JMP, _, _, L_false)` 无条件跳转 |
| 113–117 | `sem_if_then_label(c, label)` | 生成 then 入口标号 `(label, L_true, _, _)` |
| 119–123 | `sem_if_then_end(c, label_end)` | then 体结束后，在有 else 的情况下生成 `(jmp, _, _, L_end)` 跳过 else 分支 |
| 125–129 | `sem_if_false_label(c, label)` | 生成 false 入口标号 `(label, L_false, _, _)` |
| 131–135 | `sem_if_end_label(c, label)` | 生成整个 if 出口标号 `(label, L_end, _, _)` |

**设计细节**：标号管理完全在 semantic 层，parser 不需要知道 Ln/Lf/Le 具体是多少号。IF 有 else 时生成 3 个标号，无 else 时只有 2 个（L_end 不生成，false 就是出口）。

---

## 七、WHILE 语句语义（行 137–173）

| 行号 | 函数 | 讲解 |
|------|------|------|
| 139–143 | `sem_while_begin(c, &ll, &lb, &le)` | 分配 3 个标号：loop（循环入口）、body（循环体）、exit（出口） |
| 145–149 | `sem_while_loop_label(c, label)` | 生成 `(label, L_loop, _, _)` — 循环每次迭代前先回到这里 |
| 151–158 | `sem_while_check(c, cond, label_body, label_exit)` | 条件表达式已解析完后调用：① 生成 `(jnz, cond, _, L_body)` — 条件真进入 → ② 生成 `(jmp, _, _, L_exit)` — 条件假退出 |
| 160–164 | `sem_while_body_label(c, label)` | 生成 `(label, L_body, _, _)` |
| 166–173 | `sem_while_end(c, label_loop, label_exit)` | 循环体结束后：① 生成 `(jmp, _, _, L_loop)` 无条件回跳 → ② 生成 `(label, L_exit, _, _)` 出口标号 |

---

## 八、语义值工具（行 175–196）

| 行号 | 函数 | 讲解 |
|------|------|------|
| 177–180 | `sem_init_sv(sv)` | 初始化 SemValue：name[0]='\0'（空字符串），is_temp=0 |
| 182–188 | `sem_value_from_id(c, idx, sv)` | 从符号表读取标识符信息填充 sv。`idx` 有效 → `strncpy(sv->name, sym_table[idx].name, ...)`；无效 → `snprintf(sv->name, "id%d", idx)` 用索引号兜底。`is_temp=0` 表示这是用户定义的变量 |
| 190–196 | `sem_value_from_const(c, idx, sv)` | 从常数表读取常数值填充 sv。`idx` 有效 → `snprintf(sv->name, "%g", const_table[idx])` 将 double 转为字符串（如 3.14 → "3.14"）；无效 → 兜底字符串。`is_temp=0` 表示这是字面常量 |
