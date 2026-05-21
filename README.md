# 编译原理课程设计 — 小型 Pascal 子集编译器

将 Pascal-like 源程序编译为目标代码（中间四元式 → x86-64 AT&T 汇编），
并支持 GTK3 图形界面交互。

## 工作流程

```
源程序(.txt) → 词法分析 → Token 序列
                         ↓
            符号表 ←── 语法分析（递归下降）+ 语义分析
                         ↓
                    中间代码（四元式）
                         ↓
                    优化（折叠/传播/死代码消除）
                         ↓
                   x86-64 AT&T 汇编 (.s)
                         ↓
                  gcc 汇编链接 → 可执行文件
```

## 模块说明

| 文件 | 环节 | 功能 |
|------|------|------|
| `grammar.h` | 全局定义 | Token 类型码、四元式操作码、符号表/常数表/语义栈结构体、Compiler 全局上下文 |
| `scanner.c` | 词法分析 | 关键字/界符统一查表、Pascal 常数自动机（8 状态 DFA 识别整数/实数/科学计数法）、双字符界符预读、输出 Token 序列 |
| `symbol.c` | 符号表 | 标识符/常数查填、临时变量 `t1,t2...` 分配、标号 `L1,L2...` 分配、a1~a6 翻译文法语义栈、按类型分配偏移与宽度 |
| `parser.c` | 语法+语义分析 | 递归下降子程序法（每个文法规则一个函数）、7 级优先级表达式分析（`or > and > not > 比较 > +- > */ > 因子`）、IF/WHILE 语句标号生成、a1~a6 翻译文法实现 |
| `quadruple.c` | 中间代码 | 四元式 `(op, arg1, arg2, result)` 数据结构，emit/dump/backpatch |
| `optimize.c` | 优化 | 常量折叠（编译时计算常数运算）、常量传播（变量替换为已知常数）、死代码消除（删除无用的临时变量赋值），多遍迭代至不动点 |
| `codegen.c` | 目标代码生成 | 四元式逐条翻译为 x86-64 AT&T 汇编，RIP 相对寻址，printf 调用，函数序言/尾声 |
| `main.c` | 入口 | CLI 模式（读取源文件 → 输出 Token/符号表/四元式/优化/汇编）、GTK3 模式（五标签页：Token/符号表/四元式/汇编/运行输出，读取文件按钮，GCC 编译运行按钮） |

## 编译命令

**Linux（GTK3 GUI 版）：**
```bash
gcc $(pkg-config --cflags gtk+-3.0) src/*.c -o compiler $(pkg-config --libs gtk+-3.0) -lm
```

**Linux（纯 CLI 版，无 GUI 依赖）：**
```bash
gcc src/*.c -o compiler -lm
```

**交叉编译 Windows .exe（需安装 mingw64-gcc 和 mingw64-gtk3）：**
```bash
x86_64-w64-mingw32-gcc $(x86_64-w64-mingw32-pkg-config --cflags gtk+-3.0) \
  src/*.c -o compiler.exe $(x86_64-w64-mingw32-pkg-config --libs gtk+-3.0) -lm
```

## 运行

```bash
./compiler                        # 启动 GTK3 图形界面
./compiler test/sample1.txt       # CLI 编译单个文件

# 汇编链接执行
gcc -no-pie test/sample1.txt.s -o s1 && ./s1
```
