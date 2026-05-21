## 模块: semantic.h — 语义动作接口

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
