# parser.h 逐行讲解

## 文件头 — 条件编译与包含 (第1—3行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef PARSER_H` | 头文件防护宏开始，防止重复包含（include guard）。如果 `PARSER_H` 未定义则继续，否则跳过整个文件。 |
| 2 | `#define PARSER_H` | 定义宏 `PARSER_H`，标记本头文件已被包含，后续再遇到 `#include "parser.h"` 将直接跳过。 |
| 3 | `#include "grammar.h"` | 引入公共类型定义头文件。该文件包含了 `Compiler` 结构体、`SemValue` 语义值、`Token` 结构体、所有 Token 码常量（如 `TOK_PROGRAM`=3, `TOK_IF`=21）、四元式操作码（如 `OP_ASSIGN`=2, `OP_JNZ`=8）、符号种类/类型码等。parser 的所有子程序都依赖这些定义。 |

## 模块注释 (第5—17行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 5 | `// * parser.h -- syntax and semantic analysis` | 模块说明：语法分析与语义分析。parser 是编译器的核心模块，负责将 Token 序列转化为中间代码（四元式序列）。 |
| 6 | `// *` | 分隔空行，增强注释可读性。 |
| 7 | `// * Parsing strategy:` | 解析策略小节标题。 |
| 8 | `// *   - Statements: recursive descent (PROGRAM, SUB_PROGRAM, VARIABLE,` | 语句解析采用递归下降法。递归下降是一种自顶向下的语法分析方法，每个非终结符对应一个子程序。 |
| 9 | `// *     ID_SEQUENCE, TYPE, COM_SENTENCE, STATEMENT, EVA_SENTENCE,` | 列出所有递归下降子程序名称：PROGRAM（程序）、SUB_PROGRAM（子程序）、VARIABLE（变量声明）、ID_SEQUENCE（标识符序列）、TYPE（类型）、COM_SENTENCE（复合语句）、STATEMENT（语句）、EVA_SENTENCE（赋值语句）。 |
| 10 | `// *     IF_STATEMENT, WHILE_STATEMENT)` | 扩展的 IF/WHILE 语句子程序。原文档的 STATEMENT 仅包含 EVA_SENTENCE，这里扩展支持条件与循环。 |
| 11 | `// *   - Expressions: recursive descent (7 precedence levels)` | 表达式也采用递归下降法，按 7 级优先级分层。 |
| 12 | `// *` | 分隔。 |
| 13 | `// * Semantic actions (translation grammar):` | 语义动作小节标题。翻译文法将语义动作嵌入语法产生式中，在识别语法成分时同步生成中间代码。 |
| 14 | `// *   - VAR declaration: a1-a6 semantic routines` | 变量声明的语义动作：a1（初始化偏移）→ a2（压栈标识符）→ a3/a4/a5（设置类型和宽度）→ a6（弹栈、填写符号表、分配活动记录偏移）。 |
| 15 | `// *   - Assignment: generate (:=, src, _, dst)` | 赋值语句语义动作：生成赋值四元式，形如 `(:=, 表达式结果, _, 目标变量名)`。 |
| 16 | `// *   - Expression: generate arithmetic/logic quadruples` | 表达式语义动作：在每个优先级层次为算术和逻辑运算生成对应的四元式。 |
| 17 | `// *   - IF/WHILE: generate jump quadruples + backpatching` | IF/WHILE 语义动作：生成跳转四元式（jnz, jmp），并通过 label 机制实现回填（backpatching）。 |

## 函数声明 (第19—20行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 19 | `void parser_init(Compiler *c);` | 初始化语法分析器。设置 `peek_valid = 0`（预读标志复位），为新一轮解析做准备。参数 `c` 是编译器上下文指针，包含符号表、四元式序列等所有全局状态。调用时机：每次开始语法分析前。 |
| 20 | `int  parser_parse(Compiler *c);` | 执行语法分析和语义分析（两遍扫描架构）。返回 1 表示成功，0 表示有语法错误。内部流程：(1) 调用 `scanner_scan_all` 进行第一遍词法分析；(2) 保存 `token_count`；(3) 调用 `scanner_init` 重置扫描器；(4) 恢复 `token_count`；(5) 重置临时计数；(6) 调用 `PROGRAM` 启动递归下降；(7) 检查文件末尾。 |

## 文件尾 (第22行)

| 行号 | 代码 | 讲解 |
|------|------|------|
| 22 | `#endif` | 头文件防护宏结束，与第 1 行的 `#ifndef PARSER_H` 配对。 |
