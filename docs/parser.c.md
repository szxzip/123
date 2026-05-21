## 模块: parser.c — 语法分析 (纯语法结构识别)

# parser.c — 语法分析 (纯语法结构)

parser.c 只负责语法结构识别与错误检查，所有语义动作通过 `semantic.h` 调用。

## 头部与 Token 预读（1-23 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1-3 | `#include` | 引入 parser.h、scanner.h、semantic.h |
| 5-10 | 注释 | 说明职责分离：只做语法，语义走 semantic.h |
| 12-13 | `peek / peek_valid` | 单 Token 预读机制全局变量 |
| 16-23 | `next_token()` | 优先取 peek（消费后清零），否则调 scanner_next_token |

## 表达式分析（25-132 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 26-33 | 前向声明 | 8 个 parse_* 函数声明 |
| 35-43 | `parse_or_expr()` | L1 or：先 parse_and → while TOK_OR → 调 sem_emit_binop(OP_OR_OP) |
| 45-53 | `parse_and_expr()` | L2 and：同模式 |
| 55-63 | `parse_not_expr()` | L3 not：遇 TOK_NOT → 递归 → sem_emit_unary_not；否则降级 |
| 65-76 | `parse_comparison()` | L4 比较：遇 6 种比较符 → sem_emit_comparison(rel_op) |
| 78-88 | `parse_arith_expr()` | L5 +-：while +- → sem_emit_binop(OP_ADD/SUB) |
| 90-100 | `parse_term()` | L6 */：while */ → sem_emit_binop(OP_MUL/DIV) |
| 102-128 | `parse_factor()` | L7 因子：TOK_ID → sem_value_from_id；TOK_CONST → sem_value_from_const；TOK_LPAREN → 递归 → 期望 `)`；TOK_MINUS → sem_emit_unary_minus；其他报错 |
| 130-132 | `parse_expression()` | 入口：parse_or_expr |

## 递归下降子程序（134-381 行）

| 行号 | 函数 | 语法规则 | 调用的语义 |
|------|------|---------|-----------|
| 148-174 | `PROGRAM()` | `program id SUB_PROGRAM .` | sem_mark_program / sem_emit_end |
| 177-180 | `SUB_PROGRAM()` | `VARIABLE COM_SENTENCE` | (无，纯组合) |
| 183-205 | `VARIABLE()` | `var ID_SEQ : TYPE ;` 或 ε | sem_a1 / sem_a6 |
| 208-229 | `ID_SEQUENCE()` | `id { , id }` | sem_a2（每遇 id 压栈） |
| 232-242 | `TYPE()` | `integer \| real \| char` | sem_a3 / a4 / a5 |
| 245-265 | `COM_SENTENCE()` | `begin STATEMENT { ; STATEMENT } end` | (无，纯结构) |
| 268-299 | `STATEMENT()` | 分发 TOK_ID→EVA / TOK_IF→IF / TOK_WHILE→WHILE / TOK_WRITE→sem_emit_write / TOK_BEGIN→COM | sem_emit_write |
| 302-323 | `EVA_SENTENCE()` | `id := EXPRESSION` | sem_emit_assign |
| 326-355 | `IF_STATEMENT()` | `if EXPRESSION then STATEMENT [else STATEMENT]` | sem_if_begin / then_label / then_end / false_label / end_label |
| 358-381 | `WHILE_STATEMENT()` | `while EXPRESSION do STATEMENT` | sem_while_begin / loop_label / check / body_label / end |

## 公开接口（383-421 行）

| 行号 | 代码 | 讲解 |
|------|------|------|
| 385-388 | `parser_init()` | 重置 peek_valid |
| 390-421 | `parser_parse()` | 两遍编译：① scanner_scan_all → ② 保存 token_count → scanner_init 重置 → 恢复计数 → 清零 temp/label/quad/sem → next_token → PROGRAM → 检查 EOF |
