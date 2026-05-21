# semantic.c — 语义动作实现

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1-3 | `#include` | 引入 semantic.h、symbol.h（sym_set_type/sym_new_temp/sym_new_label）、quadruple.h（quad_emit） |

## 翻译文法 a1~a6（5-31 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 7-9 | `sem_a1()` | 初始化 cur_offset = 0（变量声明的偏移计数器归零） |
| 11-14 | `sem_a2()` | 将当前 Token 的 value（符号表索引）压入 sem_stack |
| 16-18 | `sem_a3/a4/a5()` | 分别设 cur_type = integer/real/char |
| 20-31 | `sem_a6()` | FIFO 弹栈（从栈底到栈顶），对每个标识符调 sym_set_type 补填类型/宽度/偏移，偏移逐次累加 len |

## 表达式语义（33-71 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 35-40 | `sem_emit_binop()` | 二元运算：sym_new_temp 分配 tN → quad_emit 生成 (op, left, right, tN) → 更新 left.name = tN, is_temp = 1 |
| 42-47 | `sem_emit_unary_not()` | 逻辑非：作 (not, sv, _, tN)，结果覆写 sv |
| 49-54 | `sem_emit_unary_minus()` | 一元负号：作 (-, 0, sv, tN) |
| 56-71 | `sem_emit_comparison()` | 比较运算符：TOK_EQ→OP_JE 等 6 种映射，sym_new_temp → quad_emit → 结果覆写 left |

## 赋值与输出（73-81 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 75-77 | `sem_emit_assign()` | quad_emit(OP_ASSIGN, src, "_", dst) |
| 79-81 | `sem_emit_write()` | quad_emit(OP_WRITE, id, "_", "_") |

## 程序结构（83-95 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 85-91 | `sem_mark_program()` | 设 sym_table[idx].kind = KIND_PROGRAM → quad_emit(OP_PROGRAM, name, "_", "_") |
| 93-95 | `sem_emit_end()` | quad_emit(OP_END, "_", "_", "_") |

## IF 语句（97-135 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 99-111 | `sem_if_begin()` | sym_new_label × 3（true/false/end）→ 生成 (jnz, cond, _, L_true) + (jmp, _, _, L_false) |
| 113-117 | `sem_if_then_label()` | 生成 (label, L_then, _, _) |
| 119-123 | `sem_if_then_end()` | then 结束 → 生成 (jmp, _, _, L_end) 跳过 else |
| 125-135 | `sem_if_false_label/end_label()` | 生成 else 分支入口标号 / 出口标号 |

## WHILE 语句（137-173 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 139-143 | `sem_while_begin()` | sym_new_label × 3（loop/body/exit） |
| 145-149 | `sem_while_loop_label()` | 生成 (label, L_loop, _, _) |
| 151-158 | `sem_while_check()` | 条件比较后 → 生成 (jnz, cond, _, L_body) + (jmp, _, _, L_exit) |
| 160-164 | `sem_while_body_label()` | 生成 (label, L_body, _, _) |
| 166-173 | `sem_while_end()` | 循环体结束 → 生成 (jmp, _, _, L_loop) + (label, L_exit, _, _) |

## 语义值工具（175-196 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 177-180 | `sem_init_sv()` | 清零 SemValue |
| 182-188 | `sem_value_from_id()` | 从 sym_table[idx] 读取标识符名写入 sv.name，is_temp = 0 |
| 190-196 | `sem_value_from_const()` | 从 const_table[idx] 读取常数值转字符串写入 sv.name，is_temp = 0 |
