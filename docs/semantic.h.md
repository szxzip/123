## 模块: semantic.h — 语义动作接口

## 简明解释

**23 个语义动作函数按用途分组：**

- **声明语义 (a1–a6, 6 个函数)**：`a1` 初始化 offset → `a2` 将标识符索引压入语义栈 → `a3`/`a4`/`a5` 设置当前类型 (integer/real/char) → `a6` FIFO 弹栈，遍历栈中所有标识符，填入符号表（type、len、offset），然后 offset += len
- **表达式语义 (4 个函数)**：`sem_emit_binop` 分配临时变量，生成 `(op, left, right, t)` 四元式，将结果覆写回 left；`sem_emit_unary_not` 生成 `(not, sv, _, t)`；`sem_emit_unary_minus` 生成 `(-, "0", sv, t)`；`sem_emit_comparison` 将 TOK_\* 映射为 OP_J\* 操作码，生成比较四元式
- **赋值与输出 (2 个函数)**：`sem_emit_assign` 生成 `(:=, src, _, dst)`；`sem_emit_write` 生成 `(write, id, _, _)`
- **程序结构 (2 个函数)**：`sem_mark_program` 标记符号表条目为 KIND_PROGRAM，生成 `(program, name, _, _)`；`sem_emit_end` 生成 `(end, _, _, _)`
- **IF 语义 (5 个函数)**：`sem_if_begin` 分配 3 个标号 (L_then / L_false / L_end)，生成 jnz + jmp → 有 else 时 `sem_if_then_end` 在 then 体后补 `jmp L_end` → `sem_if_then_label`/`sem_if_false_label`/`sem_if_end_label` 分别发射相应 label 四元式
- **WHILE 语义 (5 个函数)**：`sem_while_begin` 分配 3 个标号 (L_loop / L_body / L_exit) → `sem_while_loop_label` 发射 loop 入口 label → `sem_while_check` 生成 `jnz cond L_body` + `jmp L_exit` → `sem_while_body_label` 发射体入口 label → `sem_while_end` 生成 `jmp L_loop` 回跳 + `L_exit` 出口 label
- **SemValue 工具 (3 个函数)**：`sem_init_sv` 初始化 name / is_temp；`sem_value_from_id` 从符号表索引填充 sv.name；`sem_value_from_const` 从常数表索引（double → 字符串）填充 sv.name

> 编译器原理：语义分析赋予语法树意义。本编译器采用语法制导翻译（翻译文法）——语义动作在递归下降解析过程中执行，而非解析完成后。a1–a6 模式是经典示例：变量声明需先收集所有标识符（a2 压栈），看到类型后再统一填入类型信息（a6 FIFO 弹栈）。表达式语义实现三地址码生成——复杂表达式被分解为带临时变量的简单运算。

---

# semantic.h — 语义动作接口

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1-3 | `#ifndef` / `#define` / `#include` | 头文件保护，引入 grammar.h |
| 6-11 | `sem_a1~a6()` 声明 | 翻译文法语义动作（声明语句）：a1 初始化 offset、a2 压栈标识符、a3/a4/a5 设当前类型、a6 弹栈填符号表 |
| 14-17 | `sem_emit_binop/unary_not/unary_minus/comparison()` | 表达式语义：二元运算、一元 not/负号、比较运算。各自分配临时变量并生成对应四元式 |
| 20-21 | `sem_emit_assign/write()` | 赋值 `(:=, src, _, dst)` 和输出 `(write, id, _, _)` 四元式生成 |
| 24-25 | `sem_mark_program/emit_end()` | 程序结构语义：标记程序名（设 kind=KIND_PROGRAM）生成 `(program,...)` 和 `(end,_,_,_)` |
| 28-33 | `sem_if_begin/then_label/then_end/false_label/end_label()` | IF 语句语义：分配 3 个标号，生成 jnz/jmp/label 四元式，区分有/无 else 分支 |
| 36-40 | `sem_while_begin/loop_label/check/body_label/end()` | WHILE 语句语义：分配 3 个标号，生成循环入口/label/条件跳转/回跳/出口四元式 |
| 43-45 | `sem_init_sv/value_from_id/value_from_const()` | 语义值工具：初始化 SemValue、从标识符索引/常数索引填充 SemValue.name |
