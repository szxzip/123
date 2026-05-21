## 模块: quadruple.h — 四元式中间代码接口

## 简明解释

- `quad_init`（重置）— 重置四元式计数器、临时变量计数器、标号计数器，清零整个四元式数组
- `quad_emit`（追加一条四元式，返回索引）— 追加一条四元式 `(op, arg1, arg2, result)`，返回新生成四元式的序号（下标索引）
- `quad_dump`（格式化输出）— 格式化输出当前所有四元式为表格形式
- `quad_backpatch`（替换已有四元式的结果字段）— 将已生成四元式的 `result` 字段替换为新标号，实现回填技术

---

# quadruple.h 逐行讲解

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef QUADRUPLE_H` | 头文件保护宏的开始。检查 `QUADRUPLE_H` 是否未定义，防止头文件被重复包含。 |
| 2 | `#define QUADRUPLE_H` | 定义 `QUADRUPLE_H` 宏。如果第 1 行检查通过（首次包含），则定义此宏，下次再包含时第 1 行条件为假，跳过整个文件。 |
| 3 | `#include "grammar.h"` | 引入 `grammar.h`。因为本文件用到了 `Compiler` 类型（它是 `grammar.h` 中定义的结构体），以及 `MAX_QUADS`、`MAX_STR` 等宏，所以必须先包含。 |
| 4 | （空行） | 分隔符空白行，无功能。 |
| 5 | `// ========== 模块说明 ==========` | 注释分隔线，标记模块文档开始。 |
| 6 | `// * quadruple.c/h — 四元式序列管理` | 模块名称说明：本文件负责四元式（quadruple）中间代码的管理。 |
| 7 | `// *   对应课程要求: 中间代码设计 (四元式)` | 说明这是编译原理课程要求的"中间代码设计"模块，采用四元式格式。 |
| 8 | `// *   功能:` | 功能列表标题。 |
| 9 | `// *     - quad_emit:   生成一条四元式` | 功能一：`quad_emit` 函数，生成并追加一条新的四元式到队列中。 |
| 10 | `// *     - quad_dump:   输出四元式序列` | 功能二：`quad_dump` 函数，将当前所有四元式格式化输出为字符串。 |
| 11 | `// *     - quad_backpatch: 回填跳转目标` | 功能三：`quad_backpatch` 函数，修改某条已生成的四元式的目标地址（标号），即"回填"技术。 |
| 12 | `// * ================================` | 注释分隔线，标记模块文档结束。 |
| 13 | （空行） | 分隔符空白行。 |
| 14 | `void quad_init(Compiler *c);` | 声明 `quad_init` 函数。接收编译器上下文指针 `c`，无返回值。用于初始化四元式相关的计数器，将 `quad_count`、`temp_count`、`label_count` 归零，并清零整个四元式数组。 |
| 15 | `int quad_emit(Compiler *c, int op, const char *a1, const char *a2, const char *r);` | 声明 `quad_emit` 函数。接收编译器上下文 `c`、操作码 `op`、两个操作数 `a1` `a2`、结果 `r`。返回生成的四元式的序号（下标，从 0 开始）。如果四元式已满则返回 -1。这是中间代码生成的核心接口。 |
| 16 | `void quad_dump(Compiler *c, char *buf, int bufsize);` | 声明 `quad_dump` 函数。将当前所有四元式格式化输出到 `buf` 缓冲区中（最多 `bufsize` 字节）。无返回值。用于调试和输出中间代码。 |
| 17 | `void quad_backpatch(Compiler *c, int quad_idx, const char *label);` | 声明 `quad_backpatch` 函数。将第 `quad_idx` 条四元式的 `result` 字段替换为指定的标号 `label`。用于"回填"（backpatching）技术：生成跳转指令时目标尚未知，先生成占位四元式，待目标确定后再回填。 |
| 18 | （空行） | 分隔符空白行。 |
| 19 | `#endif` | 头文件保护宏的结尾。与第 1 行 `#ifndef` 配对，结束条件编译块。 |
