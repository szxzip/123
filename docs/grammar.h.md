## 模块: grammar.h — 全局定义 (Token/四元式/符号表/结构体)

# grammar.h 逐行详解

本文档对 `compiler/src/grammar.h` 进行逐行（或逻辑分组）的详细解释，涵盖每条 `#define`、`enum`、`typedef struct`、`extern` 声明及 `inline` 函数的作用与设计意图。

---

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef GRAMMAR_H` | 头文件保护宏的起始。`ifndef` 即 "if not defined"——如果 `GRAMMAR_H` 尚未被定义过，则继续处理以下内容；否则跳过整个文件到 `#endif`。这是防止头文件被重复 `#include` 导致重定义错误的标准 C 语言惯用法。 |
| 2 | `#define GRAMMAR_H` | 定义宏 `GRAMMAR_H`。它本身不展开为任何值，仅作为"已包含此头文件"的标记。后续再次 `#include "grammar.h"` 时，第 1 行的 `#ifndef` 检测到该宏已定义，将跳过全部内容。 |
| 3 | （空行） | 分隔符，无逻辑含义。 |
| 4 | `#include <ctype.h>` | 引入 C 标准库 `<ctype.h>`，提供字符分类函数：`isalpha()`（判断字母）、`isdigit()`（判断数字）、`isalnum()`（判断字母或数字）、`isspace()`（判断空白字符）等。这里的 `scanner.c` 词法分析器在识别标识符和数字常量时会用到这些函数。放在头文件中是因为 `Compiler` 上下文的消费者可能也需要这些声明。 |
| 5 | `#include <stdio.h>` | 引入 C 标准 I/O 库 `<stdio.h>`，提供 `printf`、`fopen`、`fgets` 等函数。编译器输出 Token 序列、符号表、四元式列表时用到 `printf`；`main.c` 读取源文件时用到 `fopen`/`fgets`。 |
| 6 | `#include <stdlib.h>` | 引入 C 标准库 `<stdlib.h>`，提供 `malloc`、`free`、`exit`、`atoi` 等。`main.c` 中可能用 `malloc` 分配源程序缓冲区，用 `exit` 处理错误退出。 |
| 7 | `#include <string.h>` | 引入 C 字符串库 `<string.h>`，提供 `strcmp`、`strcpy`、`strlen` 等。符号表查找标识符名称时使用 `strcmp`；构造四元式字符串参数时使用 `strcpy`。 |
| 8 | （空行） | 分隔符。 |
| 9 | `#define MAX_SYMBOLS 256   // 符号表最大容量` | 定义宏 `MAX_SYMBOLS` 为 256。符号表是一个固定大小的数组 `sym_table[MAX_SYMBOLS]`，最多可存储 256 个符号（变量名、程序名、临时变量）。选择 256 是因为：① 对教学编译器足够大；② 2 的幂便于索引和内存对齐；③ 恰好一个字节可表示 0~255 的索引（虽然实际 `sym_count` 用 `int`）。注释中"符号表最大容量"说明了用途。 |
| 10 | `#define MAX_CONSTANTS 256 // 常数表最大容量` | 定义宏 `MAX_CONSTANTS` 为 256。常数表 `const_table[]` 存储词法分析阶段识别出的实数和整数常量的值。同样取 256，与符号表对称，对于教学级 Pascal 子集编译器完全够用。 |
| 11 | `#define MAX_QUADS 1024    // 四元式最大条数` | 定义宏 `MAX_QUADS` 为 1024。四元式是中间代码的基本单元，格式为 `(op, arg1, arg2, result)`。1024 条四元式对应大约 1024 条中间指令，对于教学用的小型 Pascal 程序足够。取 1024（2^10）而非 1000 是为了 2 的幂对齐。`quadruple.c` 中的 `emit()` 函数在生成四元式前会检查 `quad_count < MAX_QUADS`。 |
| 12 | `#define MAX_TOKENS 2048   // Token 序列最大长度` | 定义宏 `MAX_TOKENS` 为 2048。在词法分析阶段，`scanner_scan_all()` 将整个源程序扫描为 Token 序列并存入 `token_list[]`。2048 意味着源程序最多可含约 2000 个词法单元。两遍扫描架构下，Token 序列必须全部存入内存供语法分析回读。取 2048（2^11）是因为 Token 数量通常远多于四元式数量（一个表达式可能产生多个 Token 但只生成少量四元式）。 |
| 13 | `#define MAX_LINE 256      // 源程序单行最大长度` | 定义宏 `MAX_LINE` 为 256。`main.c` 中读取源程序时，用 `fgets(buffer, MAX_LINE, fp)` 逐行读入。256 字符的单行长度对于 Pascal 代码绰绰有余。同样取 2 的幂值。 |
| 14 | `#define MAX_NAME 32       // 标识符最大长度` | 定义宏 `MAX_NAME` 为 32。Pascal 标准中标识符长度没有硬限制，但教学编译器取 32 足够容纳常见变量名（如 `counter`、`temperature` 等）。`Symbol.name` 和 `SemValue.name` 字符数组均用此宏定义长度。 |
| 15 | `#define MAX_STR 64        // 通用字符串长度` | 定义宏 `MAX_STR` 为 64。用于 `Quadruple` 结构体中 `arg1`、`arg2`、`result` 三个字符串字段。64 字节足够容纳像 `"_t123"`、`"label_5"` 这类自动生成的临时名/标号名以及标识符名，且留有余量。 |
| 16 | （空行） | 分隔符。 |
| 17 | `// ========== Token 类型码 ==========` | 注释：标识接下来这段枚举是 Token（词法单元）的类型码定义。Token 类型码在词法分析器（`scanner.c`）中赋值给 `Token.code`，在语法分析器（`parser.c`）中用作 `switch`/`if` 判断当前单词类型。 |
| 18 | `enum {` | 匿名枚举的起始。C 语言中匿名枚举等价于定义一组整数常量，不创建新类型名。这里用 `enum` 而不用 `#define` 的好处是：① 值自动递增；② 调试器中可显示符号名；③ 类型安全稍好（编译器可能给出隐式转换警告）。 |
| 19 | `TOK_EOF = 0,   // 文件结束` | 定义 `TOK_EOF = 0`。EOF = End Of File，文件结束标记。值为 0 是因为：① 0 是 C 语言中最常见的"假/空/结束"哨兵值；② 全局变量默认初始化为 0，`Token.code` 初始即为 EOF；③ 语法分析器常用 `while (token.code != TOK_EOF)` 作为解析循环条件。当词法分析器扫描到源程序末尾时，向 Token 序列追加此标记。 |
| 20 | `TOK_ID = 1,    // 标识符` | 定义 `TOK_ID = 1`。ID = Identifier，标识符（变量名、程序名）。值为 1 区别于 0（EOF）。当词法分析器的 DFA 识别出一个以字母开头、由字母和数字组成的字符串且它不是关键字时，返回此码。此时 `Token.value` 中存储该标识符在符号表 `sym_table[]` 中的索引。 |
| 21 | `TOK_CONST = 2, // 常数` | 定义 `TOK_CONST = 2`。CONST = Constant，数字常量（整数或实数）。值为 2。当词法分析器识别出一个数字序列时返回此码。此时 `Token.value` 中存储该常量在 `const_table[]` 中的索引，且 `Token.real_val` 或 `Token.int_val` 中直接保存数值（用于表达式求值时快速获取）。 |
| 22 | `// 关键字 (index+3, 与文档中的 Keys 表一致)` | 注释：说明从第 23 行起的关键字 Token 码等于该关键字在 `keys_table[]` 中的索引加 3。这是整个 Token 编码体系的核心约定——详见下方各条。 |
| 23 | `TOK_PROGRAM = 3,` | 定义 `TOK_PROGRAM = 3`。关键字 `program` 的 Token 码。值 3 = 0（keys_table 第 0 项） + 3。语法分析器在 `parser_parse()` 中首先检查 `token.code == TOK_PROGRAM`，匹配 `PROGRAM → program id SUB_PROGRAM .` 产生式。 |
| 24 | `TOK_VAR = 4,` | 定义 `TOK_VAR = 4`。关键字 `var` 的 Token 码。值 4 = 1 + 3。声明变量区时检查此码，对应 `VARIABLE → var ID_SEQ : TYPE ;` 产生式。 |
| 25 | `TOK_INTEGER = 5,` | 定义 `TOK_INTEGER = 5`。关键字 `integer` 的 Token 码。值 5 = 2 + 3。出现在类型声明 `: integer` 中，触发语义动作将当前类型设为 `TY_INTEGER`。 |
| 26 | `TOK_REAL = 6,` | 定义 `TOK_REAL = 6`。关键字 `real` 的 Token 码。值 6 = 3 + 3。类似 `integer`，对应类型 `TY_REAL`（8 字节浮点）。 |
| 27 | `TOK_CHAR = 7,` | 定义 `TOK_CHAR = 7`。关键字 `char` 的 Token 码。值 7 = 4 + 3。对应类型 `TY_CHAR`（1 字节字符）。 |
| 28 | `TOK_BEGIN = 8,` | 定义 `TOK_BEGIN = 8`。关键字 `begin` 的 Token 码。值 8 = 5 + 3。复合语句的起始标记，对应 `COM_SENTENCE → begin STATEMENT { ; STATEMENT } end`。 |
| 29 | `TOK_END = 9,` | 定义 `TOK_END = 9`。关键字 `end` 的 Token 码。值 9 = 6 + 3。复合语句结束标记，同时也是整个程序的结束（`PROGRAM → program id SUB_PROGRAM .` 中 `SUB_PROGRAM` 以复合语句结束）。 |
| 30 | `// 界符` | 注释：界符（分隔符/运算符），它们不是关键字，不在 `keys_table` 中。从 10 开始独立编码。 |
| 31 | `TOK_COMMA = 10,` | 定义 `TOK_COMMA = 10`。逗号 `,` 的 Token 码。用于变量声明中分隔多个标识符：`ID_SEQ → id { , id }`。当词法分析器遇到字符 `,` 时直接返回此码，无需查表。 |
| 32 | `TOK_COLON = 11,` | 定义 `TOK_COLON = 11`。冒号 `:` 的 Token 码。用于类型声明：`VARIABLE → var ID_SEQ : TYPE ;`。词法分析器遇到 `:` 且下一个字符不是 `=` 时返回此码。 |
| 33 | `TOK_SEMICOLON = 12,` | 定义 `TOK_SEMICOLON = 12`。分号 `;` 的 Token 码。作为语句分隔符，出现在变量声明末尾和复合语句中多语句之间。 |
| 34 | `TOK_ASSIGN = 13, // :=` | 定义 `TOK_ASSIGN = 13`。赋值号 `:=` 的 Token 码。这是 Pascal 特有的赋值运算符（区别于比较用的 `=`）。词法分析器遇到 `:` 且下一个字符是 `=` 时返回此码。语法分析器在 `EVA → id := EXPRESSION` 产生式中匹配此 Token，并生成 `OP_ASSIGN` 四元式。 |
| 35 | `TOK_MUL = 14,` | 定义 `TOK_MUL = 14`。乘号 `*` 的 Token 码。表达式解析中优先级为 L6（最高算术优先级），对应四元式 `OP_MUL`。 |
| 36 | `TOK_DIV = 15,` | 定义 `TOK_DIV = 15`。除号 `/` 的 Token 码。同样 L6 优先级，对应 `OP_DIV`。注意：Pascal 中 `/` 总是产生实数结果，而 `div` 是整数除（本编译器不在扩展关键字中）。此处设计与课程文档一致。 |
| 37 | `TOK_PLUS = 16,` | 定义 `TOK_PLUS = 16`。加号 `+` 的 Token 码。L5 优先级，对应 `OP_ADD`。 |
| 38 | `TOK_MINUS = 17,` | 定义 `TOK_MINUS = 17`。减号 `-` 的 Token 码。L5 优先级，对应 `OP_SUB`。也用于一元负号（`parser.c` 表达式解析 L7 层处理 `- factor`）。 |
| 39 | `TOK_DOT = 18,` | 定义 `TOK_DOT = 18`。句点 `.` 的 Token 码。在 Pascal 中，`.` 是程序结束标记：`PROGRAM → program id SUB_PROGRAM .`。语法分析器在顶层产生式末尾匹配此 Token。 |
| 40 | `TOK_LPAREN = 19,` | 定义 `TOK_LPAREN = 19`。左括号 `(` 的 Token 码。用于表达式分组 `(EXPRESSION)` 改变优先级。 |
| 41 | `TOK_RPAREN = 20,` | 定义 `TOK_RPAREN = 20`。右括号 `)` 的 Token 码。与左括号配对使用。 |
| 42 | `// 扩展关键字: If/While/逻辑运算` | 注释：从 21 开始是课程扩展的关键字和运算符，包括流程控制（if/while）和逻辑运算（and/or/not）。这些在 `keys_table[]` 数组中索引为 18~25，加上偏移 3 后等于 21~28。 |
| 43 | `TOK_IF = 21,` | 定义 `TOK_IF = 21`。关键字 `if` 的 Token 码。值 21 = 18 + 3。语法分析器匹配 `IF → if EXPRESSION then STATEMENT [ else STATEMENT ]`。语义动作中生成条件跳转四元式。 |
| 44 | `TOK_THEN = 22,` | 定义 `TOK_THEN = 22`。关键字 `then` 的 Token 码。值 22 = 19 + 3。作为 if 语句中条件表达式和 then 分支之间的分隔符。 |
| 45 | `TOK_ELSE = 23,` | 定义 `TOK_ELSE = 23`。关键字 `else` 的 Token 码。值 23 = 20 + 3。可选的 else 分支。语义动作中需要回填（backpatch）跳转目标。 |
| 46 | `TOK_WHILE = 24,` | 定义 `TOK_WHILE = 24`。关键字 `while` 的 Token 码。值 24 = 21 + 3。匹配 `WHILE → while EXPRESSION do STATEMENT`。语义动作生成循环头和条件跳转。 |
| 47 | `TOK_DO = 25,` | 定义 `TOK_DO = 25`。关键字 `do` 的 Token 码。值 25 = 22 + 3。while 循环中条件表达式和循环体之间的分隔符。 |
| 48 | `TOK_AND = 26,` | 定义 `TOK_AND = 26`。关键字 `and` 的 Token 码。值 26 = 23 + 3。逻辑与运算符，表达式 L2 优先级，对应 `OP_AND_OP`。 |
| 49 | `TOK_OR = 27,` | 定义 `TOK_OR = 27`。关键字 `or` 的 Token 码。值 27 = 24 + 3。逻辑或运算符，表达式 L1（最低）优先级，对应 `OP_OR_OP`。 |
| 50 | `TOK_NOT = 28,` | 定义 `TOK_NOT = 28`。关键字 `not` 的 Token 码。值 28 = 25 + 3。逻辑非运算符，一元，L3 优先级，对应 `OP_NOT_OP`。 |
| 51 | `// 比较运算符` | 注释：从 29 开始是六个比较运算符的 Token 码。它们是独立的 Token（两字符运算符如 `<=`、`<>` 等），不在 `keys_table` 中。值从 29 到 34。 |
| 52 | `TOK_EQ = 29,    // =` | 定义 `TOK_EQ = 29`。等于比较符 `=` 的 Token 码。注意与 `TOK_ASSIGN`（`:=`）的区别：Pascal 中 `=` 用于比较而非赋值。表达式 L4 优先级，对应 `OP_JE`。 |
| 53 | `TOK_LT = 30,    // <` | 定义 `TOK_LT = 30`。小于 `<` 的 Token 码。对应 `OP_JL`。 |
| 54 | `TOK_GT = 31,    // >` | 定义 `TOK_GT = 31`。大于 `>` 的 Token 码。对应 `OP_JG`。 |
| 55 | `TOK_LE = 32,    // <=` | 定义 `TOK_LE = 32`。小于等于 `<=` 的 Token 码。对应 `OP_JLE`。 |
| 56 | `TOK_GE = 33,    // >=` | 定义 `TOK_GE = 33`。大于等于 `>=` 的 Token 码。对应 `OP_JGE`。 |
| 57 | `TOK_NE = 34,    // <>` | 定义 `TOK_NE = 34`。不等于 `<>` 的 Token 码。对应 `OP_JNE`。Pascal 使用 `<>` 而非 C 语言的 `!=`。 |
| 58 | `TOK_WRITE = 35, // write 输出语句` | 定义 `TOK_WRITE = 35`。关键字 `write` 的 Token 码。值 35 = 32 + 3（在 `keys_table` 中索引为 32）。输出语句，对应 `OP_WRITE` 四元式。 |
| 59 | `TOK_NUM = 36` | 定义 `TOK_NUM = 36`。这是一个特殊的 Token 码，用作常数自动机（DFA）的**内部状态标记**，表示"当前是一个数字"。词法分析器 `scanner.c` 中的 `const_aut[][]` 状态转换矩阵使用此值标记接受状态。它不是外部可见的 Token 码——词法分析器识别完数字后返回的是 `TOK_CONST`(值 2)，而 `TOK_NUM`(值 36) 仅存在于自动机的状态表中。选择 36 是为了不与前述任何 Token 码（0~35）冲突。 |
| 60 | `};` | 匿名枚举结束。 |
| 61 | （空行） | 分隔符。 |
| 62 | `// ========== 四元式操作码 ==========` | 注释：四元式操作码枚举。四元式是编译器中常用的三地址中间表示（TAC），格式为 `(操作码, 参数1, 参数2, 结果)`。本编译器从 1 开始编号（非 0），因为 0 常用于表示"无效/空操作"。 |
| 63 | `enum {` | 四元式操作码匿名枚举开始。 |
| 64 | `OP_PROGRAM = 1, // (program, id, _, _)` | 定义 `OP_PROGRAM = 1`。程序入口伪操作。四元式格式 `(program, id, _, _)`，其中 `arg1` 为程序名标识符，`arg2` 和 `result` 为下划线 `_` 表示未使用。`parser_parse()` 在匹配 `PROGRAM → program id` 后调用 `emit(OP_PROGRAM, id_name, "_", "_")` 生成此指令。用于标记程序起始和符号表入口。 |
| 65 | `OP_ASSIGN = 2,  // (:=, src, _, dst)` | 定义 `OP_ASSIGN = 2`。赋值操作。四元式格式 `(:=, src, _, dst)`，`arg1` 为源操作数（表达式的结果变量名），`result` 为目标变量名。`arg2` 为 `_` 未使用。parser 在 `EVA → id := EXPRESSION` 的语义动作中生成。 |
| 66 | `OP_ADD = 3,     // (+, a1, a2, r)` | 定义 `OP_ADD = 3`。加法运算。`arg1` 和 `arg2` 为两个操作数，`result` 为结果（通常为临时变量 `_tN`）。表达式 L5 层处理加减法时生成。 |
| 67 | `OP_SUB = 4,     // (-, a1, a2, r)` | 定义 `OP_SUB = 4`。减法运算。格式同加法。 |
| 68 | `OP_MUL = 5,     // (*, a1, a2, r)` | 定义 `OP_MUL = 5`。乘法运算。表达式 L6 层（最高算术优先级）处理乘除时生成。 |
| 69 | `OP_DIV = 6,     // (/, a1, a2, r)` | 定义 `OP_DIV = 6`。除法运算。格式同乘法。 |
| 70 | `OP_JMP = 7,     // (jmp, _, _, label)` | 定义 `OP_JMP = 7`。无条件跳转。`result` 为跳转目标标号（如 `label_1`），`arg1` 和 `arg2` 为 `_` 未使用。用于 if-else 结构中跳过 then/else 分支、while 循环中跳回循环头等。 |
| 71 | `OP_JNZ = 8,     // (jnz, cond, _, label)` | 定义 `OP_JNZ = 8`。条件为真时跳转（Jump if Non-Zero）。`arg1` 为条件变量名（通常是比较运算结果存放的临时变量），`result` 为跳转目标标号。用于 if 和 while 的条件分支：当条件为真（非零）时跳入 then 分支或循环体。 |
| 72 | `OP_JE = 9,      // (je, a1, a2, label)` | 定义 `OP_JE = 9`。等于时跳转（Jump if Equal）。比较 `arg1 == arg2`，若为真则跳转到 `result` 标号。六个比较跳转指令中，每个对应一个比较运算符。表达式 L4 层生成比较四元式时，先用比较操作（如 `OP_JE`）将布尔结果存入临时变量，然后在 if/while 的语义动作中使用 `OP_JNZ` 检查该临时变量。 |
| 73 | `OP_JNE = 10,    // (jne, a1, a2, label)` | 定义 `OP_JNE = 10`。不等于时跳转。对应 `<>`。 |
| 74 | `OP_JL = 11,     // (jl, a1, a2, label)` | 定义 `OP_JL = 11`。小于时跳转。对应 `<`。 |
| 75 | `OP_JG = 12,     // (jg, a1, a2, label)` | 定义 `OP_JG = 12`。大于时跳转。对应 `>`。 |
| 76 | `OP_JLE = 13,    // (jle, a1, a2, label)` | 定义 `OP_JLE = 13`。小于等于时跳转。对应 `<=`。 |
| 77 | `OP_JGE = 14,    // (jge, a1, a2, label)` | 定义 `OP_JGE = 14`。大于等于时跳转。对应 `>=`。 |
| 78 | `OP_AND_OP = 15, // (and, a1, a2, r)` | 定义 `OP_AND_OP = 15`。逻辑与操作。命名为 `OP_AND_OP` 而非 `OP_AND` 是因为 `<ctype.h>` 和某些系统头文件中 `AND` 可能已被定义为宏（实际上 C 标准库中有 `and` 作为 C++ 替代拼写），加 `_OP` 后缀避免命名冲突。 |
| 79 | `OP_OR_OP = 16,  // (or, a1, a2, r)` | 定义 `OP_OR_OP = 16`。逻辑或操作。同样加 `_OP` 后缀避免与系统宏（如 `<iso646.h>` 中的 `or`）冲突。 |
| 80 | `OP_NOT_OP = 17, // (not, a1, _, r)` | 定义 `OP_NOT_OP = 17`。逻辑非操作。一元运算，`arg1` 为源操作数，`arg2` 为 `_`，`result` 为结果。 |
| 81 | `OP_END = 18,    // (end, _, _, _)` | 定义 `OP_END = 18`。程序结束伪操作。`arg1`、`arg2`、`result` 均为 `_`。parser 在匹配 `PROGRAM → program id SUB_PROGRAM .` 中的 `.` 时生成，标记四元式序列的结束。 |
| 82 | `OP_LABEL = 19,  // (label, _, _, _)` | 定义 `OP_LABEL = 19`。标号定义伪操作。`result` 存储标号名称（如 `label_1`），`arg1` 和 `arg2` 为 `_`。用于 if/while 结构中标记跳转目标位置。`symbol.c` 中的 `new_label()` 函数分配唯一的标号名，`quadruple.c` 中的 `emit_label()` 函数生成此操作。 |
| 83 | `OP_WRITE = 20   // (write, id, _, _)` | 定义 `OP_WRITE = 20`。输出操作。`arg1` 为要输出的变量名，`arg2` 和 `result` 为 `_`。对应 Pascal 的 `write` 语句。 |
| 84 | `};` | 四元式操作码匿名枚举结束。 |
| 85 | （空行） | 分隔符。 |
| 86 | `// ========== 符号种类 ==========` | 注释：符号表中每个条目的 `kind` 字段取以下三种值之一，用于区分符号的角色。 |
| 87 | `enum { KIND_PROGRAM = 0, KIND_VARIABLE = 1, KIND_TEMP = 2 };` | 定义三种符号种类。`KIND_PROGRAM = 0`：程序名符号，符号表第 0 项总是程序名（`symbol.c` 的 `enter()` 首先被调用来注册程序名）。`KIND_VARIABLE = 1`：用户声明的普通变量。`KIND_TEMP = 2`：编译器自动生成的临时变量（如 `_t0`、`_t1`），用于存储表达式中间结果。选择 0/1/2 作为连续整数是为了方便数组索引和 `switch` 语句。 |
| 88 | （空行） | 分隔符。 |
| 89 | `// ========== 类型码 ==========` | 注释：类型码枚举，用于 `Symbol.type` 和 `Compiler.cur_type` 字段。 |
| 90 | `enum { TY_INTEGER = 0, TY_REAL = 1, TY_CHAR = 2 };` | 定义三种数据类型。`TY_INTEGER = 0`：整型，宽度 4 字节。`TY_REAL = 1`：实型（浮点），宽度 8 字节。`TY_CHAR = 2`：字符型，宽度 1 字节。选择连续整数 0/1/2 便于：① `type_len()` 函数中快速查表（实际用三元运算符）；② `switch` 语句分发；③ 活动记录偏移计算。 |
| 91 | （空行） | 分隔符。 |
| 92 | `// ========== Token 结构体 ==========` | 注释：Token 结构体定义。每个词法单元用一个 Token 实例表示。 |
| 93 | `typedef struct {` | 使用 `typedef` 定义结构体并同时赋予类型名 `Token`。此后代码中可直接写 `Token t;` 而不必写 `struct token_t t;`。注意此处结构体本身是匿名的（没有 tag 名），只有 typedef 名 `Token`。 |
| 94 | `int code;        // 单词类别码` | 字段 `code`：存储 Token 的类型码，取值来自第 18~59 行的匿名枚举（`TOK_EOF`=0, `TOK_ID`=1, `TOK_PROGRAM`=3 等）。语法分析器通过 `c->token.code` 判断当前读到的单词类型，并用 `switch`/`if` 分支处理不同产生式。词法分析器 `scanner.c` 在 `scanner_next()` 中设置此字段。 |
| 95 | `int value;       // 符号表/常数表索引` | 字段 `value`：当 `code == TOK_ID` 时，此字段存储该标识符在 `sym_table[]` 中的索引；当 `code == TOK_CONST` 时，此字段存储该常量在 `const_table[]` 中的索引。语法分析和语义动作用此索引来获取符号名/类型或常数值。对于关键字和界符 Token，此字段无意义（通常为 0 或未使用）。 |
| 96 | `double real_val; // 实常数数值 (用于 CONST token)` | 字段 `real_val`：当 `code == TOK_CONST` 时，如果该常量是实数（包含小数点），`scanner.c` 直接将数值写入此字段。这样表达式求值时无需再查常数表，可直接读取。注意：`real_val` 是 8 字节 double，即使常量是整数也会存储在此（或同时存储在 `int_val`）。 |
| 97 | `int int_val;     // 整常数值` | 字段 `int_val`：当 `code == TOK_CONST` 时，如果该常量是纯整数（不含小数点），词法分析器同时将整数值写入此字段。设计上冗余存储了 `real_val` 和 `int_val`，因为 C 语言中 `double` 可精确表示 32 位以内的整数，只存一个就够了——存两个是为了兼容性：某些语义动作可能更喜欢用 `int`。 |
| 98 | `} Token;` | Token 结构体定义结束，typedef 名 `Token`。 |
| 99 | （空行） | 分隔符。 |
| 100 | `// ========== 符号表条目 ==========` | 注释：符号表条目结构体定义。符号表是编译器前端最重要的数据结构之一，记录程序中每个标识符的属性。 |
| 101 | `typedef struct {` | 符号表结构体定义开始。 |
| 102 | `char name[MAX_NAME]; // 标识符名` | 字段 `name`：标识符的字符串名称，最大长度 `MAX_NAME`（32 字节）。符号表查找 `lookup()` 通过 `strcmp` 与此字段比较来检索标识符。`enter()` 注册新符号时将词法分析器读到的标识符字符串 `strcpy` 到此字段。 |
| 103 | `int type;            // TY_INTEGER / TY_REAL / TY_CHAR` | 字段 `type`：数据类型，取值为 `TY_INTEGER`(0)、`TY_REAL`(1) 或 `TY_CHAR`(2)。在变量声明 `ID_SEQ : TYPE ;` 的语义动作 a6（PPT 第 19-20 页）中，批量为此前通过语义栈收集的标识符设置此字段。类型检查、表达式类型转换、`type_len()` 宽度计算均依赖此字段。 |
| 104 | `int kind;            // KIND_PROGRAM / KIND_VARIABLE / KIND_TEMP` | 字段 `kind`：符号种类，取值为 `KIND_PROGRAM`(0)、`KIND_VARIABLE`(1) 或 `KIND_TEMP`(2)。`KIND_TEMP` 的符号由 `symbol.c` 中的 `new_temp()` 函数创建，其名称格式为 `_tN`（N 为 `temp_count` 递增值）。代码生成时可根据 kind 区分用户变量和临时变量。 |
| 105 | `int offset;          // 活动记录偏移` | 字段 `offset`：该变量在运行时活动记录（Activation Record）中的偏移量。变量声明的语义动作 a6 按声明顺序为每个变量分配偏移值（`cur_offset` 递增）。例如第一个整型变量 offset=0，第二个整型变量 offset=4，然后是实型变量 offset=8… 最终代码生成时，通过 `offset` 来生成正确的内存访问地址。临时变量也需 offset，但它们的偏移在变量之后或单独计算。 |
| 106 | `int len;             // 类型宽度` | 字段 `len`：该变量类型对应的字节宽度。`TY_INTEGER` → 4，`TY_REAL` → 8，`TY_CHAR` → 1。在分配 offset 时，`cur_offset += len` 来前进到下一个变量的偏移位置。此值与 `type_len(type)` 的返回值相同，冗余存储是为了加速——避免每次需要宽度时都调用函数。 |
| 107 | `} Symbol;` | Symbol 结构体定义结束，typedef 名 `Symbol`。 |
| 108 | （空行） | 分隔符。 |
| 109 | `// ========== 四元式 ==========` | 注释：四元式结构体定义。四元式是中间代码的基本表示，格式为 `(op, arg1, arg2, result)`。 |
| 110 | `typedef struct {` | 四元式结构体定义开始。 |
| 111 | `int op; // 操作码` | 字段 `op`：操作码，取自四元式操作码枚举（`OP_PROGRAM`=1 到 `OP_WRITE`=20）。`quadruple.c` 的 `emit()` 函数接收此值并存储到数组中。`dump_quads()` 输出时，用 `op_names[op]` 获取操作码的字符串名称。 |
| 112 | `char arg1[MAX_STR];` | 字段 `arg1`：第一个参数，字符串形式，最大长度 `MAX_STR`(64)。对于赋值操作，它存储源操作数变量名；对于二元运算，它是左操作数；对于跳转操作，它可能是条件变量。当参数未使用时填充 `"_"`。 |
| 113 | `char arg2[MAX_STR];` | 字段 `arg2`：第二个参数，字符串形式。对于二元运算，它是右操作数；对于跳转操作，它可能是比较的第二操作数或 `"_"`。 |
| 114 | `char result[MAX_STR];` | 字段 `result`：结果字段，字符串形式。对于运算操作，它是存放结果的变量名（临时变量或目标变量）；对于跳转操作，它是跳转目标标号；对于标号定义 `OP_LABEL`，它是标号名。 |
| 115 | `} Quadruple;` | 四元式结构体定义结束，typedef 名 `Quadruple`。 |
| 116 | （空行） | 分隔符。 |
| 117 | `// ========== 语义值 (用于表达式求值时传递) ==========` | 注释：语义值结构体。在递归下降的表达式解析过程中，每个子表达式解析完成后需要向上一级返回其"值"——即变量的名称和性质。此结构体封装了这些信息。 |
| 118 | `typedef struct {` | SemValue 结构体定义开始。 |
| 119 | `char name[MAX_NAME];` | 字段 `name`：表达式/运算结果的变量名称。如果是标识符引用，存储标识符原名；如果子表达式生成了四元式并分配了临时变量，存储临时变量名（如 `_t0`）。上层解析器通过此名引用上一级的结果来构造更高层的四元式。 |
| 120 | `int is_temp; // 1=临时变量, 0=普通标识符/常数` | 字段 `is_temp`：标记 `name` 字段指向的是临时变量（1）还是普通变量/常量（0）。在生成赋值四元式（`EVA → id := EXPRESSION`）时，如果 `is_temp == 1`，则源操作数是临时变量，否则是用户变量。在比较操作中，此标志也有助于区分中间结果和字面量。 |
| 121 | `} SemValue;` | SemValue 结构体定义结束，typedef 名 `SemValue`。 |
| 122 | （空行） | 分隔符。 |
| 123 | `// ========== 全局编译器上下文 ==========` | 注释：Compiler 结构体是整个编译器前端的全局状态容器。所有模块（`scanner.c`、`parser.c`、`symbol.c`、`quadruple.c`）通过一个共享的 `Compiler *c` 指针来访问和修改编译器状态。类似面向对象中的"单例"模式，但用显式传参实现。 |
| 124 | `typedef struct {` | Compiler 结构体定义开始。 |
| 125 | `// 词法分析` | 注释：以下字段属于词法分析器（`scanner.c`）使用的状态。 |
| 126 | `char *source;                 // 源程序字符串` | 字段 `source`：指向源程序完整文本的字符指针。`main.c` 读取文件后，将所有行拼接成一个以 `\0` 结尾的 C 字符串，由 `c->source` 指向。词法分析器的 `scanner_next()` 通过 `c->source[c->pos]` 逐字符扫描。此字符串在编译器退出前需 `free()` 释放。 |
| 127 | `int pos;                      // 当前读取位置` | 字段 `pos`：词法分析器在 `source` 字符串中的当前读取位置（字符索引，0 起始）。`scanner_next()` 每次读取一个字符时 `pos++` 前移，直到 `pos >= len` 时返回 `TOK_EOF`。 |
| 128 | `int len;                      // 源程序总长度` | 字段 `len`：`source` 字符串的总字符数（不含末尾 `\0`）。由 `main.c` 在拼接源程序后设置。词法分析器用 `pos < len` 判断是否还有字符可读。 |
| 129 | `char ch;                      // 当前字符` | 字段 `ch`：词法分析器当前正在处理的字符（即 `source[pos]`）。词法分析器的 DFA 状态机根据 `ch` 的值决定状态转移。读取新字符时调用 `scanner_next_char()` 将 `ch = source[++pos]`。 |
| 130 | `Token token;                  // 当前 Token` | 字段 `token`：语法分析器当前正在处理的 Token。在第一遍扫描后，`token_list[]` 已填充完毕；第二遍语法分析时，`parser.c` 从 `token_list[]` 中按序取出 Token 并复制到此字段。语法分析器解析产生式时直接检查 `c->token.code`、`c->token.value` 等。 |
| 131 | `Token token_list[MAX_TOKENS]; // Token 序列 (词法分析输出)` | 字段 `token_list[]`：Token 序列数组，容量 `MAX_TOKENS`(2048)。第一遍词法扫描 `scanner_scan_all()` 将所有 Token 按顺序存入此数组，第二遍语法分析 `parser_parse()` 从此数组中读取。与直接从源程序实时取 Token 相比，这种"先扫描全部再分析"的两遍架构简化了错误恢复和符号表预处理。 |
| 132 | `int token_count;` | 字段 `token_count`：`token_list[]` 中有效 Token 的数量。`scanner_scan_all()` 扫描完毕后设置此值。语法分析器用索引 `c->pos`（注意：此处的 `pos` 在语法分析阶段复用为 Token 序列的读取位置）遍历 `token_list[]`，当索引达到 `token_count` 时表示已读完所有 Token。根据 AGENTS.md 的说明，`token_count` 必须在重置前保存，因为 `scanner_init()` 会将其清零。 |
| 133 | （空行） | 空行，用于逻辑分隔。 |
| 134 | `// 符号表` | 注释：以下字段属于符号表（`symbol.c`）使用的状态。 |
| 135 | `Symbol sym_table[MAX_SYMBOLS];` | 字段 `sym_table[]`：符号表数组，容量 `MAX_SYMBOLS`(256)，每个元素为 `Symbol` 结构体。符号按注册顺序填入：第 0 项固定为程序名（`KIND_PROGRAM`），后续依次为用户声明的变量和编译器生成的临时变量。查找函数 `lookup(name)` 遍历此数组，通过 `strcmp` 匹配 `name` 字段。线性查找对教学编译器足够（256 个符号的线性扫描开销可忽略）。 |
| 136 | `int sym_count;` | 字段 `sym_count`：符号表中当前已填充的条目数，同时是下一个新符号将插入的位置索引。`enter(name)` 函数返回 `sym_count++` 作为新符号的索引，然后将符号信息写入 `sym_table[sym_count-1]`。`lookup()` 遍历范围为 0 到 `sym_count - 1`。此变量初始为 0。 |
| 137 | `double const_table[MAX_CONSTANTS];` | 字段 `const_table[]`：常数表数组，容量 `MAX_CONSTANTS`(256)，存储 double 类型的常数值。与符号表不同，常数表只存数值（不存字段结构），因为常数没有名称/类型/种类等属性——常数的属性由 Token 携带（`TOK_CONST` + `real_val`/`int_val`）。`symbol.c` 的 `enter_const(value)` 函数检查常数是否已存在（无重复），不存在则追加到表尾并返回索引。 |
| 138 | `int const_count;` | 字段 `const_count`：常数表中当前已存储的常数个数（即下一个常数将插入的位置）。`enter_const()` 返回 `const_count++` 并存储新值。初始为 0。 |
| 139 | （空行） | 空行，用于逻辑分隔。 |
| 140 | `// 语义栈 (变量声明用)` | 注释：语义栈专用于变量声明 `VARIABLE → var ID_SEQ : TYPE ;` 的处理。根据课程 PPT 第 19-20 页，语义动作 a2 将每个新声明的标识符压入 `sem_stack[]`，动作 a6 按 FIFO 顺序弹出处理（设置 type、分配 offset）。 |
| 141 | `int sem_stack[MAX_SYMBOLS];` | 字段 `sem_stack[]`：语义栈数组，存储待处理标识符在符号表中的索引值。容量与符号表相同（`MAX_SYMBOLS`=256），因为最多所有符号都可能在某处被压栈。语义动作 a2（在 `ID_SEQ → id` 之后触发）执行 `c->sem_stack[++c->sem_top] = token.value`。语义动作 a6（在 `TYPE ;` 之后触发）while 循环依次弹出处理。 |
| 142 | `int sem_top;` | 字段 `sem_top`：语义栈的栈顶指针。初始值为 -1（栈空）。`a2` 压栈时 `sem_top += 1` 后写入；`a6` 弹出时读取后 `sem_top -= 1`。使用 `int` 而非 `unsigned` 以便用 -1 表示空栈。 |
| 143 | `int cur_type;   // 当前声明的类型` | 字段 `cur_type`：当前正在声明的变量类型。由语义动作 a4（在 `TYPE → integer/real/char` 之后）设置，例如遇到 `integer` → `c->cur_type = TY_INTEGER`。语义动作 a6 读取此值并赋给语义栈中所有变量的 `type` 字段。 |
| 144 | `int cur_offset; // 当前偏移` | 字段 `cur_offset`：当前活动记录偏移量，初始值为 0。语义动作 a6 中，按声明顺序从语义栈弹出变量并为每个变量分配 offset：`sym.offset = c->cur_offset; c->cur_offset += sym.len;`。这保证了变量在内存中按声明顺序紧密排列（无空隙，假设自然对齐）。 |
| 145 | （空行） | 空行，用于逻辑分隔。 |
| 146 | `// 四元式` | 注释：以下字段属于四元式生成模块（`quadruple.c`）使用的状态。 |
| 147 | `Quadruple quads[MAX_QUADS];` | 字段 `quads[]`：四元式数组，容量 `MAX_QUADS`(1024)。每次调用 `emit(op, arg1, arg2, result)` 时，将新的四元式追加到此数组末尾。`dump_quads()` 遍历输出。 |
| 148 | `int quad_count;` | 字段 `quad_count`：四元式数组中当前的条目数。`emit()` 使用 `c->quad_count++` 获取插入位置并递增计数。`dump_quads()` 遍历范围 0 到 `quad_count - 1`。初始为 0。 |
| 149 | `int temp_count;  // 临时变量计数器` | 字段 `temp_count`：临时变量计数器。由 `symbol.c` 的 `new_temp()` 函数使用：每次调用返回一个唯一的编号，并构造临时变量名 `_tN`（N 取自 `temp_count++`）。例如第一次调用返回 `_t0`，第二次返回 `_t1`。表达式处理过程中，每生成一个运算四元式就需要一个新的临时变量来存放结果。 |
| 150 | `int label_count; // 标号计数器` | 字段 `label_count`：标号计数器。由 `symbol.c` 的 `new_label()` 函数使用：每次调用返回唯一编号，构造标号名 `label_N`。用于 if/while 结构中标记跳转目标位置（`L_then`、`L_false`、`L_loop`、`L_exit` 等）。初始为 0。 |
| 151 | （空行） | 空行，用于逻辑分隔。 |
| 152 | `// 错误` | 注释：错误处理相关字段。 |
| 153 | `int error_flag;` | 字段 `error_flag`：错误标志。初始为 0。任何模块检测到错误（如词法非法字符、语法不匹配、语义类型不兼容）时设置 `c->error_flag = 1`。主控流程在关键步骤后检查此标志，若为 1 则跳过后续阶段或退出。 |
| 154 | `char error_msg[256];` | 字段 `error_msg`：错误消息字符串缓冲区。当检测到错误时，用 `sprintf`/`snprintf` 将错误描述写入此字段，然后由 `main.c` 输出给用户。大小 256 与 `MAX_LINE` 一致，足够容纳上下文相关的错误信息。 |
| 155 | `} Compiler;` | Compiler 结构体定义结束，typedef 名 `Compiler`。 |
| 156 | （空行） | 分隔符。 |
| 157 | `// ---- 以下数据表声明为 extern, 定义在各 .c 文件中 ----` | 注释：说明接下来的声明是 `extern` 变量——它们的实际定义（分配存储空间）位于各个 `.c` 源文件中，此处仅声明（告诉编译器"存在这样的变量"），以便多个 `.c` 文件共享同一份数据。 |
| 158 | （空行） | 分隔符。 |
| 159 | `// 关键字/界符统一表 (与文档中的 Keys 表一致)` | 注释：`keys_table[]` 是关键字字符串数组，按 Token 码顺序排列。词法分析器在识别出一个标识符后，遍历此表与 `keys_table[i]` 做 `strcmp`，若匹配则将 Token 码设为 `i + 3`（因为 `TOK_PROGRAM` 从 3 开始）。 |
| 160 | `extern const char *keys_table[];` | 声明外部数组 `keys_table`：字符串指针数组，每个元素是一个 C 字符串（关键字文本如 `"program"`、`"var"`、`"begin"` 等）。`const` 表示数组内容不可修改。`extern` 表示此数组的实际定义（包括初始化列表）在某个 `.c` 文件中——通常是 `scanner.c`。该文件中写 `const char *keys_table[] = {"program", "var", "integer", ...};`。多个源文件通过 `#include "grammar.h"` 获取此声明，链接时解析到同一个存储位置。 |
| 161 | （空行） | 分隔符。 |
| 162 | `// 四元式操作码名称 (用于输出)` | 注释：`op_names[]` 是四元式操作码的字符串名称数组，用于 `dump_quads()` 输出时将 `op` 数字转换为可读字符串。 |
| 163 | `extern const char *op_names[];` | 声明外部数组 `op_names`：字符串指针数组，按操作码值索引。例如 `op_names[OP_ADD]` 为 `"+"`，`op_names[OP_JNZ]` 为 `"jnz"`。数组长度至少为 21（`OP_WRITE`+1）。实际定义一般在 `quadruple.c` 中：`const char *op_names[] = {"", "program", ":=", "+", "-", "*", "/", "jmp", "jnz", "je", ...};`。索引 0 为空字符串 `""` 占位（因为第一个有效操作码 `OP_PROGRAM`=1）。 |
| 164 | （空行） | 分隔符。 |
| 165 | `// 类型宽度` | 注释：`type_len()` 函数返回给定类型的字节宽度。 |
| 166 | `static inline int type_len(int type) {` | 定义一个 `static inline` 函数 `type_len`。`static` 修饰表示此函数具有文件作用域（每个包含此头文件的 `.c` 文件会生成自己的一份副本，或编译器优化掉）。`inline` 关键字是 C99 标准引入的，提示编译器将此函数在调用处展开为内联代码，避免函数调用的开销。由于函数体仅一行且被频繁调用（变量声明的偏移计算、类型宽度查询），内联展开能显著提升编译器自身性能。返回值类型为 `int`，参数 `type` 为 `int`（接收 `TY_INTEGER`/`TY_REAL`/`TY_CHAR`）。 |
| 167 | `return (type == TY_INTEGER) ? 4 : (type == TY_REAL) ? 8 : 1;` | 函数体：嵌套的三元运算符（`?:`）。逻辑为：如果 `type == TY_INTEGER`(0) 返回 4；否则如果 `type == TY_REAL`(1) 返回 8；否则（必然是 `TY_CHAR`(2)）返回 1。这些宽度值对应：`int` 通常 4 字节（32 位系统），`double` 8 字节（IEEE 754 双精度），`char` 1 字节。此函数在语义动作 a6 中计算 `sym.len`，也在偏移递增 `cur_offset += type_len(cur_type)` 时调用。 |
| 168 | `}` | 函数体结束。 |
| 169 | （空行） | 分隔符。 |
| 170 | `// 常数自动机状态转换矩阵 (定义在 scanner.c)` | 注释：`const_aut[][]` 是 Pascal 常数识别 DFA 的状态转换矩阵，定义在 `scanner.c` 中。 |
| 171 | `extern const int const_aut[8][5];` | 声明外部二维数组 `const_aut`：8 行 5 列的整数矩阵。8 行表示 DFA 有 8 个状态（0~7），5 列表示字符类别数为 5（数字、小数点、E/e 指数标记、+/- 符号、其他）。矩阵元素值表示"当前状态遇到某类字符时应转移到哪个新状态"。`scanner.c` 中定义此数组（如 `const int const_aut[8][5] = { {1, 2, -1, -1, -1}, ... };`）。负数（如 -1）表示错误状态。`const` 防止意外修改。`extern` 说明实际定义在 `scanner.c`。括号内的 `8` 和 `5` 是数组维度，声明时给出维度有助于代码阅读，但不是必须的（C 语言中 extern 声明可省略第一维）。 |
| 172 | （空行） | 分隔符。 |
| 173 | `#endif // GRAMMAR_H` | 头文件保护宏的结束，与第 1 行的 `#ifndef GRAMMAR_H` 配对。`#endif` 之后的 `// GRAMMAR_H` 是注释，标注此 `#endif` 对应哪个 `#ifndef`，便于在嵌套宏中定位配对关系。至此头文件结束。 |

---

## 补充说明

### Token 编码体系

```
keys_table 索引:   0      1     2        3      4     5      6     ...
关键字:         program  var   integer  real   char  begin  end   ...
Token 码:        3      4      5        6      7     8      9     ...
(keys_table 索引 + 3)
```

扩展关键字从索引 18 开始：
```
keys_table 索引:  18   19   20   21   22   23   24   25   ... 32
关键字:          if   then else while do   and  or   not  ... write
Token 码:        21   22   23   24   25   26   27   28   ... 35
```

非关键字 Token（界符、比较运算符）直接按序编码为 10~20 和 29~34，不经过 `keys_table` 映射。

### 被调用的位置

| 符号 | 定义位置 | 主要使用位置 |
|------|---------|-------------|
| `keys_table[]` | `scanner.c` | `scanner.c`（词法分析器查关键字） |
| `op_names[]` | `quadruple.c` | `quadruple.c`（`dump_quads()` 输出四元式） |
| `const_aut[][]` | `scanner.c` | `scanner.c`（常数 DFA 状态转移） |
| `type_len()` | `grammar.h`（inline） | `parser.c`（变量声明偏移计算）、`symbol.c`（设置 `Symbol.len`） |

### extern 的意义

`extern` 声明告诉编译器：这个变量 / 数组在**另一个编译单元**（`.c` 文件）中定义（分配内存并初始化），当前文件只需引用它。最终由**链接器**将所有的引用解析到同一个物理内存地址。这是 C 语言中跨文件共享全局数据的标准方式。语法上 `extern` 声明不能同时带初始化器（那就会变成定义），例如在 `.c` 文件中定义时必须去掉 `extern` 并给出初始化列表。
