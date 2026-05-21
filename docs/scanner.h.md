## 模块: scanner.h — 词法分析器接口

## 简明解释

- **词法分析是编译的第一阶段**，将字符流转换为词法单元流。扫描器模块采用 DFA（确定有限自动机）方法，系统化且高效。
- **`scanner_init()`**：将源程序字符串加载到 `Compiler` 上下文，重置读取位置，预读第一个字符。
- **`scanner_next_token()`**：读取一个词法单元。根据首字符分三路：字母→标识符/关键字（DFA 循环读取字母数字序列后查 `keys_table`），数字→常数（Pascal 常数自动机），其他→界符（含 `:=` `<=` `>=` `<>` 双字符向前看）。
- **`scanner_scan_all()`**：循环调用 `scanner_next_token()` 完成全源程序扫描，填充 `token_list[]`，作为词法分析阶段的完整输出。
- **`scanner_dump_tokens()`**：将词法单元序列格式化为 `(k,1)(i,N)`（标识符）、`(c,N)`（常数）、`(p,N)`（关键字/界符）的可读字符串，每 8 个词法单元换行。

---

# scanner.h 逐行讲解

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1 | `#ifndef SCANNER_H` | 头文件护卫宏的 `#ifndef` 检查，防止头文件被重复包含。`SCANNER_H` 是此头文件专用的宏名。 |
| 2 | `#define SCANNER_H` | 定义护卫宏 `SCANNER_H`。若此宏未定义则下定义并编译后续内容，若已定义（即头文件已被包含过）则跳过直到 `#endif`。 |
| 3 | `#include "grammar.h"` | 引入 `grammar.h` 头文件。scanner 模块依赖其中定义的 `Compiler` 结构体、`Token` 结构体、Token 类型码（`TOK_*`）、常量宏（`MAX_NAME` 等）以及 `keys_table[]`、`const_aut[][]` 的外部声明。 |
| 4 | *(空行)* | 分隔线，增强可读性。 |
| 5 | `// ========== 模块说明 ==========` | 模块说明区段标题，纯注释。 |
| 6 | `// * scanner.c/h — 词法分析器` | 说明此模块的职责：词法分析器（Lexical Analyzer / Scanner），即编译器的第一阶段。 |
| 7 | `// *   对应课程要求: 扫描器设计实现` | 注明此模块对应课程教学文档要求的"扫描器设计实现"任务。 |
| 8 | `// *   功能:` | 功能列表的引导注释。 |
| 9 | `// *     - InitScanner: 初始化, 加载源程序` | 功能一：`scanner_init()` —— 初始化扫描器，将源程序字符串加载到 `Compiler` 上下文中，设置读取位置指针为 0。 |
| 10 | `// *     - NextToken:   读取下一个 Token` | 功能二：`scanner_next_token()` —— 从源程序中读取下一个词法单元（Token），识别关键字、标识符、常数、界符。 |
| 11 | `// *     - ScanAll:     扫描全部 Token (词法分析阶段输出)` | 功能三：`scanner_scan_all()` —— 一次性扫描全部 Token 到 `token_list[]` 数组中，作为词法分析阶段的完整输出。 |
| 12 | `// *   实现技术:` | 实现技术说明的引导注释。 |
| 13 | `// *     - 关键字/界符: 查 keys_table` | 技术一：关键字（`program`, `var` 等）和界符（`,`, `:` 等）通过查表 (`keys_table[]`) 来识别，查到的索引值 +3 即为 Token 码。 |
| 14 | `// *     - 标识符:      DFA (字母开头, 后跟字母数字)` | 技术二：标识符的识别采用确定有限自动机（DFA），规则是必须字母开头、后跟零或多个字母或数字。 |
| 15 | `// *     - 常数:        Pascal 常数自动机 (整数、实数、科学计数法)` | 技术三：常数识别使用 Pascal 常数自动机（一个 8 状态的 DFA），可识别整数（`123`）、实数（`123.45`）、科学计数法（`1.23E+5`）。 |
| 16 | `// *     - 双界符:      :=  <=  >=  <>` | 技术四：双字符界符需要特殊处理（向前看一个字符），包括 `:=`（赋值）、`<=`（小于等于）、`>=`（大于等于）、`<>`（不等于）。 |
| 17 | `// * ================================` | 模块说明结束分隔线。 |
| 18 | *(空行)* | 分隔线。 |
| 19 | `void scanner_init(Compiler *c, const char *source);` | 函数声明：初始化扫描器。参数 `c` 是指向全局编译器上下文结构体的指针，`source` 是源程序字符串。此函数设置源字符串、长度、读取位置，并预读第一个字符到 `c->ch`。 |
| 20 | `void scanner_next_token(Compiler *c);` | 函数声明：读取下一个 Token。将当前字符位置起的一个词法单元识别结果写入 `c->token`（包括 `code`、`value`、`real_val`、`int_val`）。识别标识符时自动将其加入符号表，识别常数时自动将其加入常数表。 |
| 21 | `void scanner_scan_all(Compiler *c);` | 函数声明：扫描全部 Token。循环调用 `scanner_next_token()` 直到遇到 EOF 或出错，将所有 Token 存入 `c->token_list[]`，并记录 `c->token_count`。 |
| 22 | `void scanner_dump_tokens(Compiler *c, char *buf, int bufsize);` | 函数声明：将 Token 序列格式化为可读字符串输出到 `buf`，格式化规则为 `(k,1)(i,索引)` 表示标识符、`(c,索引)` 表示常数、`(p,码值)` 表示界符/关键字，每 8 个 Token 换行。 |
| 23 | *(空行)* | 分隔线。 |
| 24 | `#endif` | 与第 1 行的 `#ifndef` 配对，结束条件编译块。 |

---

## 架构概览

`scanner.h` 是词法分析模块的头文件，定义了四个公开函数：

| 函数 | 作用 | 调用时机 |
|------|------|---------|
| `scanner_init()` | 初始化扫描器，加载源程序 | 扫描开始前（由 `scanner_scan_all()` 或 `parser_parse()` 调用） |
| `scanner_next_token()` | 读取下一个 Token | 每次需要获取一个词法单元时调用 |
| `scanner_scan_all()` | 扫描全部 Token 到序列 | 两遍扫描的第一遍（词法分析阶段） |
| `scanner_dump_tokens()` | 格式化输出 Token 序列 | 词法分析阶段输出（调试/展示用） |

### 依赖关系

```
scanner.h
  └── grammar.h        ← Compiler 结构体定义、Token 码枚举、keys_table[] 外部声明
        ├── <ctype.h>   ← isspace, isalpha 等（非直接使用）
        ├── <stdio.h>   ← snprintf 等
        ├── <stdlib.h>  ← malloc 等
        └── <string.h>  ← strlen, strcmp 等
```
