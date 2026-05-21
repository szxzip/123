## 模块: parser.h — 语法分析器接口

## 简明解释

- **parser_init**: 重置预读（peek）机制，`peek_valid = 0`
- **parser_parse**: 编译主入口，采用**两遍扫描**架构：第一遍 `scanner_scan_all` 全量词法分析 → 保存 token_count → 第二遍重置扫描器后递归下降语法分析 → 生成四元式
- 注意：所有语义动作（四元式生成、符号表填写、临时变量分配）已移入 `semantic.c/h`，parser 只负责语法结构识别

---

# parser.h — 语法分析器接口

| 行号 | 代码 | 讲解 |
|------|------|------|
| 1-3 | `#ifndef` / `#define` / `#include` | 头文件保护，引入 grammar.h |
| 5-17 | 注释 | 分析策略：语句用递归下降子程序法（PROGRAM 等 10 个子程序），表达式用 7 级优先级递归下降 |
| 19 | `parser_init(c)` | 初始化解析器（重置 peek 机制） |
| 20 | `parser_parse(c)` | 执行编译：先 scan_all 做词法分析，再重置扫描器做递归下降语法分析。返回 1 成功 / 0 失败 |

> 注意：语义动作已移入 `semantic.c/h`，parser 通过 `semantic.h` 接口调用。
