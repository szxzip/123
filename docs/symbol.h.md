# symbol.h 逐行讲解

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef SYMBOL_H` | 头文件守卫开始。检查宏 `SYMBOL_H` 是否未定义，防止同一个头文件被多次包含导致重复定义错误。 |
| 2 | `#define SYMBOL_H` | 定义宏 `SYMBOL_H`。下次再有 `#include "symbol.h"` 时，第 1 行的 `#ifndef` 就会为假，跳过整个文件。 |
| 3 | `#include "grammar.h"` | 包含 `grammar.h`，以获取本模块需要的核心类型定义：`Compiler` 结构体（含符号表、常数表、语义栈等所有字段）、`Symbol` 结构体、`Token` 枚举、`MAX_SYMBOLS` / `MAX_CONSTANTS` / `MAX_NAME` 等宏常量、`KIND_*` / `TY_*` 枚举值、以及内联函数 `type_len()`。 |
| 4 | （空行） | 分隔行，无实际作用。 |
| 5 | `// ========== 模块说明 ==========` | 注释分隔线，标示下方是该模块的功能说明文档。 |
| 6 | `// * symbol.c/h — 符号表与常数表管理` | 说明本模块的职责：管理符号表和常数表。符号表记录标识符（变量名、程序名、临时变量）的元信息；常数表记录编译期出现的数值常量。 |
| 7 | `// *   对应课程要求: 符号表系统（活动记录、主表、常数表）` | 指明本模块对应编译原理课程中"符号表系统"部分的教学要求，涵盖活动记录（栈帧偏移管理）、主符号表、常数表三大组件。 |
| 8 | `// *   功能:` | 引出功能列表的标题行。 |
| 9 | `// *     - 标识符查填 (enter_id / lookup_id)` | 功能一：标识符的"查填"——先查符号表是否已有该名字，有则返回索引（查），无则插入尾部并返回新索引（填）。由 `sym_enter_id()` 和 `sym_lookup_id()` 实现。 |
| 10 | `// *     - 常数查填 (enter_const / lookup_const)` | 功能二：常数的查填机制——类似标识符，但比较的是浮点数值而非字符串。由 `sym_enter_const()` 和 `sym_lookup_const()` 实现。 |
| 11 | `// *     - 临时变量分配 (new_temp)` | 功能三：为表达式求值的中间结果分配临时变量。每次调用生成一个名为 `t1`、`t2`、`t3`... 的新临时变量，并在符号表中登记。 |
| 12 | `// *     - 符号表打印` | 功能四：将符号表和常数表的内容格式化输出为可读字符串，供调试和控制台展示。 |
| 13 | `// * ================================` | 注释分隔线结束。 |
| 14 | （空行） | 分隔行。 |
| 15 | `void sym_init(Compiler *c);` | 函数声明：初始化符号表系统。将 `Compiler` 上下文中的 `sym_count`、`const_count`、`temp_count`、`label_count` 等计数器清零，并用 `memset` 清空符号表、常数表、语义栈数组。每次编译新源文件前调用一次。 |
| 16 | `int  sym_enter_id(Compiler *c, const char *name, int type, int kind, int offset);` | 函数声明：标识符查填。参数含义：`name` 为标识符字符串，`type` 为类型码（`TY_INTEGER`/`TY_REAL`/`TY_CHAR`），`kind` 为种类码（`KIND_PROGRAM`/`KIND_VARIABLE`/`KIND_TEMP`），`offset` 为活动记录偏移地址。返回该标识符在 `sym_table[]` 中的索引；表满则返回 `-1`。 |
| 17 | `int  sym_lookup_id(Compiler *c, const char *name);` | 函数声明：纯查询标识符。在符号表中线性搜索 `name`，找到返回索引，未找到返回 `-1`。不会插入新条目。用于语义分析阶段引用变量时查找其定义信息。 |
| 18 | `void sym_set_type(Compiler *c, int index, int type, int len, int offset);` | 函数声明：设置符号的类型信息。用于变量声明的语义动作（翻译文法中的 a6 动作）：在确定变量的类型和偏移后，批量更新符号表中对应条目的 `type`、`len`（类型宽度）、`offset` 字段。 |
| 19 | `int  sym_enter_const(Compiler *c, double val);` | 函数声明：常数查填。在 `const_table[]` 数组中搜索数值 `val`（用 `fabs` 容差比较），已有则返回索引，没有则追加。所有数值常量统一用 `double` 存储，即使是整数常量也提升为 `double`。表满返回 `-1`。 |
| 20 | `int  sym_lookup_const(Compiler *c, double val);` | 函数声明：纯查询常数。与 `sym_enter_const` 采用相同的浮点容差比较逻辑，但不会插入新条目。未找到返回 `-1`。 |
| 21 | `char *sym_new_temp(Compiler *c);` | 函数声明：分配新临时变量。内部使用 `static` 局部缓冲区构造名称（`t1`、`t2`...），递增 `temp_count`，并将该临时变量登记到符号表。返回指向内部静态缓冲区的指针。**注意**：由于使用 `static` 缓冲区，每次调用会覆盖上次返回的字符串。 |
| 22 | `int  sym_new_label(Compiler *c);` | 函数声明：分配新标号（label）。递增 `label_count` 并返回新值。标号用于四元式跳转指令（如 `OP_JMP`、`OP_JNZ`）的目标，编译期用递增整数唯一标识。 |
| 23 | `void sym_dump(Compiler *c, char *buf, int bufsize);` | 函数声明：格式化输出符号表到字符串缓冲区。输出包含表头（索引/名字/种类/类型/偏移/宽度）和每一行的符号条目信息。`bufsize` 是缓冲区大小，防止溢出。 |
| 24 | `void const_dump(Compiler *c, char *buf, int bufsize);` | 函数声明：格式化输出常数表到字符串缓冲区。输出包含表头（索引/值）和每一行的常数值（以 `%g` 格式化为最简形式）。 |
| 25 | （空行） | 分隔行。 |
| 26 | `#endif` | 头文件守卫结束，与第 1 行的 `#ifndef` 配对。 |
